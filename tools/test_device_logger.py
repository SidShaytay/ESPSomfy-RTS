import tempfile
import os
import pty
import threading
import time
import unittest
from pathlib import Path
from device_logger import RestartTracker, safe_status, make_logger, usb_worker


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
