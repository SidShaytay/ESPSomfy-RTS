import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('radio_maintenance', ROOT / 'tools/radio_maintenance.py')
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)

FIELDS = [
    ('h','header','o',0),('S','shades','l',0),('G','groups','l',0),('sv','serverId','s',0),
    ('pr','proto','i',0),('d','direction','i',0),('w','tiltDirection','i',0),
    ('p','position','i',0),('t','target','i',0),('q','tiltPosition','i',0),('u','tiltTarget','i',0),
    ('se','section','s',0),('en','enabled','b',0),('ri','radioInit','b',0),('k','ok','b',0),
    ('fq','frequency','n',0),('ak','apiKey','s',0),('rb','rebooting','b',0)]

class FakeClient(guard.ProtoClient):
    instances = []
    wrong_server = False
    moving = False
    gpio = False
    bad_schema = False
    reject_save = False
    fail_reply = False
    changed_field = False
    already_disabled = False
    unexpected_event = False
    def __init__(self,*args,**kwargs):
        self.status=101;self._next_id=1;self.outgoing=[];self.did_close=False
        self.__class__.instances.append(self)
        self.enabled=not self.already_disabled
        self.incoming=[{'v':2,'t':1,'d':{'sh':'test'}}]
        types=[[49,'getSettings'],[50,'saveSettings']]
        if self.bad_schema: types[1][0]=16
        self.incoming += [{'v':2,'t':4,'d':{'T':types,'F':FIELDS,'E':[[1,'badRequest']]}}]
        shade={'p':30,'t':30,'pr':8 if self.gpio else 0,'d':1 if self.moving else 0}
        self.incoming += [{'v':2,'t':4,'d':{'h':{'sv':'other' if self.wrong_server else 'expected','pr':0},'S':[shade],'G':[{'pr':0}]}}]
        if self.unexpected_event:self.incoming += [{'v':2,'t':128,'d':{}}]
        self.incoming += [{'v':2,'t':5,'d':{}}]
    def send_text(self,text):
        frame=json.loads(text);self.outgoing.append(frame)
        if frame['t']==49:
            data={'k':1,'fq':433.42}
            if self.enabled:data.update(en=1,ri=1)
            elif self.changed_field:data['fq']=868.3
            self.incoming.append({'v':2,'t':2,'id':frame['id'],'d':data})
        if frame['t']==50:
            if not self.reject_save:self.enabled=False
            data={} if self.reject_save else {'k':1}
            if not self.fail_reply:self.incoming.append({'v':2,'t':2,'id':frame['id'],'d':data})
    def recv(self,timeout=10):
        if not self.incoming:raise TimeoutError('fake timeout')
        return self.incoming.pop(0)
    def close(self):self.did_close=True

class GuardTests(unittest.TestCase):
    def setUp(self):
        FakeClient.instances=[]
        for flag in ('wrong_server','moving','gpio','bad_schema','reject_save','fail_reply','changed_field','already_disabled','unexpected_event'):
            setattr(FakeClient,flag,False)
    def run_guard(self):return guard.disable_radio('192.0.2.1','expected',client_type=FakeClient)
    def test_exact_outbound_disable_and_readback(self):
        self.assertEqual(self.run_guard(),{'radio_disabled':True,'changed':True})
        client=FakeClient.instances[-1]
        self.assertEqual([f['t'] for f in client.outgoing],[3,3,49,50,49])
        self.assertEqual(client.outgoing[3]['d'],{'se':'radio','en':0})
        self.assertTrue(client.did_close)
    def test_already_disabled_is_read_only(self):
        FakeClient.already_disabled=True
        self.assertFalse(self.run_guard()['changed'])
        self.assertEqual([f['t'] for f in FakeClient.instances[-1].outgoing],[3,3,49,49])
    def test_fail_closed_before_save(self):
        for flag in ('wrong_server','moving','gpio','bad_schema','unexpected_event'):
            with self.subTest(flag=flag):
                self.setUp();setattr(FakeClient,flag,True)
                with self.assertRaises(guard.GuardError):self.run_guard()
                self.assertNotIn(50,[f['t'] for f in FakeClient.instances[-1].outgoing])
    def test_no_retry_after_failed_or_uncertain_save(self):
        for flag in ('reject_save','fail_reply','changed_field'):
            with self.subTest(flag=flag):
                self.setUp();setattr(FakeClient,flag,True)
                with self.assertRaises(guard.GuardError):self.run_guard()
                self.assertEqual([f['t'] for f in FakeClient.instances[-1].outgoing].count(50),1)
    def test_raw_encoder_refuses_every_other_operation(self):
        client=guard.guarded_client_type(FakeClient)('192.0.2.1')
        for operation in range(0,256):
            if operation in (3,49,50):continue
            with self.subTest(operation=operation),self.assertRaises(guard.GuardError):client.send(operation,{},True)
        for payload in ({'se':'radio','en':1},{'se':'mqtt','en':0},{'se':'radio','en':False},{'se':'radio','en':0,'fq':433.42}):
            with self.assertRaises(guard.GuardError):client.send(50,payload,True)
        with self.assertRaises(guard.GuardError):client.send_text('{"v":2,"t":50,"id":1,"d":{"se":"radio","en":0},"extra":1}')
        with self.assertRaises(guard.GuardError):client.send_text('{"v":2,"t":16,"t":50,"id":1,"d":{"se":"radio","en":0}}')
        self.assertEqual(client.outgoing,[])
    def test_transport_guard_never_retries_a_save(self):
        client=guard.guarded_client_type(FakeClient)('192.0.2.1')
        client.send(50,{'se':'radio','en':0},True)
        with self.assertRaises(guard.GuardError):client.send(50,{'se':'radio','en':0},True)
        self.assertEqual(len(client.outgoing),1)

if __name__=='__main__':unittest.main()
