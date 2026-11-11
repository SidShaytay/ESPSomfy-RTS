import contextlib
import runpy
import io
from pathlib import Path
import unittest
from unittest.mock import patch, Mock
import urllib.error

SCRIPT = Path(__file__).with_name('capture-local-ui.py')


class CaptureErrors(unittest.TestCase):
    def run_capture(self, failure):
        output = io.StringIO()
        opener = Mock()
        opener.open.side_effect = failure
        with patch('urllib.request.build_opener', return_value=opener), patch('sys.argv', [str(SCRIPT), '--host', '192.0.2.1']), contextlib.redirect_stderr(output):
            try:
                runpy.run_path(str(SCRIPT), run_name='__main__')
            except SystemExit as error:
                result = error.code
            else:
                self.fail('Capture did not report a failure')
        self.assertEqual(result, 1)
        self.assertNotIn('Traceback', output.getvalue())
        return output.getvalue()

    def http_error(self, status, body=b'', reason='Unknown code'):
        return urllib.error.HTTPError('http://192.0.2.1/lcd/screenshot', status, reason, {}, io.BytesIO(body))

    def test_protected_capture_explains_why_and_how_to_continue(self):
        message = self.run_capture(self.http_error(423, b'Protected LCD screen'))
        self.assertIn('protected', message)
        self.assertIn('Leave', message)

    def test_presentation_failure_is_distinct_from_memory_failure(self):
        message = self.run_capture(self.http_error(503, b'LCD frame not yet fully transferred'))
        self.assertIn('still drawing', message)
        message = self.run_capture(self.http_error(503, b'PSRAM screenshot allocation failed'))
        self.assertIn('memory', message)

    def test_busy_and_authentication_have_actionable_errors(self):
        self.assertIn('progress', self.run_capture(self.http_error(429)))
        self.assertIn('API key', self.run_capture(self.http_error(401)))
        self.assertIn('API key', self.run_capture(self.http_error(403)))

    def test_redirects_remain_refused_without_printing_location_or_body(self):
        message = self.run_capture(self.http_error(302, b'secret-key', 'Redirect refused for LCD capture'))
        self.assertIn('redirect', message)
        self.assertNotIn('secret-key', message)

    def test_unknown_errors_do_not_echo_untrusted_response_or_url(self):
        message = self.run_capture(self.http_error(500, b'secret-key \x1b[31m'))
        self.assertIn('HTTP 500', message)
        self.assertNotIn('secret-key', message)
        self.assertNotIn('192.0.2.1', message)

    def test_unreadable_service_error_body_still_reports_http_failure(self):
        error = self.http_error(503)
        error.read = Mock(side_effect=TimeoutError('private detail'))
        message = self.run_capture(error)
        self.assertIn('HTTP 503', message)
        self.assertNotIn('private detail', message)

    def test_transport_failure_does_not_print_private_exception_details(self):
        message = self.run_capture(urllib.error.URLError('secret-key'))
        self.assertIn('reach', message)
        self.assertNotIn('secret-key', message)


if __name__ == '__main__':
    unittest.main()
