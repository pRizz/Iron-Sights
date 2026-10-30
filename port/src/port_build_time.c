#include "port_build_time.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int localParts(struct tm *utc, struct tm *local, long *offset)
{
#ifdef _WIN32
    /* Explicit 64-bit CRT APIs also work after 2038 in the x86 player. */
    __time64_t instant = _mkgmtime64(utc);
    if (instant < 0 || _localtime64_s(local, &instant)) return 0;
#else
    time_t instant = timegm(utc);
    if (instant < 0 || !localtime_r(&instant, local)) return 0;
#endif
    /* Compare civil fields directly: a valid local date can fall before the
     * CRT's UTC epoch (e.g. Dec 31, 1969 for the first UTC instant of 1970). */
    int days = local->tm_yday - utc->tm_yday;
    if (local->tm_year != utc->tm_year) {
        int year = (local->tm_year < utc->tm_year ? local->tm_year : utc->tm_year) + 1900;
        int yearDays = 365 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        days += local->tm_year < utc->tm_year ? -yearDays : yearDays;
    }
    *offset = days * 86400L + (local->tm_hour - utc->tm_hour) * 3600L +
        (local->tm_min - utc->tm_min) * 60L + local->tm_sec - utc->tm_sec;
    return 1;
}

void portFormatBuildTime(char *output, size_t capacity, const char *maybeUtc)
{
    int year, month, day, hour, minute, second, consumed = 0;
    struct tm utc = {0}, local = {0};
    char canonical[32], readable[64];
    long offset;
    if (!output || !capacity) return;
    /* A failed conversion retains the exact, unambiguous source timestamp. */
    snprintf(output, capacity, "%s", maybeUtc && *maybeUtc ? maybeUtc : "Unavailable");
    if (!maybeUtc || strlen(maybeUtc) != 20 ||
        sscanf(maybeUtc, "%4d-%2d-%2dT%2d:%2d:%2dZ%n",
            &year, &month, &day, &hour, &minute, &second, &consumed) != 6 ||
        consumed != 20 || year < 1970 || second > 59) return;
    utc.tm_year = year - 1900;
    utc.tm_mon = month - 1;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = minute;
    utc.tm_sec = second;
    if (!localParts(&utc, &local, &offset)) return;
    /* Reject normalized invalid dates and noncanonical inputs. */
    if (!strftime(canonical, sizeof(canonical), "%Y-%m-%dT%H:%M:%SZ", &utc) ||
        strcmp(canonical, maybeUtc) ||
        !strftime(readable, sizeof(readable), "%b %d, %Y %I:%M %p", &local)) return;
    const long magnitude = offset < 0 ? -offset : offset;
    snprintf(output, capacity, "%s UTC%c%02ld:%02ld", readable,
        offset < 0 ? '-' : '+', magnitude / 3600, magnitude / 60 % 60);
}
