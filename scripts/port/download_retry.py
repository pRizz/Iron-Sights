"""Fetch checksum-pinned build inputs, retrying transient network failures.

GitHub release downloads intermittently answer 502/503/504 or time out on CI
runners. Callers still verify every byte against a pinned SHA256, so a retry
can only repeat the same request; it never widens what is accepted.
"""
import http.client
import socket
import time
import urllib.error
import urllib.request

# Seconds to wait before each retry; four attempts span about a minute.
RETRY_DELAYS = (5, 15, 45)


def is_transient(error):
    """True for failures a later identical request may not hit."""
    if isinstance(error, urllib.error.HTTPError):
        return error.code == 429 or 500 <= error.code < 600
    return isinstance(error, (urllib.error.URLError, socket.timeout, TimeoutError,
                              ConnectionError, http.client.IncompleteRead))


def fetch(url, timeout, delays=RETRY_DELAYS, sleep=time.sleep, log=print):
    """Return the body of url, retrying transient failures after each delay."""
    for attempt, delay in enumerate((*delays, None), start=1):
        try:
            with urllib.request.urlopen(url, timeout=timeout) as response:
                return response.read()
        except Exception as error:
            if delay is None or not is_transient(error):
                raise
            log(f"Download attempt {attempt} failed ({error}); retrying in {delay} s: {url}", flush=True)
            sleep(delay)
