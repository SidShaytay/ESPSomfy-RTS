import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('collector',Path(__file__).with_name('collect_stack_audit.py'))
collector=importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


class CollectorTests(unittest.TestCase):
    def test_allowlist_discards_raw_prefix_and_private_lines(self):
        lines=['WiFi password=secret-device-name\n',
          'I (100) StackAudit: STACK_AUDIT schema=1 role=loop uptimeMs=100 minimumUnusedBytes=2048 samples=1 maxScanUs=8\n',
          'STACK_AUDIT schema=1 role=private-device uptimeMs=100 minimumUnusedBytes=1 samples=1 maxScanUs=1\n',
          'STACK_AUDIT schema=1 role=web uptimeMs=100 minimumUnusedBytes=1 samples=1 maxScanUs=1 password=secret\n']
        result=collector.parse(lines)
        self.assertEqual(len(result['samples']),1)
        self.assertEqual(result['samples'][0]['minimumUnusedBytes'],2048)
        self.assertEqual(result['rejectedAuditLines'],2)
        self.assertNotIn('secret',str(result));self.assertNotIn('private-device',str(result))

    def test_sdk_roles_are_fixed_and_numeric(self):
        roles=('idle0','idle1','timer-daemon','system-events','tcpip','esp-timer')
        lines=[f'STACK_AUDIT schema=1 role={role} uptimeMs=100 minimumUnusedBytes=2048 samples=1 maxScanUs=8' for role in roles]
        result=collector.parse(lines)
        self.assertEqual(result['rolesObserved'],sorted(roles))

    def test_rejects_mixed_boots_and_empty_evidence(self):
        line='STACK_AUDIT schema=1 role=loop uptimeMs={} minimumUnusedBytes=20 samples={} maxScanUs=8\n'
        with self.assertRaises(ValueError):collector.parse([line.format(30000,7),line.format(100,1)])
        with self.assertRaises(ValueError):collector.parse(['no samples'])

    def test_rejects_reboot_between_roles_or_monotonic_samples(self):
        loop='STACK_AUDIT schema=1 role=loop uptimeMs=60000 minimumUnusedBytes=20 samples=10 maxScanUs=8'
        web='STACK_AUDIT schema=1 role=web uptimeMs=100 minimumUnusedBytes=20 samples=1 maxScanUs=8'
        later='STACK_AUDIT schema=1 role=loop uptimeMs=120000 minimumUnusedBytes=10 samples=20 maxScanUs=8'
        for marker in ['ESP-ROM:esp32s3-20210327', 'rst:0xc (RTC_SW_CPU_RST)', 'Rebooting...']:
            with self.assertRaises(ValueError):collector.parse([loop,marker,web])
            with self.assertRaises(ValueError):collector.parse([loop,marker,later])

    def test_records_only_captured_elf_and_rejects_identity_boundaries(self):
        identity='LCD running slot=app0 elfSHA256='+('a'*64)
        sample='STACK_AUDIT schema=1 role=loop uptimeMs=100 minimumUnusedBytes=20 samples=1 maxScanUs=8'
        self.assertEqual(collector.parse([identity,sample])['capturedElfSha256'],'a'*64)
        with self.assertRaises(ValueError):collector.parse([identity,sample,identity])
        reboot=['ESP-ROM:esp32s3-20210327','rst:0xc (RTC_SW_CPU_RST)']
        self.assertIsNone(collector.parse([identity]+reboot+[sample])['capturedElfSha256'])
        identity_b='LCD running slot=app1 elfSHA256='+('b'*64)
        self.assertEqual(collector.parse([identity]+reboot+[identity_b,sample])['capturedElfSha256'],'b'*64)

    def test_first_identity_can_follow_early_samples_in_same_observed_boot(self):
        sample='STACK_AUDIT schema=1 role=radio uptimeMs=20 minimumUnusedBytes=3000 samples=1 maxScanUs=8'
        identity='LCD running slot=app0 elfSHA256='+('a'*64)
        self.assertEqual(collector.parse(['ESP-ROM:esp32s3-20210327',sample,identity])['capturedElfSha256'],'a'*64)
        with self.assertRaises(ValueError):collector.parse([sample,identity])


if __name__=='__main__':unittest.main()
