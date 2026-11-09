"""ROM-free checks that pinned downloads retry transient failures only."""
import io
import socket
import unittest
import urllib.error
from unittest.mock import patch

import download_retry


def http_error(code):
    return urllib.error.HTTPError("https://example.invalid/x", code, "fixture", {}, None)


class DownloadRetry(unittest.TestCase):
    def fetch(self, outcomes):
        sleeps = []
        with patch.object(download_retry.urllib.request, "urlopen", side_effect=outcomes) as opened:
            try:
                return download_retry.fetch("https://example.invalid/x", 30, sleep=sleeps.append,
                                            log=lambda *a, **k: None), sleeps, opened.call_count
            except Exception as error:
                return error, sleeps, opened.call_count

    def test_gateway_timeouts_are_retried_until_the_body_arrives(self):
        body, sleeps, calls = self.fetch([http_error(504), http_error(502), io.BytesIO(b"pinned")])
        self.assertEqual((body, sleeps, calls), (b"pinned", [5, 15], 3))

    def test_connection_failures_and_timeouts_are_retried(self):
        outcomes = [urllib.error.URLError("unreachable"), socket.timeout(), ConnectionResetError(), io.BytesIO(b"ok")]
        body, sleeps, calls = self.fetch(outcomes)
        self.assertEqual((body, sleeps, calls), (b"ok", [5, 15, 45], 4))

    def test_persistent_failure_raises_after_the_last_attempt(self):
        error, sleeps, calls = self.fetch([http_error(503)] * 4)
        self.assertIsInstance(error, urllib.error.HTTPError)
        self.assertEqual((error.code, sleeps, calls), (503, [5, 15, 45], 4))

    def test_missing_asset_fails_at_once(self):
        error, sleeps, calls = self.fetch([http_error(404)])
        self.assertEqual((error.code, sleeps, calls), (404, [], 1))

    def test_transient_classification(self):
        self.assertTrue(download_retry.is_transient(http_error(429)))
        self.assertTrue(download_retry.is_transient(http_error(500)))
        self.assertFalse(download_retry.is_transient(http_error(403)))
        self.assertFalse(download_retry.is_transient(ValueError("checksum")))


if __name__ == "__main__":
    unittest.main()
