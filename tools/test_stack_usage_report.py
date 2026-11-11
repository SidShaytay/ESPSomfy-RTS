import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('stack_report', Path(__file__).with_name('stack_usage_report.py'))
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class StackReportTests(unittest.TestCase):
    def test_exports_only_repository_paths_and_largest_duplicate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'first.su').write_text('src/Test.h:1:2:void f()\t32\tstatic\n'
                '/private/host/cache/vendor.cpp:2:3:void vendor()\t8192\tstatic\n'
                'src/Test.cpp:4:5:void variable()\t96\tdynamic,bounded\n')
            (root / 'second.su').write_text('src/Test.h:1:2:void f()\t64\tstatic\n')
            result = report.summarize(root)
            self.assertEqual(result['uniqueRepositoryFunctions'], 2)
            self.assertEqual([r['frameBytes'] for r in result['largestFrames']], [96, 64])
            self.assertEqual(result['largestFrames'][0]['qualifier'], 'dynamic,bounded')
            self.assertNotIn('private', str(result))
            self.assertNotIn('vendor()', str(result))

    def test_binary_prologues_ignore_jump_table_data(self):
        text = """42000020 <real()>:
42000020: 004136 entry a1, 32
42000023: 000000 nop
42000026: 0fff36 entry a1, 32752
42000030 <other()>:
42000030: 00a136 entry a1, 0x50
"""
        result = report.binary_frames(text)
        self.assertEqual([r['frameBytes'] for r in result], [80, 32])


if __name__ == '__main__':
    unittest.main()
