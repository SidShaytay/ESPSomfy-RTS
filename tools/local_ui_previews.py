#!/usr/bin/env python3
"""Convert exact host-rendered LCD previews into PNGs without extra dependencies."""
from pathlib import Path
import sys
from lcd_image import png_bytes
p = Path(sys.argv[1])
images = {}
for file in p.glob('*.ppm'):
    head = file.read_bytes().split(b'\n', 3)
    if head[:3] != [b'P6', b'320 170', b'255']:
        raise ValueError('Unexpected renderer output')
    images[file.stem] = head[3]
    file.with_suffix('.png').write_bytes(png_bytes(320, 170, head[3]))
width, height = 984, 178
out = bytearray(bytes([17, 23, 25]) * (width*height))
for index, name in enumerate(['status', 'devices', 'control']):
    for y in range(170):
        offset = ((y+4)*width+4+index*328)*3
        out[offset:offset+960] = images[name][y*960:(y+1)*960]
(p/'overview.png').write_bytes(png_bytes(width, height, out))
