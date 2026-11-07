#!/usr/bin/env python3
"""Read-only framebuffer capture. Never flashes, wakes or commands a shutter."""
import argparse
import datetime
from pathlib import Path
import os
import urllib.request
import urllib.error
from lcd_image import lcd_bmp_rgb, png_bytes

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--host', required=True, help='Controller hostname or IPv4 address')
parser.add_argument('--apikey-file', type=Path, help='Existing API key file, if security is enabled')
parser.add_argument('--output', type=Path, help='Output .png or .bmp file (must not exist)')
args = parser.parse_args()
if any(c in args.host for c in '/?#@') or not args.host:
    parser.error('Pass only a controller hostname or IPv4 address')
if args.output and args.output.suffix.lower() not in ('.png', '.bmp'):
    parser.error('Output must be .png or .bmp')

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise urllib.error.HTTPError(req.full_url, code, 'Redirect refused for LCD capture', headers, fp)

headers = {}
if args.apikey_file:
    headers['apikey'] = args.apikey_file.read_text().strip()
request = urllib.request.Request('http://' + args.host + '/lcd/screenshot', headers=headers)
with urllib.request.build_opener(NoRedirect()).open(request, timeout=5) as response:
    data = response.read(108867)
    commands = response.headers.get('X-LCD-Commands')
    frame = response.headers.get('X-LCD-Frame')
    backlight = response.headers.get('X-LCD-Backlight')
if len(data) != 108866 or data[:2] != b'BM':
    raise SystemExit('Controller did not return the expected 320x170 BMP')
path = args.output or (Path.home()/'.local/state/espsomfy-lilygo/lcd-captures'/
    (datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.png'))
os.umask(0o077)
path.parent.mkdir(parents=True, exist_ok=True)
rgb = lcd_bmp_rgb(data)
with path.open('xb') as output:
    output.write(png_bytes(320, 170, rgb) if path.suffix.lower() == '.png' else data)
print(f'Saved {path} (frame {frame}, backlight PWM {backlight}, local commands {commands}).')
