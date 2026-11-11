#!/usr/bin/env python3
"""Drive an explicitly enabled USB audit image without modem-control changes.

Stop competing serial readers first. All raw output must stay in a private file.
No command is sent automatically on connection. Input injection expires after
2s; use release after each scripted UI sequence. This tests software UI paths,
not the physical encoder. Default and measurement-only images reject controls.
"""
import argparse
import fcntl
import os
from pathlib import Path
import re
import select
import threading
import time
import termios

COMMANDS = {'state', 'turn -1', 'turn 1', 'down', 'up', 'back-down',
            'back-up', 'release', 'wifi-reconnect', 'update-check',
            'update-stage', 'update-cancel'}
COMMAND_IDS = {'state': 1, 'turn -1': 2, 'turn 1': 2, 'down': 3, 'up': 4,
               'back-down': 5, 'back-up': 6, 'release': 7, 'wifi-reconnect': 8,
               'update-check': 9, 'update-stage': 10, 'update-cancel': 11}
ACK = re.compile(rb'AUDIT_ACK schema=1 id=(\d+) command=(\d+) result=(\d+) uptimeMs=(\d+)')


class UsbAuditClient:
    def __init__(self, port, capture, sequence=1):
        self.fd = None
        self.condition = threading.Condition()
        self.pending = None
        self.response = None
        self.stopped = threading.Event()
        self.error = None
        self.next_id = sequence
        self.log = None
        try:
            self.log = os.fdopen(os.open(capture, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb')
            self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            attrs = termios.tcgetattr(self.fd)
            attrs[0] = attrs[1] = attrs[3] = 0
            attrs[2] &= ~(termios.HUPCL | termios.CRTSCTS | termios.CSIZE | termios.PARENB)
            attrs[2] |= termios.CLOCAL | termios.CREAD | termios.CS8
            attrs[4] = attrs[5] = termios.B115200
            attrs[6][termios.VMIN] = attrs[6][termios.VTIME] = 0
            termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
            self.reader = threading.Thread(target=self._read, daemon=True)
            self.reader.start()
        except BaseException:
            if self.fd is not None:
                os.close(self.fd)
            if self.log:
                self.log.close()
            raise

    def _read(self):
        pending = b''
        try:
            while not self.stopped.is_set():
                if not select.select([self.fd], [], [], .2)[0]:
                    continue
                block = os.read(self.fd, 4096)
                if not block:
                    raise ConnectionError('USB disconnected; do not retry state-changing commands')
                self.log.write(block)
                self.log.flush()
                pending += block
                while b'\n' in pending:
                    line, pending = pending.split(b'\n', 1)
                    match = ACK.search(line)
                    if match:
                        with self.condition:
                            if self.pending and int(match[1]) == self.pending[0]:
                                value = tuple(map(int, match.groups()[1:]))
                                if value[0] != self.pending[1]:
                                    raise ConnectionError('Acknowledgement command differs from outstanding request')
                                self.response = value
                                self.condition.notify_all()
                if len(pending) > 16384:
                    pending = b''  # Bound malformed diagnostics; full bytes remain in private log.
        except BaseException as exc:
            with self.condition:
                self.error = exc
                self.condition.notify_all()

    def command(self, command, timeout=5):
        if command not in COMMANDS:
            raise ValueError('Unsupported audit command')
        identity = self.next_id
        if not 0 < identity < 2**32:
            raise ValueError('Sequence exhausted')
        self.next_id += 1
        payload = f'AUDIT {identity} {command}\n'.encode('ascii')
        # Register one outstanding request and write while holding the lock, so
        # ACKs observed before a request are discarded rather than cached.
        deadline = time.monotonic() + timeout
        with self.condition:
            if self.pending or self.error:
                raise ConnectionError('USB session is busy or failed')
            self.pending = (identity, COMMAND_IDS[command])
            self.response = None
            try:
                # Exactly one attempt; missing/partial ACKs are never replayed.
                if os.write(self.fd, payload) != len(payload):
                    raise ConnectionError('Partial command write; stop without replay')
                while self.response is None:
                    if self.error:
                        raise ConnectionError('USB capture failed') from self.error
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        self.error = TimeoutError('Missing acknowledgement; stop without replay')
                        raise self.error
                    self.condition.wait(remaining)
                value = self.response
            finally:
                self.pending = None
                self.response = None
        return dict(sequence=identity, command=value[0], result=value[1], uptimeMs=value[2])

    def close(self):
        self.stopped.set()
        self.reader.join(timeout=2)
        os.close(self.fd)
        self.log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--sequence', type=int, default=1)
    parser.add_argument('command', choices=sorted(COMMANDS))
    args = parser.parse_args()
    client = UsbAuditClient(args.port, args.capture, args.sequence)
    try:
        print(client.command(args.command))
    finally:
        client.close()


if __name__ == '__main__':
    main()
