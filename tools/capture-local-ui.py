#!/usr/bin/env python3
"""Read-only framebuffer capture. Never flashes, wakes or commands a shutter."""
import argparse
import datetime
from pathlib import Path
import os
import sys
import urllib.request
import urllib.error
from lcd_image import lcd_bmp_rgb, png_bytes

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise urllib.error.HTTPError(req.full_url, code, 'Redirect refused for LCD capture', headers, fp)

def http_failure(error):
    if error.code == 423:
        return 'Capture refused: this LCD screen is protected. Leave the protected screen and try again.'
    if error.code == 429:
        return 'Another LCD capture is in progress. Wait for it to finish and try again.'
    if error.code in (401, 403):
        return 'Capture authentication failed. Check the API key supplied with --apikey-file.'
    if 300 <= error.code < 400:
        return 'Capture redirect refused. Check that --host identifies the controller directly.'
    if error.code == 503:
        # Match only known controller reasons; never echo arbitrary response text,
        # redirect destinations or exception details that could contain secrets.
        try:
            reason = error.read(256).decode('utf-8', errors='replace').strip()
        except (OSError, ValueError):
            reason = ''
        if reason == 'LCD frame not yet fully transferred':
            return 'Capture temporarily unavailable: the LCD is still drawing. Wait a moment and try again.'
        if reason == 'No RGB framebuffer: simple status or disabled display':
            return 'Capture unavailable: the controller has no graphical LCD framebuffer.'
        if reason in ('PSRAM screenshot allocation failed', 'Screenshot response allocation failed', 'LCD response allocation failed'):
            return 'Capture unavailable: not enough memory for a screenshot. Try again after other work finishes.'
        return 'Controller temporarily unavailable (HTTP 503). Try again shortly.'
    return f'Capture failed (HTTP {error.code}).'


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True, help='Controller hostname or IPv4 address')
    parser.add_argument('--apikey-file', type=Path, help='Existing API key file, if security is enabled')
    parser.add_argument('--output', type=Path, help='Output .png or .bmp file (must not exist)')
    args = parser.parse_args(argv)
    if any(c in args.host for c in '/?#@') or not args.host:
        parser.error('Pass only a controller hostname or IPv4 address')
    if args.output and args.output.suffix.lower() not in ('.png', '.bmp'):
        parser.error('Output must be .png or .bmp')

    try:
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
            raise ValueError('Unexpected LCD bitmap')
        rgb = lcd_bmp_rgb(data)
        path = args.output or (Path.home()/'.local/state/espsomfy-lilygo/lcd-captures'/
            (datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.png'))
        os.umask(0o077)
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as output:
            output.write(png_bytes(320, 170, rgb) if path.suffix.lower() == '.png' else data)
    except urllib.error.HTTPError as error:
        with error:
            print(http_failure(error), file=sys.stderr)
        return 1
    except (urllib.error.URLError, TimeoutError):
        print('Could not reach the controller. Check --host and the network, then try again.', file=sys.stderr)
        return 1
    except OSError:
        print('Could not read the API key or write the capture. Check the file paths, permissions and available space.', file=sys.stderr)
        return 1
    except ValueError:
        print('Controller did not return the expected 320x170 BMP. Check the controller and capture settings.', file=sys.stderr)
        return 1
    print(f'Saved {path} (frame {frame}, backlight PWM {backlight}, local commands {commands}).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
