import tempfile
import os
import pty
import threading
import time
import unittest
import sys
from types import SimpleNamespace
from unittest.mock import Mock, patch
from pathlib import Path
from device_logger import RestartTracker, safe_status, make_logger, usb_worker, TelemetryWatchdog, StaleTelemetryError, telemetry_worker


class DeviceLogging(unittest.TestCase):
    def test_uptime_drop_records_restart_but_wrap_does_not(self):
        tracker = RestartTracker()
        self.assertFalse(tracker.update(10000))
        self.assertTrue(tracker.update(200))
        tracker.update(0xfffffff0)
        self.assertFalse(tracker.update(20))

    def test_network_secrets_and_model_data_are_excluded(self):
        self.assertEqual(safe_status({'uptime': 5, 'strength': -60,
                                     'ssid': 'private', 'password': 'secret',
                                     'shades': [1]}),
                         {'uptime': 5, 'strength': -60})

    def test_rotates_and_timestamps_persistent_records(self):
        with tempfile.TemporaryDirectory() as tmp:
            logger = make_logger(Path(tmp), 'test', max_bytes=200)
            for _ in range(20):
                logger.info('a diagnostic record')
            for handler in logger.handlers:
                handler.close()
            self.assertTrue((Path(tmp) / 'test.log.1').exists())
            self.assertRegex((Path(tmp) / 'test.log').read_text(), r'^\d{4}-\d\d-\d\dT')

    def test_usb_captures_output_without_sending_device_input(self):
        master, slave = pty.openpty()
        stop = threading.Event()
        try:
            with tempfile.TemporaryDirectory() as tmp:
                logger = make_logger(Path(tmp), 'usb-test')
                worker = threading.Thread(target=usb_worker,
                                          args=(os.ttyname(slave), logger, stop))
                worker.start()
                path = Path(tmp) / 'usb-test.log'
                deadline = time.monotonic() + 3
                while 'USB attached' not in path.read_text() and time.monotonic() < deadline:
                    time.sleep(.01)
                os.write(master, b'Task watchdog got triggered\n')
                while 'watchdog' not in path.read_text() and time.monotonic() < deadline:
                    time.sleep(.01)
                stop.set()
                worker.join(2)
                self.assertIn('Task watchdog got triggered', path.read_text())
                os.set_blocking(master, False)
                with self.assertRaises(BlockingIOError):
                    os.read(master, 1024)
        finally:
            stop.set()
            os.close(master)
            os.close(slave)

    def test_silent_open_connection_expires_after_three_missed_updates(self):
        watchdog = TelemetryWatchdog(now=0)
        watchdog.check(now=94)
        with self.assertRaises(StaleTelemetryError):
            watchdog.check(now=95)

    def test_wifi_events_do_not_hide_missing_memory_telemetry(self):
        watchdog = TelemetryWatchdog(now=0)
        watchdog.observe({'strength': -60}, now=90)
        with self.assertRaises(StaleTelemetryError):
            watchdog.check(now=95)

    def test_memory_telemetry_extends_freshness_window(self):
        watchdog = TelemetryWatchdog(now=0)
        watchdog.observe({'uptime': 123000, 'freeHeap': 153000}, now=90)
        watchdog.check(now=95)
        with self.assertRaises(StaleTelemetryError):
            watchdog.check(now=185)

    def test_worker_reconnects_silent_socket_and_logs_reason(self):
        stopped = False
        connections = []
        def connect(*args, **kwargs):
            nonlocal stopped
            client = SimpleNamespace(raw=[], sock=Mock(), close=Mock())
            connections.append(client)
            if len(connections) == 2:
                stopped = True
            return client
        stop = SimpleNamespace(is_set=lambda: stopped, wait=lambda _: None)
        protocol = SimpleNamespace(ProtoClient=connect, Schema=Mock(),
                                   run_sync=lambda *a, **k: (None, [], []))
        with tempfile.TemporaryDirectory() as tmp:
            logger = make_logger(Path(tmp), 'reconnect-test')
            with patch.dict(sys.modules, {'proto_client': protocol}), \
                 patch('device_logger.time.monotonic', side_effect=[0, 96, 192]):
                telemetry_worker('example.invalid', logger, stop)
            self.assertEqual(len(connections), 2)
            self.assertIn('Protocol stale:',
                          (Path(tmp) / 'reconnect-test.log').read_text())
            connections[0].sock.shutdown.assert_called_once()
            connections[0].close.assert_called_once()
