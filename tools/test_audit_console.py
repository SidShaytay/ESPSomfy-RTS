import importlib.util
import os
from pathlib import Path
import pty
import select
import tempfile
import threading
import time
import unittest
spec=importlib.util.spec_from_file_location('audit_console',Path(__file__).with_name('audit_console.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class UsbTests(unittest.TestCase):
    def test_unsolicited_ack_is_discarded(self):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            capture=Path(directory)/'private.log'
            client=module.UsbAuditClient(os.ttyname(slave),capture)
            os.write(master,b'AUDIT_ACK schema=1 id=1 command=3 result=0 uptimeMs=10\n')
            deadline=time.monotonic()+1
            while b'uptimeMs=10' not in capture.read_bytes():
                self.assertLess(time.monotonic(),deadline);time.sleep(.01)
            def respond():
                self.assertEqual(os.read(master,256),b'AUDIT 1 state\n')
                os.write(master,b'AUDIT_ACK schema=1 id=1 command=1 result=0 uptimeMs=30\n')
            worker=threading.Thread(target=respond);worker.start()
            self.assertEqual(client.command('state')['uptimeMs'],30)
            worker.join();client.close()
        os.close(master);os.close(slave)

    def test_mismatched_command_ack_is_not_success(self):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            client=module.UsbAuditClient(os.ttyname(slave),Path(directory)/'private.log')
            def respond():
                os.read(master,256)
                os.write(master,b'AUDIT_ACK schema=1 id=1 command=1 result=0 uptimeMs=20\n')
            worker=threading.Thread(target=respond);worker.start()
            with self.assertRaises(ConnectionError):client.command('down',timeout=.5)
            worker.join();client.close()
        os.close(master);os.close(slave)

    def test_no_unsolicited_commands_and_one_attempt(self):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            capture=Path(directory)/'private.log'
            client=module.UsbAuditClient(os.ttyname(slave),capture)
            self.assertEqual(capture.stat().st_mode&0o777,0o600)
            self.assertFalse(select.select([master],[],[],.05)[0])
            def respond():
                request=os.read(master,256)
                self.assertEqual(request,b'AUDIT 1 state\n')
                os.write(master,b'private unrelated diagnostic\nAUDIT_ACK schema=1 id=1 command=1 result=0 uptimeMs=20\n')
            worker=threading.Thread(target=respond);worker.start()
            self.assertEqual(client.command('state')['uptimeMs'],20);worker.join()
            with self.assertRaises(ValueError):client.command('update-install')
            with self.assertRaises(TimeoutError):client.command('down',timeout=.05)
            self.assertEqual(os.read(master,256),b'AUDIT 2 down\n')
            self.assertFalse(select.select([master],[],[],.05)[0])
            client.close()
            self.assertIn(b'private unrelated',capture.read_bytes())
        os.close(master);os.close(slave)

if __name__=='__main__':unittest.main()
