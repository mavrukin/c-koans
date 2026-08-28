/*
 * About Reading Input
 *
 * Writing output is easy because you control it. Reading input is hard
 * because you do not: it may be too long, malformed, or absent, and it
 * arrives one buffer at a time.
 *
 * The rule that survives contact with real input:
 *
 *     read a whole line, then parse the line.
 *
 * Reading and parsing in one step — which is what scanf("%d") does — leaves
 * the stream in a state that depends on what failed, and that is where the
 * infinite loops come from.
 *
 * These koans read from in-memory strings and temporary files so the input is
 * fixed. Everything shown applies to stdin unchanged.
 */
#include "koan.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * sscanf parses a string and returns how many items it *assigned* — not how
 * many it saw, and not how many bytes it consumed. That count is the only
 * evidence you have that the parse worked.
 */
KOAN(sscanf_returns_the_assignment_count)
{
    int day = 0, year = 0;
    char month[16] = {};

    KOAN_EQ_INT(/*__*/ 3, sscanf("25 Dec 2026", "%d %15s %d", &day, month, &year));
    KOAN_EQ_INT(/*__*/ 25, day);
    KOAN_EQ_STR(/*__STR*/ "Dec", month);
    KOAN_EQ_INT(/*__*/ 2026, year);

    /* It stops at the first mismatch and reports how far it got. The
     * remaining arguments are left untouched, not zeroed. */
    day = 0; year = -1;
    KOAN_EQ_INT(/*__*/ 1, sscanf("25 oops", "%d %d", &day, &year));
    KOAN_EQ_INT(/*__*/ 25, day);
    KOAN_EQ_INT(/*__*/ -1, year);

    /* Nothing matched at all: zero assignments. */
    KOAN_EQ_INT(/*__*/ 0, sscanf("oops", "%d", &day));

    /* And at end of input, EOF rather than zero — a different failure. */
    KOAN_EQ_INT(/*__*/ EOF, sscanf("", "%d", &day));
}

/*
 * %s is unbounded: it writes until whitespace, however long that takes. It is
 * the scanf equivalent of gets(), and it overflows exactly as readily. Always
 * give it a width, and note the width is the buffer size MINUS ONE.
 */
KOAN(scanf_s_must_always_have_a_width)
{
    char small[8];

    /* %7s writes at most 7 characters plus a NUL: exactly 8 bytes. */
    KOAN_EQ_INT(/*__*/ 1, sscanf("abcdefghijkl", "%7s", small));
    KOAN_EQ_STR(/*__STR*/ "abcdefg", small);
    KOAN_EQ_SZ(/*__SZ*/ 7, strlen(small));

    /* %s stops at whitespace, so it can never read a line containing spaces. */
    char word[16] = {};
    sscanf("two words", "%15s", word);
    KOAN_EQ_STR(/*__STR*/ "two", word);
}

/*
 * Whitespace in a format matches any amount of whitespace, including none.
 * Most conversions skip leading whitespace automatically — but %c and %[
 * do not, which is the source of the "it read a newline" bug.
 */
KOAN(whitespace_handling_is_not_uniform)
{
    int a = 0, b = 0;

    /* One space in the format matches many in the input. */
    KOAN_EQ_INT(/*__*/ 2, sscanf("  1     2  ", "%d %d", &a, &b));
    KOAN_EQ_INT(/*__*/ 2, b);

    /* %d skipped the leading spaces without being asked. */
    a = 0;
    KOAN_EQ_INT(/*__*/ 1, sscanf("   7", "%d", &a));
    KOAN_EQ_INT(/*__*/ 7, a);

    /* %c does not skip. Here it reads the space itself. */
    char c = 0;
    sscanf(" x", "%c", &c);
    KOAN_EQ_CHR(/*__CHR*/ ' ', c);

    /* A leading space in the format fixes it. */
    c = 0;
    sscanf(" x", " %c", &c);
    KOAN_EQ_CHR(/*__CHR*/ 'x', c);
}

/*
 * %[...] is a scanset: it matches a run of the listed characters, and unlike
 * %s it may include spaces. %[^\n] therefore reads "everything up to the
 * newline" — the closest scanf comes to reading a line.
 */
KOAN(scansets_match_character_classes)
{
    char field[32] = {};

    /* Read up to the comma. */
    KOAN_EQ_INT(/*__*/ 1, sscanf("hello, world", "%31[^,]", field));
    KOAN_EQ_STR(/*__STR*/ "hello", field);

    /* Only digits. */
    char digits[16] = {};
    sscanf("2026abc", "%15[0-9]", digits);
    KOAN_EQ_STR(/*__STR*/ "2026", digits);

    /* A scanset that matches nothing assigns nothing — and leaves the buffer
     * untouched, which is why you must check the return value. */
    char untouched[8] = "before";
    KOAN_EQ_INT(/*__*/ 0, sscanf("xyz", "%7[0-9]", untouched));
    KOAN_EQ_STR(/*__STR*/ "before", untouched);
}

/*
 * fgets reads a line into a fixed buffer, keeping the newline if it fits. It
 * returns null at end of input. It is the safe counterpart to gets(), which
 * was removed from the language in C11 for being unusable safely.
 */
static FILE *string_stream(const char *text)
{
    FILE *f = tmpfile();
    if (!f) KOAN_FAIL("could not create a temporary file");
    fputs(text, f);
    rewind(f);
    return f;
}

KOAN(fgets_keeps_the_newline)
{
    FILE *f = string_stream("first\nsecond\n");
    char  line[32];

    KOAN_TRUE(fgets(line, sizeof line, f) != nullptr);
    KOAN_EQ_STR(/*__STR*/ "first\n", line);
    KOAN_EQ_SZ(/*__SZ*/ 6, strlen(line));

    /* Trimming it is your job, and this is the idiom. */
    line[strcspn(line, "\n")] = '\0';
    KOAN_EQ_STR(/*__STR*/ "first", line);

    KOAN_TRUE(fgets(line, sizeof line, f) != nullptr);
    KOAN_EQ_STR(/*__STR*/ "second\n", line);

    /* Null means end of input, not an empty line. */
    KOAN_TRUE(fgets(line, sizeof line, f) == nullptr);

    fclose(f);
}

/*
 * A line longer than the buffer is delivered in pieces, with no newline on
 * the first piece. Detecting that is how you avoid silently splitting one
 * line into two records.
 */
KOAN(a_long_line_arrives_in_pieces)
{
    FILE *f = string_stream("abcdefghij\n");
    char  chunk[6];              /* room for 5 characters plus a NUL */

    KOAN_TRUE(fgets(chunk, sizeof chunk, f) != nullptr);
    KOAN_EQ_STR(/*__STR*/ "abcde", chunk);

    /* No newline in the buffer means the line was longer than it. */
    KOAN_TRUE(strchr(chunk, '\n') == nullptr);

    KOAN_TRUE(fgets(chunk, sizeof chunk, f) != nullptr);
    KOAN_EQ_STR(/*__STR*/ "fghij", chunk);

    /* The newline arrives on its own in the final piece. */
    KOAN_TRUE(fgets(chunk, sizeof chunk, f) != nullptr);
    KOAN_EQ_STR(/*__STR*/ "\n", chunk);

    fclose(f);
}

/*
 * Why scanf("%d") on a stream is a trap: on a mismatch it assigns nothing and
 * consumes nothing, so the offending text is still there. A loop that retries
 * without clearing the stream spins forever.
 */
KOAN(a_failed_scanf_does_not_consume_the_input)
{
    FILE *f = string_stream("oops 42\n");
    int   value = 0;

    /* First attempt fails, and the stream has not moved. */
    KOAN_EQ_INT(/*__*/ 0, fscanf(f, "%d", &value));
    KOAN_EQ_INT(/*__*/ 'o', fgetc(f));       /* still at the 'o' */
    ungetc('o', f);

    /* Retrying without discarding the bad input fails identically — this is
     * the infinite loop. */
    KOAN_EQ_INT(/*__*/ 0, fscanf(f, "%d", &value));

    /* The fix: discard the rest of the line, then continue. */
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') { }

    KOAN_EQ_INT(/*__*/ EOF, fscanf(f, "%d", &value));
    fclose(f);
}

/*
 * The robust pattern, and the one to reach for by default: fgets to get a
 * line, then strtol to parse it. Every failure is reported, the stream always
 * advances, and nothing can overflow.
 */
typedef enum { READ_OK, READ_EOF, READ_TOO_LONG, READ_NOT_A_NUMBER } ReadResult;

static ReadResult read_int_line(FILE *f, long *out)
{
    char line[32];

    if (!fgets(line, sizeof line, f)) return READ_EOF;

    /* No newline means the line did not fit; drain the rest so the next call
     * starts at a line boundary rather than mid-record. */
    if (!strchr(line, '\n')) {
        int c;
        while ((c = fgetc(f)) != EOF && c != '\n') { }
        return READ_TOO_LONG;
    }

    line[strcspn(line, "\n")] = '\0';

    errno = 0;
    char *end = nullptr;
    long  v = strtol(line, &end, 10);

    if (end == line || *end != '\0' || errno == ERANGE) return READ_NOT_A_NUMBER;

    *out = v;
    return READ_OK;
}

KOAN(fgets_then_parse_is_the_robust_pattern)
{
    FILE *f = string_stream("42\nnot a number\n-7\n");
    long  v = 0;

    KOAN_EQ_INT(/*__*/ READ_OK, read_int_line(f, &v));
    KOAN_EQ_INT(/*__*/ 42, (int)v);

    /* A bad line is reported and skipped — the stream still advances. */
    KOAN_EQ_INT(/*__*/ READ_NOT_A_NUMBER, read_int_line(f, &v));

    /* So the next line parses normally. This is what scanf could not do. */
    KOAN_EQ_INT(/*__*/ READ_OK, read_int_line(f, &v));
    KOAN_EQ_INT(/*__*/ -7, (int)v);

    KOAN_EQ_INT(/*__*/ READ_EOF, read_int_line(f, &v));
    fclose(f);
}

/*
 * Bringing it together.
 *
 * Read a small "key = value" configuration, tolerating blank lines, comments,
 * surrounding whitespace and over-long lines, and reporting the line number of
 * anything it rejects. This is what real input handling looks like: most of
 * the code is about the input being wrong.
 */
typedef struct {
    char key[24];
    long value;
} Setting;

typedef struct {
    size_t accepted;
    size_t rejected;
    int    first_bad_line;      /* 0 when everything parsed */
} ParseReport;

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static ParseReport read_config(FILE *f, Setting *out, size_t cap)
{
    ParseReport report = { 0 };
    char        line[64];
    int         lineno = 0;

    while (fgets(line, sizeof line, f)) {
        lineno++;

        if (!strchr(line, '\n') && !feof(f)) {      /* over-long: drain it */
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') { }
            report.rejected++;
            if (!report.first_bad_line) report.first_bad_line = lineno;
            continue;
        }
        line[strcspn(line, "\n")] = '\0';

        char *text = trim(line);
        if (*text == '\0' || *text == '#') continue;   /* blank or comment */

        char *eq = strchr(text, '=');
        if (!eq) {
            report.rejected++;
            if (!report.first_bad_line) report.first_bad_line = lineno;
            continue;
        }

        *eq = '\0';
        char *key = trim(text);
        char *val = trim(eq + 1);

        errno = 0;
        char *end = nullptr;
        long  v = strtol(val, &end, 10);

        if (*key == '\0' || strlen(key) >= sizeof out->key ||
            end == val || *end != '\0' || errno == ERANGE) {
            report.rejected++;
            if (!report.first_bad_line) report.first_bad_line = lineno;
            continue;
        }

        if (report.accepted < cap) {
            snprintf(out[report.accepted].key, sizeof out->key, "%s", key);
            out[report.accepted].value = v;
        }
        report.accepted++;
    }
    return report;
}

KOAN(assembling_a_configuration_reader)
{
    FILE *f = string_stream(
        "# a comment\n"
        "\n"
        "  timeout = 30  \n"
        "retries=5\n"
        "broken line\n"
        "empty =\n"
        "port = 8080\n");

    Setting     settings[8];
    ParseReport r = read_config(f, settings, 8);
    fclose(f);

    /* Three good settings; the comment and blank line are not errors. */
    KOAN_EQ_SZ(/*__SZ*/ 3, r.accepted);
    KOAN_EQ_SZ(/*__SZ*/ 2, r.rejected);

    /* "broken line" is on line 5 — comments and blanks still count as lines. */
    KOAN_EQ_INT(/*__*/ 5, r.first_bad_line);

    /* Whitespace around both key and value was trimmed. */
    KOAN_EQ_STR(/*__STR*/ "timeout", settings[0].key);
    KOAN_EQ_INT(/*__*/ 30, (int)settings[0].value);
    KOAN_EQ_STR(/*__STR*/ "retries", settings[1].key);
    KOAN_EQ_INT(/*__*/ 8080, (int)settings[2].value);

    /* A file of nothing but comments yields nothing, and no errors. */
    FILE *quiet = string_stream("# only\n# comments\n");
    ParseReport empty = read_config(quiet, settings, 8);
    fclose(quiet);
    KOAN_EQ_SZ(/*__SZ*/ 0, empty.accepted);
    KOAN_EQ_SZ(/*__SZ*/ 0, empty.rejected);
    KOAN_EQ_INT(/*__*/ 0, empty.first_bad_line);
}

KOAN_LESSON(lesson_about_reading_input, "About Reading Input",
    KOAN_CASE(sscanf_returns_the_assignment_count),
    KOAN_CASE(scanf_s_must_always_have_a_width),
    KOAN_CASE(whitespace_handling_is_not_uniform),
    KOAN_CASE(scansets_match_character_classes),
    KOAN_CASE(fgets_keeps_the_newline),
    KOAN_CASE(a_long_line_arrives_in_pieces),
    KOAN_CASE(a_failed_scanf_does_not_consume_the_input),
    KOAN_CASE(fgets_then_parse_is_the_robust_pattern),
    KOAN_CASE(assembling_a_configuration_reader)
);
