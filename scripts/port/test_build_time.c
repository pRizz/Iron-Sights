#include "port_build_time.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void zone(const char *name)
{
#ifdef _WIN32
    _putenv_s("TZ", name);
    _tzset();
#else
    setenv("TZ", name, 1);
    tzset();
#endif
}

static void expect(const char *stamp, const char *display)
{
    char actual[128];
    portFormatBuildTime(actual, sizeof(actual), stamp);
    if (strcmp(actual, display)) {
        fprintf(stderr, "%s -> %s (expected %s)\n", stamp, actual, display);
        abort();
    }
}

int main(void)
{
    /* Arrange/act/assert: standard and daylight offsets follow the build instant. */
    zone("EST5EDT");
    expect("2026-01-15T04:30:00Z", "Jan 14, 2026 11:30 PM UTC-05:00");
    expect("2026-07-15T04:30:00Z", "Jul 15, 2026 12:30 AM UTC-04:00");
    expect("2026-01-01T04:30:00Z", "Dec 31, 2025 11:30 PM UTC-05:00");
    expect("2025-01-01T04:30:00Z", "Dec 31, 2024 11:30 PM UTC-05:00");
    expect("1970-01-01T00:00:00Z", "Dec 31, 1969 07:00 PM UTC-05:00");
    /* Arrange/act/assert: non-hour offsets and date rollover survive conversion. */
    zone("NPT-5:45");
    expect("2026-10-28T23:30:00Z", "Oct 29, 2026 05:15 AM UTC+05:45");
    /* Arrange/act/assert: valid UTC/leap dates format and invalid input stays exact. */
    zone("UTC0");
    expect("2024-02-29T12:00:00Z", "Feb 29, 2024 12:00 PM UTC+00:00");
#ifdef _WIN32
    expect("2060-10-29T12:00:00Z", "Oct 29, 2060 12:00 PM UTC+00:00");
#endif
    expect("2026-02-29T12:00:00Z", "2026-02-29T12:00:00Z");
    expect("2026-13-01T12:00:00Z", "2026-13-01T12:00:00Z");
    expect("2026-10-29T24:00:00Z", "2026-10-29T24:00:00Z");
    expect("2026-10-29T12:00:60Z", "2026-10-29T12:00:60Z");
    expect("not a date", "not a date");
    expect(NULL, "Unavailable");
    expect("", "Unavailable");
    /* Arrange/act/assert: tiny/empty output never writes beyond its capacity. */
    char tiny[2] = {'x', 'x'};
    portFormatBuildTime(tiny, sizeof(tiny), "2026-10-29T12:00:00Z");
    assert(tiny[1] == '\0');
    portFormatBuildTime(tiny, 0, NULL);
    assert(tiny[1] == '\0');
    portFormatBuildTime(NULL, 0, NULL);
    puts("Local build-time display: DST, rollover, offsets, validation and bounds passed.");
    return 0;
}
