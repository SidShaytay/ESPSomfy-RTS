#!/usr/bin/env python3
"""One-way, no-RF maintenance admission using existing protocol 2.

Importing this module never connects. Only the explicit CLI --disable-radio
operation opens a socket. The root deployment operator must review and invoke
it; tests inject a fake client and cannot reach a controller.
"""
import argparse
import importlib.util
import ipaddress
import json
from pathlib import Path
import stat
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location('_maintenance_proto_client', ROOT / 'test/e2e/proto_client.py')
_proto = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_proto)
ProtoClient = _proto.ProtoClient
Schema = _proto.Schema

class GuardError(Exception):
    """Fixed public messages only; never include a token, frame, or settings."""

# These exact protocol keys and numbers are reviewed against ProtoSchema.h.
# A peer-supplied schema cannot remap a permitted settings operation to an RF op.
EXPECTED = {
    'header':('h','o',0),'shades':('S','l',0),'groups':('G','l',0),
    'serverId':('sv','s',0),'proto':('pr','i',0),'direction':('d','i',0),
    'tiltDirection':('w','i',0),'position':('p','i',0),'target':('t','i',0),
    'tiltPosition':('q','i',0),'tiltTarget':('u','i',0),
    'section':('se','s',0),'enabled':('en','b',0),'radioInit':('ri','b',0),'ok':('k','b',0),
}

def validate_outgoing(text):
    def unique(pairs):
        result = {}
        for key,value in pairs:
            if key in result:
                raise ValueError('duplicate key')
            result[key] = value
        return result
    try:
        frame = json.loads(text,object_pairs_hook=unique)
    except (ValueError, TypeError):
        raise GuardError('Outbound frame is not permitted') from None
    if not isinstance(frame,dict) or type(frame.get('v')) is not int or frame['v'] != 2 or type(frame.get('t')) is not int:
        raise GuardError('Outbound frame is not permitted')
    kind = frame['t']
    if kind == 3 and frame == {'v':2,'t':3}:
        return frame
    if set(frame) != {'v','t','id','d'} or type(frame['id']) is not int or frame['id'] <= 0:
        raise GuardError('Outbound frame is not permitted')
    if kind == 49 and frame['d'] == {'se':'radio'}:
        return frame
    data = frame['d']
    if kind == 50 and isinstance(data,dict) and set(data) == {'se','en'} and data['se'] == 'radio' and type(data['en']) is int and data['en'] == 0:
        return frame
    raise GuardError('Only sync credit, radio reads, and radio disable are permitted')

def guarded_client_type(base):
    class GuardedClient(base):
        def __init__(self,*args,**kwargs):
            self._disable_sent = False
            super().__init__(*args,**kwargs)
        def send_text(self,text):
            frame = validate_outgoing(text)
            if frame['t'] == 50:
                if self._disable_sent:
                    raise GuardError('Radio disable is never retried automatically')
                self._disable_sent = True
            return super().send_text(text)
    return GuardedClient

def _frame(client, deadline):
    remaining = deadline-time.monotonic()
    if remaining <= 0:
        raise GuardError('Protocol timed out; no automatic retry')
    try:
        frame = client.recv(timeout=remaining)
    except Exception:
        raise GuardError('Protocol interrupted; no automatic retry') from None
    if not isinstance(frame,dict) or frame.get('v') != 2 or type(frame.get('t')) is not int or not isinstance(frame.get('d'),dict):
        raise GuardError('Unexpected protocol response')
    return frame

def _sync(client, expected_server_id, deadline):
    first = _frame(client,deadline)
    if first['t'] != 1:
        raise GuardError('Protocol 2 hello was not received')
    schema = Schema()
    frames = []
    for _ in range(128):
        frame = _frame(client,deadline)
        if frame['t'] == 5:
            break
        if frame['t'] != 4:
            raise GuardError('Concurrent controller activity during preflight')
        try:
            schema.add(frame['d'])
        except Exception:
            raise GuardError('Unrecognized controller schema') from None
        frames.append(frame['d'])
        client.send(3)
    else:
        raise GuardError('Controller sync exceeds the bounded preflight')
    if not schema.complete() or schema.type_numbers.get('getSettings') != 49 or schema.type_numbers.get('saveSettings') != 50:
        raise GuardError('Settings operation schema differs from reviewed protocol')
    for name, (key,kind,default) in EXPECTED.items():
        if schema.keys.get(name) != key or schema.fields.get(key) != (name,kind,default):
            raise GuardError('Settings or motion schema differs from reviewed protocol')
    headers = [frame['h'] for frame in frames if 'h' in frame]
    if len(headers) != 1 or not isinstance(headers[0],dict) or headers[0].get('sv') != expected_server_id:
        raise GuardError('Controller identity does not match the expected server')
    if type(headers[0].get('pr',0)) is not int or headers[0].get('pr',0) != 0:
        raise GuardError('Maintenance requires an RTS-only controller model')
    shades,groups = [],[]
    for frame in frames:
        for key,destination in (('S',shades),('G',groups)):
            items = frame.get(key,[])
            if not isinstance(items,list):
                raise GuardError('Controller model is malformed')
            destination.extend(items)
    if not shades:
        raise GuardError('No configured shades were available to validate')
    for item in shades+groups:
        if not isinstance(item,dict) or type(item.get('pr',0)) is not int or item.get('pr',0) != 0:
            raise GuardError('Maintenance requires RTS-only shades and groups')
    for shade in shades:
        if any(type(shade.get(key,0)) is not int for key in ('d','w','p','t','q','u')):
            raise GuardError('Controller motion state is malformed')
        if shade.get('d',0) != 0 or shade.get('w',0) != 0 or shade.get('p',0) != shade.get('t',0) or shade.get('q',0) != shade.get('u',0):
            raise GuardError('A configured shade is not idle; no radio setting was changed')
    return schema

def _operation(client,kind,data,deadline):
    identifier = client.send(kind,data,with_id=True)
    frame = _frame(client,deadline)
    if frame['t'] != 2 or frame.get('id') != identifier:
        raise GuardError('Concurrent or unexpected activity; no further operations')
    if type(frame['d'].get('k')) is not int or frame['d']['k'] != 1:
        raise GuardError('Controller refused maintenance; no automatic retry')
    return frame['d']

def _enabled(settings):
    value = settings.get('en',0)
    if type(value) is not int or value not in (0,1):
        raise GuardError('Radio enabled state is malformed')
    return value == 1

def disable_radio(host,expected_server_id,token=None,client_type=ProtoClient,timeout=20):
    # The address/identity are supplied privately by the root operator, never
    # discovered by scanning, login attempts, or a mutable web endpoint.
    try:
        ipaddress.IPv4Address(host)
    except ValueError:
        raise GuardError('An explicit IPv4 controller address is required') from None
    if not isinstance(expected_server_id,str) or not expected_server_id or len(expected_server_id)>128:
        raise GuardError('An explicit expected server identity is required')
    if token is not None and (not isinstance(token,str) or not token or '\r' in token or '\n' in token):
        raise GuardError('Invalid private API token')
    client = None
    deadline = time.monotonic()+timeout
    try:
        client = guarded_client_type(client_type)(host,headers={'apikey':token} if token else None,timeout=timeout)
        if client.status != 101:
            raise GuardError('Protocol 2 upgrade was refused')
        _sync(client,expected_server_id,deadline)
        before = _operation(client,49,{'se':'radio'},deadline)
        changed = _enabled(before)
        if changed:
            _operation(client,50,{'se':'radio','en':0},deadline)
        after = _operation(client,49,{'se':'radio'},deadline)
        if _enabled(after) or after.get('ri',0) != 0:
            raise GuardError('Radio disable was not confirmed; do not provision')
        # radioInit is a derived status that intentionally becomes false; every
        # other returned radio value must remain byte-for-byte equivalent.
        without_status = lambda values:{key:value for key,value in values.items() if key not in ('en','ri')}
        if without_status(before) != without_status(after):
            raise GuardError('Another radio field changed; do not provision')
        return {'radio_disabled':True,'changed':changed}
    except GuardError:
        raise
    except Exception:
        raise GuardError('Maintenance failed or has an uncertain result; no automatic retry') from None
    finally:
        if client is not None:
            try:client.close()
            except Exception:pass

def main():
    parser = argparse.ArgumentParser(description='One-way radio disable; never sends RF commands or restores radio enable.')
    parser.add_argument('--host',required=True)
    parser.add_argument('--expected-server-id',required=True)
    parser.add_argument('--token-file',type=Path)
    parser.add_argument('--disable-radio',required=True,action='store_true')
    args = parser.parse_args()
    token = None
    try:
        if args.token_file:
            info = args.token_file.stat()
            if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077:
                raise GuardError('Token file must be a private regular file')
            token = args.token_file.read_text().strip()
        result = disable_radio(args.host,args.expected_server_id,token)
    except (GuardError,OSError,KeyboardInterrupt):
        print('Radio maintenance was not confirmed; do not provision or retry automatically.',file=sys.stderr)
        return 1
    print(json.dumps(result,sort_keys=True))
    return 0

if __name__=='__main__':
    sys.exit(main())
