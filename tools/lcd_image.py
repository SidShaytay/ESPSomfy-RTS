"""Small standard-library image encoder for the controller's fixed-size captures."""
import struct
import zlib


def png_bytes(width, height, rgb):
    if len(rgb) != width * height * 3:
        raise ValueError('Invalid RGB pixel count')

    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload +
                struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff))

    rows = b''.join(b'\0' + rgb[y*width*3:(y+1)*width*3] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def lcd_bmp_rgb(data):
    if (len(data) != 108866 or data[:2] != b'BM' or
            struct.unpack_from('<IiiHHI', data, 14) != (40, 320, -170, 1, 16, 3) or
            struct.unpack_from('<I', data, 10)[0] != 66 or
            struct.unpack_from('<III', data, 54) != (0xf800, 0x07e0, 0x001f)):
        raise ValueError('Not a controller 320x170 top-down RGB565 BMP')
    out = bytearray()
    for (color,) in struct.iter_unpack('<H', data[66:]):
        out.extend(((color >> 11)*255//31, ((color >> 5) & 63)*255//63, (color & 31)*255//31))
    return bytes(out)
