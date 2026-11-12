#!/usr/bin/env python3
"""Read-only USB/HTTP/protocol diagnostics; never sends motor operations.

Run with --host DEVICE_IP --port /dev/serial/by-id/DEVICE --logs PRIVATE_DIR.
Uses only Python's standard library and test/e2e/proto_client.py. When installing
outside the checkout, copy that client beside this script. UTC logs rotate at
8 MiB, retaining eight files per stream (128 MiB total). Telemetry is allowlisted;
USB output is raw device diagnostics and must stay private. USB capture requires
this host to remain running and the cable connected. No DTR/RTS ioctls or serial
writes are issued; HUPCL is cleared to avoid reset on close. Stop this collector
before flashing or using another serial monitor.
"""
import argparse
import fcntl
import json
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import select
import signal
import socket
import sys
import termios
import threading
import time
import urllib.request


class RestartTracker:
    def __init__(self):
        self.previous = None

    def update(self, uptime):
        restart = (self.previous is not None and uptime < self.previous
                   and not (self.previous > 0xf0000000 and uptime < 0x10000000))
        self.previous = uptime
        return restart


class StaleTelemetryError(ConnectionError):
    pass


class TelemetryWatchdog:
    """Expect periodic memory/uptime, even if the TCP socket stays open."""
    def __init__(self, now, timeout=95):
        self.last_memory = now
        self.timeout = timeout

    def observe(self, data, now):
        if type(data.get('uptime')) is int:
            self.last_memory = now

    def check(self, now):
        if now - self.last_memory >= self.timeout:
            raise StaleTelemetryError('No memory/uptime telemetry for 95 seconds')


def safe_status(data):
    allowed = ('uptime', 'freeHeap', 'minHeap', 'maxHeap', 'totalHeap',
               'loopRate', 'loopMaxUs', 'strength', 'channel', 'connected')
    return {k: v for k, v in data.items() if k in allowed and type(v) in (int, bool)}


def make_logger(directory, name, max_bytes=8 * 1024 * 1024):
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    logger.propagate = False
    handler = RotatingFileHandler(directory / (name + '.log'),
                                  maxBytes=max_bytes, backupCount=7)
    formatter = logging.Formatter('%(asctime)sZ %(message)s', '%Y-%m-%dT%H:%M:%S')
    formatter.converter = time.gmtime
    handler.setFormatter(formatter)
    logger.addHandler(handler)
    return logger


def usb_worker(port, logger, stop):
    while not stop.is_set():
        fd = None
        pending = b''
        try:
            fd = os.open(port, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            attrs = termios.tcgetattr(fd)
            attrs[0] = attrs[1] = attrs[3] = 0
            attrs[2] &= ~(termios.HUPCL | termios.CRTSCTS | termios.CSIZE | termios.PARENB)
            attrs[2] |= termios.CLOCAL | termios.CREAD | termios.CS8
            attrs[4] = attrs[5] = termios.B115200
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            logger.info('USB attached; no serial writes or modem-control ioctls')
            while not stop.is_set():
                if not select.select([fd], [], [], 1)[0]:
                    continue
                block = os.read(fd, 4096)
                if not block:
                    raise OSError('USB disconnected')
                pending += block
                while b'\n' in pending or len(pending) >= 4096:
                    if b'\n' in pending:
                        line, pending = pending.split(b'\n', 1)
                    else:
                        line, pending = pending[:4096], pending[4096:]
                    logger.info('USB %s', json.dumps(line.decode(errors='replace').rstrip('\r')))
        except (OSError, termios.error) as exc:
            logger.info('USB unavailable: %s', type(exc).__name__)
        finally:
            if pending:
                logger.info('USB partial %s', json.dumps(pending.decode(errors='replace')))
            if fd is not None:
                os.close(fd)
        stop.wait(5)


def http_worker(host, logger, stop):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    while not stop.is_set():
        start = time.monotonic()
        try:
            with opener.open('http://' + host + '/', timeout=3) as response:
                logger.info('HTTP status=%s latency_ms=%d', response.status,
                            (time.monotonic() - start) * 1000)
        except Exception as exc:
            logger.info('HTTP unavailable: %s', type(exc).__name__)
        stop.wait(15)


def telemetry_worker(host, logger, stop):
    # Use the project's existing protocol parser; install it beside this script.
    sys.path.append(str(Path(__file__).resolve().parents[1] / 'test' / 'e2e'))
    from proto_client import ProtoClient, Schema, run_sync
    tracker = RestartTracker()
    while not stop.is_set():
        client = None
        try:
            client = ProtoClient(host, timeout=5)
            schema = Schema()
            _, frames, _ = run_sync(client, schema, timeout=15)
            logger.info('Protocol connected')
            watchdog = TelemetryWatchdog(time.monotonic())
            client.raw.clear()
            for frame in frames:
                status = schema.expand(frame.get('d', {})).get('status', {})
                if status:
                    watchdog.observe(status, time.monotonic())
                    record_status(status, tracker, logger)
            while not stop.is_set():
                watchdog.check(time.monotonic())
                try:
                    frame = client.recv(timeout=2)
                except TimeoutError:
                    if client.closed.is_set():
                        raise ConnectionError("protocol disconnected")
                    continue
                client.raw.clear()
                if schema.types.get(frame.get('t')) in ('memory', 'wifi', 'ethernet'):
                    data = schema.expand(frame.get('d', {}))
                    watchdog.observe(data, time.monotonic())
                    record_status(data, tracker, logger)
        except StaleTelemetryError:
            logger.info('Protocol stale: no memory/uptime telemetry for 95s; reconnecting')
        except Exception as exc:
            logger.info('Protocol unavailable: %s', type(exc).__name__)
        finally:
            if client:
                # Wake a reader blocked in recv before closing a stale socket.
                try:
                    client.sock.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                client.close()
        stop.wait(10)


def record_status(data, tracker, logger):
    data = safe_status(data)
    if 'uptime' in data and tracker.update(data['uptime']):
        logger.info('RESTART detected: firmware uptime decreased')
    logger.info('Telemetry %s', json.dumps(data, sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--logs', type=Path, required=True)
    args = parser.parse_args()
    os.umask(0o077)
    stop = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: stop.set())
    signal.signal(signal.SIGINT, lambda *_: stop.set())
    events = make_logger(args.logs, 'events')
    usb = make_logger(args.logs, 'usb')
    events.info('Collector started; timestamps UTC; read-only monitoring')
    threads = [threading.Thread(target=target, args=values + (stop,))
               for target, values in (
                   (usb_worker, (args.port, usb)),
                   (http_worker, (args.host, events)),
                   (telemetry_worker, (args.host, events)))]
    for thread in threads:
        thread.start()
    try:
        while not stop.is_set() and all(thread.is_alive() for thread in threads):
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=20)
    if not all(thread.is_alive() for thread in threads):
        events.info('Collector stopped')


if __name__ == '__main__':
    main()
