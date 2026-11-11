import importlib.util
import os
from pathlib import Path
import pty
import select
import tempfile
import threading
import time
import unittest
from unittest import mock
spec=importlib.util.spec_from_file_location('audit_console',Path(__file__).with_name('audit_console.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class UsbTests(unittest.TestCase):
    def test_flush_helper_releases_ack_and_preserves_original_request(self):
        for flush in (False,True):
            with self.subTest(flush=flush):
                master,slave=pty.openpty()
                with tempfile.TemporaryDirectory() as directory:
                    client=module.UsbAuditClient(os.ttyname(slave),Path(directory)/'private.log')
                    received=[]
                    def respond():
                        data=b'';deadline=time.monotonic()+.25
                        while data.count(b'\n')<2 and time.monotonic()<deadline:
                            if select.select([master],[],[],.02)[0]:data+=os.read(master,256)
                        received.append(data)
                        if data==b'AUDIT 1 down\nAUDIT 2 state\n':
                            # The helper can be acknowledged first; only the
                            # original command's exact ACK is meaningful.
                            os.write(master,b'AUDIT_ACK schema=1 id=2 command=1 result=0 uptimeMs=21\n'
                                            b'AUDIT_ACK schema=1 id=1 command=3 result=0 uptimeMs=20\n')
                    worker=threading.Thread(target=respond);worker.start()
                    try:
                        if flush:
                            reply=client.command('down',timeout=.4,flush_ack=True)
                            self.assertEqual(reply,dict(sequence=1,command=3,result=0,uptimeMs=20,nextSequence=3))
                            self.assertEqual(client.next_id,3)
                        else:
                            with self.assertRaises(TimeoutError):client.command('down',timeout=.1)
                        worker.join(1);self.assertFalse(worker.is_alive())
                        self.assertEqual(received,[b'AUDIT 1 down\n'+(b'AUDIT 2 state\n' if flush else b'')])
                    finally:
                        worker.join(1);client.close()
                os.close(master);os.close(slave)

    def test_helper_ack_alone_does_not_acknowledge_original(self):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            client=module.UsbAuditClient(os.ttyname(slave),Path(directory)/'private.log')
            def respond():
                data=b''
                while data.count(b'\n')<2:data+=os.read(master,256)
                os.write(master,b'AUDIT_ACK schema=1 id=2 command=1 result=0 uptimeMs=21\n')
            worker=threading.Thread(target=respond);worker.start()
            try:
                with self.assertRaises(TimeoutError):client.command('down',timeout=.15,flush_ack=True)
                with self.assertRaises(ConnectionError):client.command('up')
                worker.join(1);self.assertFalse(worker.is_alive())
            finally:
                client.close()
        os.close(master);os.close(slave)

    def test_flush_helper_sequence_exhaustion_sends_nothing(self):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            client=module.UsbAuditClient(os.ttyname(slave),Path(directory)/'private.log',sequence=2**32-1)
            try:
                with self.assertRaises(ValueError):client.command('down',flush_ack=True)
                self.assertEqual(client.next_id,2**32-1)
                self.assertFalse(select.select([master],[],[],.02)[0])
            finally:client.close()
        os.close(master);os.close(slave)

    def test_flush_partial_write_is_one_attempt_and_fails_session(self):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            client=module.UsbAuditClient(os.ttyname(slave),Path(directory)/'private.log')
            try:
                with mock.patch.object(module.os,'write',return_value=5) as write:
                    with self.assertRaises(ConnectionError):client.command('down',flush_ack=True)
                    write.assert_called_once_with(client.fd,b'AUDIT 1 down\nAUDIT 2 state\n')
                    with self.assertRaises(ConnectionError):client.command('up',flush_ack=True)
                    self.assertEqual(write.call_count,1)
            finally:client.close()
        os.close(master);os.close(slave)

    def check_stale_read_is_not_acknowledged(self, partial):
        master,slave=pty.openpty()
        with tempfile.TemporaryDirectory() as directory:
            client=module.UsbAuditClient(os.ttyname(slave),Path(directory)/'private.log')
            reached=threading.Event();resume=threading.Event()
            original=client.log
            class DelayedCapture:
                def write(self,data):return original.write(data)
                def flush(self):
                    original.flush();reached.set()
                    if not resume.wait(2):raise TimeoutError('Test did not release capture writer')
                def close(self):original.close()
            client.log=DelayedCapture()
            prefix=b'AUDIT_ACK schema=1 id=1 command=3'
            os.write(master,prefix if partial else prefix+b' result=0 uptimeMs=10\n')
            outcome=[]
            def issue():
                try:outcome.append(client.command('down',timeout=.2))
                except Exception as error:outcome.append(error)
            try:
                self.assertTrue(reached.wait(1)) # Bytes were read before the command.
                worker=threading.Thread(target=issue);worker.start()
                self.assertTrue(select.select([master],[],[],1)[0])
                self.assertEqual(os.read(master,256),b'AUDIT 1 down\n')
                resume.set()
                if partial:os.write(master,b' result=0 uptimeMs=10\n')
                worker.join(1);self.assertFalse(worker.is_alive())
                self.assertEqual(len(outcome),1)
                self.assertIsInstance(outcome[0],TimeoutError)
                with self.assertRaises(ConnectionError):client.command('up')
                self.assertFalse(select.select([master],[],[],.02)[0])
            finally:
                resume.set();client.close()
        os.close(master);os.close(slave)

    def test_ack_read_before_command_is_discarded_despite_slow_capture(self):
        self.check_stale_read_is_not_acknowledged(partial=False)

    def test_ack_prefix_read_before_command_cannot_bind_to_later_suffix(self):
        self.check_stale_read_is_not_acknowledged(partial=True)

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
