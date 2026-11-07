import struct
import unittest
import zlib
from lcd_image import lcd_bmp_rgb, png_bytes


class LcdImages(unittest.TestCase):
    def bitmap(self):
        b = bytearray(108866)
        b[:2] = b'BM'
        struct.pack_into('<I', b, 10, 66)
        struct.pack_into('<IiiHHI', b, 14, 40, 320, -170, 1, 16, 3)
        struct.pack_into('<III', b, 54, 0xf800, 0x07e0, 0x001f)
        struct.pack_into('<HHH', b, 66, 0xf800, 0x07e0, 0x001f)
        return b

    def test_capture_colors_and_png(self):
        rgb = lcd_bmp_rgb(self.bitmap())
        self.assertEqual(rgb[:9], bytes([255, 0, 0, 0, 255, 0, 0, 0, 255]))
        png = png_bytes(320, 170, rgb)
        self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
        at = 8
        while at < len(png):
            size = struct.unpack_from('>I', png, at)[0]
            if png[at+4:at+8] == b'IDAT':
                self.assertEqual(zlib.decompress(png[at+8:at+8+size])[1:10], rgb[:9])
                break
            at += size+12
        else:
            self.fail('No PNG pixel data')

    def test_bad_layout_or_size_is_rejected(self):
        for offset in [10, 18, 22, 28, 30, 54]:
            b = self.bitmap()
            b[offset] ^= 1
            with self.assertRaises(ValueError):
                lcd_bmp_rgb(b)
        with self.assertRaises(ValueError):
            lcd_bmp_rgb(self.bitmap()[:-1])


if __name__ == '__main__':
    unittest.main()
