/*
 * About Compiling
 *
 * You have run `make` several times now. This lesson is about what it did.
 *
 * A C file becomes a running program in four steps. Knowing which step you are
 * in explains almost every confusing error message you will ever get.
 *
 *   1. preprocess   handle the lines starting with #   -> one big text file
 *   2. compile      turn that text into instructions   -> assembly
 *   3. assemble     turn those into machine code       -> an object file (.o)
 *   4. link         join all the .o files together     -> a program
 *
 * Steps 1 to 3 happen once per .c file, each on its own, knowing nothing about
 * the others. Step 4 happens once, over all of them.
 *
 * Try it yourself. These four commands do one step each:
 *
 *     cc -std=c23 -E  hello.c -o hello.i    # stop after preprocessing
 *     cc -std=c23 -S  hello.c -o hello.s    # stop after compiling
 *     cc -std=c23 -c  hello.c -o hello.o    # stop after assembling
 *     cc              hello.o -o hello      # link
 *
 * docs/COMPILING.md walks through all of it with real output, and ends by
 * explaining the Makefile you have been using.
 */
#include "koan.h"
#include "build_demo.h"

/*
 * Step 1 is the preprocessor, and it is pure text substitution. It runs to
 * completion before the compiler sees anything.
 *
 * `#define` gives a name to some text. Everywhere that name appears afterwards,
 * the preprocessor replaces it with the text — a find-and-replace, nothing
 * more. By step 2 the name is gone.
 */
#define KOAN_GREETING "built from source"
#define KOAN_DOUBLE_OF_SIX 12

KOAN(the_preprocessor_replaces_text)
{
    KOAN_EQ_STR(__STR, KOAN_GREETING);
    KOAN_EQ_INT(__, KOAN_DOUBLE_OF_SIX);
}

/*
 * The preprocessor also fills in facts about the file it is reading.
 *
 * __LINE__ becomes the line number it appears on. __func__ becomes the name of
 * the koan it sits inside. These are how every failure message in this course
 * knows where to point you.
 */
KOAN(the_preprocessor_knows_where_it_is)
{
    /*
     * Two lines, one after the other. The second __LINE__ is one greater than
     * the first, so subtracting them gives the distance between them.
     */
    int line_here = __LINE__;
    int line_next = __LINE__;
    KOAN_EQ_INT(__, line_next - line_here);

    /* The name of the koan you are in, as text. */
    KOAN_EQ_STR(__STR, __func__);
}

/*
 * `#include` is the other half of step 1, and it is equally literal: the
 * preprocessor replaces the line with the entire contents of the named file.
 *
 * That is why a header is described as being *pasted in* rather than imported.
 * This file includes build_demo.h at the top, so everything build_demo.h
 * declares is available from that point on — including its macro.
 */
KOAN(a_header_is_pasted_in)
{
    KOAN_EQ_INT(__, BUILD_DEMO_MACRO_ANSWER);
}

/*
 * Because a header is pasted in, including it twice would paste it twice — and
 * declaring the same thing twice is an error. Every header therefore guards
 * itself:
 *
 *     #ifndef BUILD_DEMO_H      // if this name is not defined...
 *     #define BUILD_DEMO_H      // ...define it, and continue
 *     ...
 *     #endif
 *
 * The second time through, the name is already defined, so everything between
 * is skipped. Open build_demo.h and you will see exactly that.
 */
KOAN(headers_guard_themselves)
{
    /* Including it again does nothing at all, because of that guard. */
#include "build_demo.h"
    KOAN_EQ_INT(__, BUILD_DEMO_MACRO_ANSWER);
}

/*
 * Steps 2 and 3 turn one .c file into one .o file, in isolation.
 *
 * The compiler reading *this* file has never seen build_demo.c. It trusts the
 * declaration it got from the header, checks that the call matches, and emits
 * a call to a name it cannot yet resolve.
 *
 * The linker resolves that name in step 4. This is why the two most common
 * errors in C are different errors at different stages:
 *
 *   "undeclared identifier"   -> step 2. You forgot the #include.
 *   "undefined symbol"        -> step 4. The .o holding the body was not
 *                                given to the linker.
 *
 * Same missing function, different fix, depending on which step complained.
 */
KOAN(the_linker_joins_separate_files)
{
    /* Declared in build_demo.h, defined in build_demo.c, joined at step 4. */
    KOAN_EQ_INT(__, build_demo_add(3, 4));
    KOAN_EQ_INT(__, build_demo_answer());

    /* The macro from the header and the function from the other file, used
     * together — one came from step 1, the other from step 4. */
    KOAN_EQ_INT(__, build_demo_add(BUILD_DEMO_MACRO_ANSWER, 58));
}

/*
 * Some questions are answered while the program is being built, not while it
 * is running.
 *
 * `static_assert` states something that must be true at compile time. If it is
 * false there is no program at all — the build stops. It costs nothing when
 * the program runs, because by then it has already been checked.
 */
static_assert(BUILD_DEMO_MACRO_ANSWER == 42);

KOAN(some_checks_happen_before_the_program_runs)
{
    /* That this koan runs at all proves the assertion above held. */
    static_assert(1 + 1 == 2);
    KOAN_TRUE(__BOOL);
}

/*
 * The preprocessor can also *remove* code. `#if`, `#else` and `#endif` choose
 * which lines the compiler is allowed to see; the rest are deleted before
 * step 2 and never compiled at all.
 *
 * This is how one file supports several systems: the branch for the machine
 * you are not on does not have to work, because it is never compiled.
 */
KOAN(unselected_branches_are_never_compiled)
{
    int chosen;

#if BUILD_DEMO_MACRO_ANSWER == 42
    chosen = 1;
#else
    chosen = 2;      /* deleted before the compiler sees it */
#endif

    KOAN_EQ_INT(__, chosen);
}

/*
 * `-std=c23` is the flag that tells the compiler which version of C to accept.
 * The compiler reports which one it is using through __STDC_VERSION__, a
 * number that reads as a date: C11 is 201112, C17 is 201710, C23 is 202311.
 *
 * These koans require C23. If this one fails, your compiler is too old and
 * docs/TROUBLESHOOTING.md tells you what to install.
 */
KOAN(the_standard_version_is_a_compile_time_fact)
{
    KOAN_TRUE(__STDC_VERSION__ >= __);

    /* Which is newer than C17. */
    KOAN_TRUE(__STDC_VERSION__ > 201710L);
}

/*
 * Bringing it together.
 *
 * Predict what a build produces, from the rules above. There are three files:
 *
 *     build_demo.h        declarations only
 *     build_demo.c        includes the header  -> build_demo.o
 *     about_compiling.c   includes the header  -> about_compiling.o
 *
 * Work each answer out from the four steps rather than guessing. The reasoning
 * matters more than the numbers, because it is what makes build errors
 * readable for the rest of your life.
 */
KOAN(a_mental_model_of_the_build)
{
    /*
     * This file's preprocessor pasted in the header and expanded the macro.
     */
    KOAN_EQ_INT(__, BUILD_DEMO_MACRO_ANSWER);

    /*
     * build_demo.c included the same header, and its preprocessor expanded the
     * same macro independently. This function returns what *that* file saw.
     */
    KOAN_EQ_INT(__, build_demo_answer());

    /*
     * They agree — which is the evidence that the header was compiled twice,
     * once into each object file. Neither file could see the other; both saw
     * the header.
     */
    KOAN_TRUE(BUILD_DEMO_MACRO_ANSWER == build_demo_answer());

    /*
     * And the call itself crossed the boundary: the arithmetic below happened
     * inside build_demo.o, from a value defined in this one.
     */
    KOAN_EQ_INT(__, build_demo_add(BUILD_DEMO_MACRO_ANSWER, 8));

    /*
     * The rule this all adds up to: a header is for *declarations*, because it
     * is copied into every file that includes it. A function body in a header
     * would be copied too, and the linker would find several definitions of
     * one name. Bodies go in .c files, which are compiled exactly once.
     *
     * It is also why editing a header rebuilds everything that includes it,
     * while editing one .c file rebuilds only that file — which is precisely
     * what make is keeping track of for you.
     */
}

KOAN_LESSON(lesson_about_compiling, "About Compiling",
    KOAN_CASE(the_preprocessor_replaces_text),
    KOAN_CASE(the_preprocessor_knows_where_it_is),
    KOAN_CASE(a_header_is_pasted_in),
    KOAN_CASE(headers_guard_themselves),
    KOAN_CASE(the_linker_joins_separate_files),
    KOAN_CASE(some_checks_happen_before_the_program_runs),
    KOAN_CASE(unselected_branches_are_never_compiled),
    KOAN_CASE(the_standard_version_is_a_compile_time_fact),
    KOAN_CASE(a_mental_model_of_the_build)
);
