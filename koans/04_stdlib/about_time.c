/*
 * About Time
 *
 * <time.h> has three distinct notions, and mixing them up is the usual bug:
 *
 *   time_t          a point in time, as seconds since the epoch
 *   struct tm       that point broken into calendar fields
 *   clock_t         processor time used, for measuring, not for dates
 *
 * Only the middle one is human-readable, and converting to it costs a
 * timezone decision — which is why there are two functions to do it.
 *
 * These koans use fixed timestamps so they give the same answer everywhere.
 */
#include "koan.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * time_t counts seconds from the epoch, 1970-01-01 00:00:00 UTC. The standard
 * does not actually require that epoch, but every system you will meet uses it.
 */
KOAN(time_t_counts_seconds_from_an_epoch)
{
    time_t now = time(nullptr);

    /* Some point after 2020 and before 2100 — a sanity check, not a date. */
    KOAN_TRUE(now > 1577836800);        /* 2020-01-01 */
    KOAN_TRUE(now < 4102444800);        /* 2100-01-01 */

    /* difftime returns a double, because time_t's arithmetic is not
     * guaranteed to be plain subtraction. */
    time_t later = now + 60;
    KOAN_EQ_DBL(__DBL, difftime(later, now), 1e-9);
    KOAN_EQ_DBL(__DBL, difftime(now, later), 1e-9);
}

/*
 * gmtime breaks a time_t into UTC fields; localtime uses the local timezone.
 * The field encodings are famously irregular: years are since 1900, months
 * are zero-based, days of the month are not.
 */
KOAN(struct_tm_fields_have_odd_bases)
{
    time_t stamp = 1000000000;          /* 2001-09-09 01:46:40 UTC */

    /* gmtime returns a pointer to a shared static buffer. Copy it out
     * immediately — see the_shared_buffer_forms_clobber_each_other below. */
    struct tm utc = *gmtime(&stamp);

    /* tm_year is years since 1900, so 2001 is stored as 101. */
    KOAN_EQ_INT(__, utc.tm_year);
    KOAN_EQ_INT(__, utc.tm_year + 1900);

    /* tm_mon is 0-based: September is 8. */
    KOAN_EQ_INT(__, utc.tm_mon);

    /* tm_mday is 1-based, unlike tm_mon. There is no reason for this. */
    KOAN_EQ_INT(__, utc.tm_mday);

    KOAN_EQ_INT(__, utc.tm_hour);
    KOAN_EQ_INT(__, utc.tm_min);
    KOAN_EQ_INT(__, utc.tm_sec);

    /* tm_wday is 0 for Sunday. 2001-09-09 was a Sunday. */
    KOAN_EQ_INT(__, utc.tm_wday);

    /* tm_yday is 0-based day of the year. */
    KOAN_EQ_INT(__, utc.tm_yday);
}

/*
 * strftime formats a struct tm. It returns the number of characters written,
 * or zero if the buffer was too small — note that zero is also what an empty
 * format returns, so size the buffer generously.
 */
KOAN(strftime_formats_a_broken_down_time)
{
    time_t    stamp = 1000000000;
    struct tm utc = *gmtime(&stamp);

    char buf[64];

    KOAN_EQ_SZ(__SZ, strftime(buf, sizeof buf, "%Y-%m-%d", &utc));
    KOAN_EQ_STR(__STR, buf);

    strftime(buf, sizeof buf, "%H:%M:%S", &utc);
    KOAN_EQ_STR(__STR, buf);

    /* ISO 8601, which is the only format worth writing to a log. */
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &utc);
    KOAN_EQ_STR(__STR, buf);

    /* Too small a buffer writes nothing and returns zero. */
    char tiny[4];
    KOAN_EQ_SZ(__SZ, strftime(tiny, sizeof tiny, "%Y-%m-%d", &utc));
}

/*
 * mktime goes the other way: struct tm to time_t. It also *normalises* —
 * out-of-range fields are carried, which is the easiest way to do date
 * arithmetic in C.
 *
 * It interprets the fields as local time, so a round trip through gmtime and
 * mktime does not return where it started unless you are on UTC.
 */
KOAN(mktime_normalises_out_of_range_fields)
{
    /* The 32nd of January is the 1st of February. */
    struct tm t = {
        .tm_year = 2026 - 1900,
        .tm_mon  = 0,          /* January */
        .tm_mday = 32,
        .tm_hour = 12,
        .tm_isdst = -1,        /* let the library decide about DST */
    };

    time_t normalised = mktime(&t);
    KOAN_TRUE(normalised != (time_t)-1);

    /* mktime rewrote the struct in place. */
    KOAN_EQ_INT(__, t.tm_mon);       /* February */
    KOAN_EQ_INT(__, t.tm_mday);

    /* Which makes "90 days from now" a two-line calculation. */
    struct tm plus90 = {
        .tm_year = 2026 - 1900, .tm_mon = 0, .tm_mday = 1 + 90,
        .tm_hour = 12, .tm_isdst = -1,
    };
    mktime(&plus90);
    KOAN_EQ_INT(__, plus90.tm_mon);  /* April */
    KOAN_EQ_INT(__, plus90.tm_mday);
}

/*
 * gmtime, localtime, ctime and asctime all return a pointer to one shared
 * static buffer. Two calls in one expression clobber each other, and none of
 * them is thread-safe. Copy the result out the moment you get it.
 */
KOAN(the_shared_buffer_forms_clobber_each_other)
{
    time_t a = 1000000000;
    time_t b = 2000000000;

    struct tm *first = gmtime(&a);

    /* Reading a field out now is safe — it is an int, copied by value. */
    int year_before = first->tm_year;
    KOAN_EQ_INT(__, year_before);      /* 2001 */

    /* The second call writes into the same object the first returned. */
    struct tm *second = gmtime(&b);
    KOAN_TRUE(first == second);

    /* So the pointer you were holding now describes a different time. It was
     * never yours; you were reading the library's scratch space. */
    KOAN_EQ_INT(__, first->tm_year);   /* 2033, not 2001 */
    KOAN_EQ_INT(__, first->tm_year == second->tm_year);

    /* The portable fix is to copy the struct out before calling again.
     * (POSIX and C23 also offer gmtime_r, which writes where you tell it —
     * but it is hidden by some libcs under a strict -std=, so the copy is
     * what always works.) */
    struct tm ta = *gmtime(&a);
    struct tm tb = *gmtime(&b);
    KOAN_EQ_INT(__, ta.tm_year);       /* 2001 */
    KOAN_EQ_INT(__, tb.tm_year);       /* 2033 */
    KOAN_TRUE(ta.tm_year != tb.tm_year);
}

/*
 * clock() measures processor time, not elapsed time. Divide by
 * CLOCKS_PER_SEC to get seconds. It is the wrong tool for "how long did this
 * take on the wall clock" and the right one for "how much CPU did it burn".
 */
KOAN(clock_measures_processor_time)
{
    KOAN_TRUE(CLOCKS_PER_SEC > 0);

    clock_t start = clock();

    /* Do enough work to be measurable but not slow. `volatile` stops the
     * compiler deleting the loop, which it is otherwise entitled to do. */
    volatile long sink = 0;
    for (long i = 0; i < 2000000; i++) sink += i;

    clock_t end = clock();

    /* Use the result, so the loop cannot be optimised away on that ground
     * either: the sum of 0..1999999. */
    KOAN_EQ_INT(__, sink == 1999999L * 2000000L / 2);

    KOAN_TRUE(end >= start);
    double seconds = (double)(end - start) / CLOCKS_PER_SEC;
    KOAN_TRUE(seconds >= 0.0);
    KOAN_TRUE(seconds < 10.0);
}

/*
 * timespec_get is C11's portable high-resolution clock, giving whole seconds
 * plus nanoseconds. TIME_UTC is the one base every implementation supports.
 */
KOAN(timespec_get_has_nanosecond_fields)
{
    struct timespec ts;

    KOAN_EQ_INT(__, timespec_get(&ts, TIME_UTC));
    KOAN_TRUE(ts.tv_sec > 1577836800);

    /* Nanoseconds are always in range, whatever the real resolution is. */
    KOAN_TRUE(ts.tv_nsec >= 0 && ts.tv_nsec < 1000000000L);
}

/*
 * Bringing it together.
 *
 * A duration formatter and an elapsed-time helper. The formatter must handle
 * the carrying (90 seconds is 1m30s, not 90s) and the fact that a duration is
 * not a date — you cannot use strftime for it.
 */
static const char *format_duration(long seconds, char *buf, size_t cap)
{
    if (seconds < 0) { snprintf(buf, cap, "-"); return buf; }

    long days    = seconds / 86400;
    long hours   = (seconds % 86400) / 3600;
    long minutes = (seconds % 3600) / 60;
    long secs    = seconds % 60;

    if (days)         snprintf(buf, cap, "%ldd%ldh", days, hours);
    else if (hours)   snprintf(buf, cap, "%ldh%ldm", hours, minutes);
    else if (minutes) snprintf(buf, cap, "%ldm%lds", minutes, secs);
    else              snprintf(buf, cap, "%lds", secs);

    return buf;
}

KOAN(assembling_a_duration_formatter)
{
    char buf[32];

    KOAN_EQ_STR(__STR, format_duration(0, buf, sizeof buf));
    KOAN_EQ_STR(__STR, format_duration(45, buf, sizeof buf));

    /* 90 seconds carries into minutes. */
    KOAN_EQ_STR(__STR, format_duration(90, buf, sizeof buf));

    /* Exactly one hour: the minutes field is present and zero. */
    KOAN_EQ_STR(__STR, format_duration(3600, buf, sizeof buf));
    KOAN_EQ_STR(__STR, format_duration(9000, buf, sizeof buf));

    /* A day and a half. */
    KOAN_EQ_STR(__STR, format_duration(129600, buf, sizeof buf));

    /* The boundary that catches people: 59s stays seconds, 60s becomes
     * a minute with a zero seconds field. */
    KOAN_EQ_STR(__STR, format_duration(59, buf, sizeof buf));
    KOAN_EQ_STR(__STR, format_duration(60, buf, sizeof buf));

    /* And a real elapsed measurement, using difftime rather than subtraction. */
    time_t start = 1000000000;
    time_t end   = start + 3661;
    KOAN_EQ_STR(__STR,
                format_duration((long)difftime(end, start), buf, sizeof buf));
}

KOAN_LESSON(lesson_about_time, "About Time",
    KOAN_CASE(time_t_counts_seconds_from_an_epoch),
    KOAN_CASE(struct_tm_fields_have_odd_bases),
    KOAN_CASE(strftime_formats_a_broken_down_time),
    KOAN_CASE(mktime_normalises_out_of_range_fields),
    KOAN_CASE(the_shared_buffer_forms_clobber_each_other),
    KOAN_CASE(clock_measures_processor_time),
    KOAN_CASE(timespec_get_has_nanosecond_fields),
    KOAN_CASE(assembling_a_duration_formatter)
);
