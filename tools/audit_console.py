#!/usr/bin/env python3
"""Drive an explicitly enabled USB audit image without modem-control changes.

Stop competing serial readers first. All raw output must stay in a private file.
No command is sent automatically on connection. Input injection expires after
2s; use release after each scripted UI sequence. This tests software UI paths,
not the physical encoder. Default and measurement-only images reject controls.
Optional --flush-ack appends a read-only state helper without replaying the
requested command. Its result includes nextSequence for the next invocation.
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
        line_request = None
        discarding = False
        try:
            while not self.stopped.is_set():
                if not select.select([self.fd], [], [], .2)[0]:
                    continue
                # Associate bytes with the request at read time, before capture
                # I/O can block. command() uses this same lock around its write.
                with self.condition:
                    block = os.read(self.fd, 4096)
                    observed_request = self.pending
                if not block:
                    raise ConnectionError('USB disconnected; do not retry state-changing commands')
                self.log.write(block)
                self.log.flush()
                pieces = block.split(b'\n')
                for index, piece in enumerate(pieces):
                    if not pending and not discarding and piece:
                        line_request = observed_request
                    if not discarding:
                        pending += piece
                        if len(pending) > 16384:
                            pending = b''
                            discarding = True
                    if index == len(pieces) - 1:
                        continue
                    match = None if discarding else ACK.search(pending)
                    if match and line_request is not None:
                        with self.condition:
                            if self.pending == line_request and int(match[1]) == line_request[0]:
                                value = tuple(map(int, match.groups()[1:]))
                                if value[0] != line_request[1]:
                                    raise ConnectionError('Acknowledgement command differs from outstanding request')
                                self.response = value
                                self.condition.notify_all()
                    # A line retains its first byte's request across reads. A
                    # stale prefix cannot acquire a newly issued request token.
                    pending = b''
                    line_request = None
                    discarding = False
        except BaseException as exc:
            with self.condition:
                self.error = exc
                self.condition.notify_all()

    def command(self, command, timeout=5, flush_ack=False):
        if command not in COMMANDS:
            raise ValueError('Unsupported audit command')
        # Register one outstanding request and write while holding the lock, so
        # ACKs observed before a request are discarded rather than cached.
        deadline = time.monotonic() + timeout
        with self.condition:
            if self.pending or self.error:
                raise ConnectionError('USB session is busy or failed')
            identity = self.next_id
            extra = 1 if flush_ack else 0
            if not 0 < identity or identity + extra >= 2**32:
                raise ValueError('Sequence exhausted')
            self.next_id += 1 + extra
            payload = f'AUDIT {identity} {command}\n'.encode('ascii')
            if flush_ack:
                # A read-only helper encourages prompt diagnostic output. It
                # shares this one write, consumes a sequence, and is not awaited.
                payload += f'AUDIT {identity + 1} state\n'.encode('ascii')
            next_sequence = self.next_id
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
            except BaseException as exc:
                if self.error is None:
                    self.error = exc
                raise
            finally:
                self.pending = None
                self.response = None
        reply = dict(sequence=identity, command=value[0], result=value[1], uptimeMs=value[2])
        if flush_ack:
            reply['nextSequence'] = next_sequence
        return reply

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
    parser.add_argument('--flush-ack', action='store_true',
                        help='Append a read-only state helper in the same write; consumes an extra sequence')
    parser.add_argument('command', choices=sorted(COMMANDS))
    args = parser.parse_args()
    client = UsbAuditClient(args.port, args.capture, args.sequence)
    try:
        print(client.command(args.command, flush_ack=args.flush_ack))
    finally:
        client.close()


if __name__ == '__main__':
    main()
