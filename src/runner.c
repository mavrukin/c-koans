/*
 * runner.c — walks the lessons in curriculum order and stops at the first
 * koan that still needs your attention.
 *
 * The runner deliberately shows you exactly one problem at a time. That is
 * the whole idea: fix the koan in front of you, run again, move forward.
 */
#include "koan.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- Pull in the curriculum ------------------------------------------- */

#define KOAN_GROUP(title)
#define KOAN_ENTRY(sym) extern const koan_lesson lesson_##sym;
#include "manifest.def"
#undef KOAN_GROUP
#undef KOAN_ENTRY

typedef struct {
    const char        *group;   /* non-null for a tier header  */
    const koan_lesson *lesson;  /* non-null for a real lesson  */
    const char        *sym;
} entry;

static const entry table[] = {
#define KOAN_GROUP(title) { title, nullptr, nullptr },
#define KOAN_ENTRY(sym)   { nullptr, &lesson_##sym, #sym },
#include "manifest.def"
#undef KOAN_GROUP
#undef KOAN_ENTRY
};

static const size_t table_len = sizeof table / sizeof *table;

/* ---- Presentation ------------------------------------------------------ */

static bool use_color = true;

#define C(code) (use_color ? (code) : "")
#define RED     C("\033[31m")
#define GREEN   C("\033[32m")
#define YELLOW  C("\033[33m")
#define BLUE    C("\033[34m")
#define CYAN    C("\033[36m")
#define BOLD    C("\033[1m")
#define DIM     C("\033[2m")
#define RESET   C("\033[0m")

static const char *const zen[] = {
    "The obstacle is the path.",
    "A pointer that points nowhere still has a type.",
    "Undefined behaviour is not a bug you can see. It is a bug you cannot see.",
    "The compiler is not your adversary. It is your first reviewer.",
    "Memory you did not free is memory you did not understand.",
    "Every array decays. Know what it decays into.",
    "To read C is to ask, always: who owns this?",
    "The standard is shorter than you fear and stranger than you hope.",
};

static const char *zen_line(size_t n)
{
    return zen[n % (sizeof zen / sizeof *zen)];
}

/* ---- Crash handling ---------------------------------------------------- */

static const char *crashing_lesson = "(startup)";
static const char *crashing_case   = "(startup)";

static void on_crash(int sig)
{
    const char *name = sig == SIGSEGV ? "SIGSEGV (invalid memory access)"
                     : sig == SIGBUS  ? "SIGBUS (misaligned or bad address)"
                     : sig == SIGFPE  ? "SIGFPE (arithmetic error)"
                     : sig == SIGABRT ? "SIGABRT (abort called)"
                                      : "a fatal signal";

    /* write() is async-signal-safe; printf is not. Keep this minimal. */
    char buf[512];
    int  n = snprintf(buf, sizeof buf,
                      "\n  The koan crashed: %s\n"
                      "  lesson: %s\n"
                      "  koan:   %s\n\n"
                      "  This is usually a null or dangling pointer, or an\n"
                      "  index past the end of an array. Re-read the koan and\n"
                      "  ask who owns the memory you are touching.\n\n",
                      name, crashing_lesson, crashing_case);
    if (n > 0) {
        ssize_t ignored = write(STDERR_FILENO, buf, (size_t)n);
        (void)ignored;
    }
    _exit(3);
}

static void install_crash_handlers(void)
{
    struct sigaction sa = { 0 };
    sa.sa_handler = on_crash;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_RESETHAND;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
}

/* ---- Reporting --------------------------------------------------------- */

static void print_progress(size_t done, size_t total)
{
    const size_t width = 30;
    size_t filled = total ? (done * width) / total : 0;

    printf("  %s[", DIM);
    for (size_t i = 0; i < width; i++)
        fputs(i < filled ? "#" : ".", stdout);
    printf("]%s  %s%zu/%zu%s koans\n", RESET, BOLD, done, total, RESET);
}

static void report_failure(const koan_lesson *lesson,
                           const char        *case_name,
                           size_t             passed,
                           size_t             total)
{
    const koan_result *r = &koan__current;

    const char *headline =
        r->status == KOAN_UNFILLED  ? "This koan has a blank in it."
      : r->status == KOAN_NEEDS_WORK ? "This koan is waiting for you."
                                     : "This koan is not yet true.";

    printf("\n%s%s  %s%s\n\n", BOLD, YELLOW, headline, RESET);
    printf("  %slesson%s  %s\n", DIM, RESET, lesson->title);
    printf("  %skoan%s    %s\n", DIM, RESET, case_name);
    printf("  %sfile%s    %s%s:%d%s\n",
           DIM, RESET, CYAN, r->file, r->line, RESET);

    if (r->expr && r->expr[0])
        printf("  %sassert%s  %s\n", DIM, RESET, r->expr);

    if (r->expected[0] || r->actual[0]) {
        printf("\n  %sexpected%s  %s%s%s\n",
               DIM, RESET, GREEN, r->expected, RESET);
        printf("  %sactual%s    %s%s%s\n",
               DIM, RESET, RED, r->actual, RESET);
    }

    if (r->note[0])
        printf("\n  %s%s%s\n", YELLOW, r->note, RESET);

    printf("\n");
    print_progress(passed, total);
    printf("\n  %s%s%s\n\n", DIM, zen_line(passed), RESET);
    printf("  %sEdit %s and run %smake%s again.%s\n\n",
           DIM, r->file, BOLD, RESET, RESET);
}

static void report_completion(size_t total)
{
    printf("\n%s%s  All %zu koans are true.%s\n\n", BOLD, GREEN, total, RESET);
    printf("  You have walked the whole path: from the shape of an int to a\n"
           "  web server you wrote yourself. The language is no longer a\n"
           "  mystery to you; it is a tool you can account for, line by line.\n\n");
    printf("  %sNow go read the standard library headers. They are C too.%s\n\n",
           DIM, RESET);
}

/* ---- Counting ---------------------------------------------------------- */

static size_t count_all_cases(void)
{
    size_t n = 0;
    for (size_t i = 0; i < table_len; i++)
        if (table[i].lesson)
            n += table[i].lesson->ncases;
    return n;
}

/*
 * Run one koan, returning whether it passed.
 *
 * The setjmp lives here rather than in the caller, and that placement is
 * load-bearing. A local modified between setjmp and longjmp has an
 * indeterminate value after the jump unless it is volatile — the rule
 * about_error_handling teaches. Keeping the jump inside a function that holds
 * no state of its own means the runner's counters, flags and loop indices
 * never cross it, so none of them need to be volatile.
 */
static bool run_case(const koan_case *kase)
{
    if (setjmp(koan__escape) == 0) {
        kase->fn();
        return true;
    }
    return false;
}

static void list_lessons(void)
{
    printf("\n  %sThe path%s\n\n", BOLD, RESET);
    for (size_t i = 0; i < table_len; i++) {
        if (table[i].group) {
            printf("\n  %s%s%s\n", BOLD, table[i].group, RESET);
        } else {
            printf("    %-28s %s%zu koans%s  %s%s%s\n",
                   table[i].sym,
                   DIM, table[i].lesson->ncases, RESET,
                   DIM, table[i].lesson->path, RESET);
        }
    }
    printf("\n  Run one lesson with:  %smake KOAN=<name>%s\n\n", BOLD, RESET);
}

static void usage(const char *argv0)
{
    printf("usage: %s [options]\n\n"
           "  --list            show every lesson in curriculum order\n"
           "  --only <name>     run a single lesson\n"
           "  --from <name>     start at a lesson and continue\n"
           "  --all             do not stop at the first failure\n"
           "  --no-color        plain output\n"
           "  --help            this message\n\n",
           argv0);
}

/* ---- Main -------------------------------------------------------------- */

int main(int argc, char **argv)
{
    const char *only = nullptr;
    const char *from = nullptr;
    bool        keep_going = false;

    if (!isatty(STDOUT_FILENO) || getenv("NO_COLOR"))
        use_color = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            list_lessons();
            return 0;
        } else if (strcmp(argv[i], "--only") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else if (strcmp(argv[i], "--from") == 0 && i + 1 < argc) {
            from = argv[++i];
        } else if (strcmp(argv[i], "--all") == 0) {
            keep_going = true;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            use_color = false;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    install_crash_handlers();

    const size_t total  = count_all_cases();
    size_t       passed = 0;
    size_t       failed = 0;
    bool         started = (from == nullptr);
    const char  *shown_group = nullptr;

    printf("\n  %sC Koans%s %s— C23, the standard library, POSIX, and a server "
           "at the end.%s\n",
           BOLD, RESET, DIM, RESET);

    for (size_t i = 0; i < table_len; i++) {
        if (table[i].group) {
            shown_group = table[i].group;
            continue;
        }

        const koan_lesson *lesson = table[i].lesson;

        if (only && strcmp(table[i].sym, only) != 0)
            continue;
        if (!started) {
            if (strcmp(table[i].sym, from) == 0) started = true;
            else { passed += lesson->ncases; continue; }
        }

        if (shown_group) {
            printf("\n  %s%s%s\n", BOLD, shown_group, RESET);
            shown_group = nullptr;
        }

        printf("  %s%s%s\n", CYAN, lesson->title, RESET);
        crashing_lesson = lesson->title;

        for (size_t c = 0; c < lesson->ncases; c++) {
            crashing_case = lesson->cases[c].name;

            if (run_case(&lesson->cases[c])) {
                passed++;
                printf("    %s+%s %s\n", GREEN, RESET, lesson->cases[c].name);
            } else {
                failed++;
                printf("    %s-%s %s\n", RED, RESET, lesson->cases[c].name);
                report_failure(lesson, lesson->cases[c].name, passed, total);
                if (!keep_going)
                    return 1;
            }
        }
    }

    if (failed > 0) {
        printf("\n  %s%zu koans still need you.%s\n\n", YELLOW, failed, RESET);
        print_progress(passed, total);
        printf("\n");
        return 1;
    }

    if (only || from) {
        printf("\n  %sThat stretch of the path is clear.%s\n\n", GREEN, RESET);
        return 0;
    }

    report_completion(total);
    return 0;
}
