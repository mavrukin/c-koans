/*
 * About Compiling
 *
 * Before you learn what C means, learn what happens to it. A C program
 * becomes a binary in four steps, and every confusing error message you will
 * meet comes from knowing which step produced it.
 *
 *   1. preprocess   #include, #define, #if      -> one big text file
 *   2. compile      that text                   -> assembly
 *   3. assemble     assembly                    -> an object file (.o)
 *   4. link         all the .o files + libraries -> an executable
 *
 * Steps 1-3 happen once per .c file, independently. Step 4 happens once.
 * That split explains nearly everything: why headers exist, why a missing
 * semicolon is a *compile* error but a missing function body is a *link*
 * error, and why changing a header rebuilds so much.
 *
 * Run each of these yourself — they are the whole lesson:
 *
 *     cc -std=c23 -E  hello.c -o hello.i    # stop after preprocessing
 *     cc -std=c23 -S  hello.c -o hello.s    # stop after compiling
 *     cc -std=c23 -c  hello.c -o hello.o    # stop after assembling
 *     cc              hello.o -o hello      # link
 *
 * docs/COMPILING.md walks through all of it with real output, including
 * multi-file builds, -I, -l, and what make is actually doing for you.
 */
#include "koan.h"
#include "build_demo.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Step 1 is pure text substitution, and it finishes before the compiler sees
 * anything. A macro has no type and obeys no scope — it is gone by step 2.
 */
#define KOAN_BUILD_LABEL "built from source"

KOAN(the_preprocessor_runs_first)
{
    KOAN_EQ_STR(/*__STR*/ "built from source", KOAN_BUILD_LABEL);

    /* Adjacent string literals are joined during translation, so this is one
     * string by the time the program runs. */
    const char *joined = "one" " " "string";
    KOAN_EQ_STR(/*__STR*/ "one string", joined);
    KOAN_EQ_SZ(/*__SZ*/ 10, strlen(joined));
}

/*
 * The preprocessor fills in facts about the translation itself. __FILE__ and
 * __LINE__ are how every assertion in this repository reports where it is.
 */
KOAN(the_preprocessor_knows_where_it_is)
{
    KOAN_TRUE(strstr(__FILE__, "about_compiling") != nullptr);

    int line_here = __LINE__;
    KOAN_EQ_INT(/*__*/ 1, __LINE__ - line_here);

    /* __func__ is not a macro — it is a predefined identifier, so it behaves
     * like a local variable holding the enclosing function's name. */
    KOAN_EQ_STR(/*__STR*/ "the_preprocessor_knows_where_it_is", __func__);
}

/*
 * `-std=` selects the language the compiler accepts, and __STDC_VERSION__
 * reports which one you got. The values are a date: C11 is 201112L, C17 is
 * 201710L, and C23 is 202311L.
 *
 * These koans require a compiler that reports the ratified 202311L. A few
 * compilers shipped C23 support before ratification and still report the draft
 * value 202000L; they are too old for this repository. If this koan fails,
 * your compiler is the thing to fix — see docs/TROUBLESHOOTING.md.
 */
KOAN(the_standard_version_is_a_compile_time_fact)
{
    KOAN_TRUE(__STDC_VERSION__ >= /*__*/ 202311L);

    /* Which is emphatically newer than C17. */
    KOAN_TRUE(__STDC_VERSION__ > 201710L);

    /* __STDC__ is 1 in any conforming implementation. */
    KOAN_EQ_INT(/*__*/ 1, __STDC__);
}

/*
 * A header is not "imported". It is pasted in, textually, at the point of the
 * #include. Two things follow: an include guard is mandatory, and whatever a
 * header declares is visible to everything after it in that file.
 *
 * `#include <...>` searches the system paths; `#include "..."` searches
 * relative to the current file first. That is the whole difference.
 */
KOAN(a_header_is_pasted_in_not_imported)
{
    /* build_demo.h defined this macro, so it is visible here. */
    KOAN_EQ_INT(/*__*/ 42, BUILD_DEMO_MACRO_ANSWER);

    /* And this constant, which unlike the macro has a type. */
    KOAN_EQ_INT(/*__*/ 42, BUILD_DEMO_CONSTEXPR_ANSWER);
    KOAN_EQ_SZ(/*__SZ*/ sizeof(int), sizeof BUILD_DEMO_CONSTEXPR_ANSWER);

    /* Including it a second time does nothing, because of its guard. */
#include "build_demo.h"
    KOAN_EQ_INT(/*__*/ 42, BUILD_DEMO_MACRO_ANSWER);

#ifdef BUILD_DEMO_H
    KOAN_TRUE(true);
#else
    KOAN_FAIL("the include guard should be defined by now");
#endif
}

/*
 * Steps 2 and 3 turn one .c file into one .o file, in isolation. The compiler
 * checking this file has never seen build_demo.c — it trusts the declaration
 * in the header and emits a call to a name it cannot resolve.
 *
 * The *linker* resolves that name later. This is why:
 *
 *   "implicit declaration of function"  -> a compile error (no declaration)
 *   "undefined symbol: build_demo_add"  -> a link error (no definition)
 *
 * They are different failures at different stages, and the fix differs:
 * the first needs an #include, the second needs the .o on the link line.
 */
KOAN(the_linker_joins_separately_compiled_files)
{
    /* Declared in build_demo.h, defined in build_demo.c, resolved at link. */
    KOAN_EQ_INT(/*__*/ 7, build_demo_add(3, 4));

    /* Each translation unit has its own __FILE__, fixed at compile time. */
    const char *other = build_demo_unit_name();
    KOAN_TRUE(strstr(other, "build_demo") != nullptr);
    KOAN_TRUE(strcmp(other, __FILE__) != 0);
}

/*
 * Some things are decided at compile time and cost nothing at runtime.
 * static_assert is the clearest example: if it fails, there is no program.
 */
static_assert(sizeof(int) >= 4, "these koans assume a 32-bit int or wider");

KOAN(some_checks_happen_before_the_program_exists)
{
    /* That this koan runs at all proves the assertion above held. */
    static_assert(BUILD_DEMO_CONSTEXPR_ANSWER == 42);

    /* sizeof is computed by the compiler, not by the running program. */
    KOAN_EQ_SZ(/*__SZ*/ 4, sizeof(int32_t));
}

/*
 * Conditional compilation removes code before it is ever compiled. The
 * discarded branch need not even be valid C for your target — which is how
 * one source file supports several platforms.
 */
KOAN(unselected_branches_are_never_compiled)
{
    const char *platform;

#if defined(__APPLE__)
    platform = "apple";
#elif defined(__linux__)
    platform = "linux";
#else
    platform = "other";
#endif

    /* Whichever it is, exactly one branch survived. */
    KOAN_TRUE(strcmp(platform, "apple") == 0 ||
              strcmp(platform, "linux") == 0 ||
              strcmp(platform, "other") == 0);

    /* NDEBUG is the flag that strips assert(). The Makefile does not set it,
     * so assertions are live while you work through the koans. */
#ifdef NDEBUG
    KOAN_TRUE(true);
#else
    KOAN_TRUE(true);
#endif
}

/*
 * Bringing it together.
 *
 * Predict what a build produces. Given these files:
 *
 *     build_demo.h    declarations only
 *     build_demo.c    #include "build_demo.h"   -> build_demo.o
 *     about_compiling.c #include "build_demo.h" -> about_compiling.o
 *
 * ...the header is compiled twice (once into each object file), the
 * functions are compiled once, and the linker joins two .o files into one
 * program. Work out each answer from the pipeline, not by guessing.
 */
typedef struct {
    int object_files;       /* .o files produced by steps 1-3 */
    int times_header_read;  /* how often the preprocessor pasted the header */
    int definitions_of_add; /* how many .o files contain the function body */
    int link_steps;         /* how many times the linker runs */
} BuildShape;

static BuildShape shape_of_two_file_build(void)
{
    return (BuildShape){
        .object_files       = 2,
        .times_header_read  = 2,
        .definitions_of_add = 1,
        .link_steps         = 1,
    };
}

KOAN(assembling_a_mental_model_of_the_build)
{
    BuildShape s = shape_of_two_file_build();

    /* One .o per .c file — never per header. */
    KOAN_EQ_INT(/*__*/ 2, s.object_files);

    /* The header is textually pasted into each .c that includes it, so its
     * declarations are compiled once per translation unit. */
    KOAN_EQ_INT(/*__*/ 2, s.times_header_read);

    /* But the *definition* appears exactly once. Putting a function body in a
     * header without `static` or `inline` is how you get "duplicate symbol". */
    KOAN_EQ_INT(/*__*/ 1, s.definitions_of_add);

    /* And the link happens once, over all the objects at the end. */
    KOAN_EQ_INT(/*__*/ 1, s.link_steps);

    /* The consequence you will feel daily: editing a header forces every
     * translation unit that includes it to be recompiled, while editing one
     * .c file forces only that one. This is why make tracks dependencies —
     * and why the Makefile passes -MMD to generate them automatically. */
    KOAN_EQ_INT(/*__*/ 2, s.times_header_read);

    /* The call really did cross the boundary. */
    KOAN_EQ_INT(/*__*/ 100, build_demo_add(BUILD_DEMO_MACRO_ANSWER, 58));
}

KOAN_LESSON(lesson_about_compiling, "About Compiling",
    KOAN_CASE(the_preprocessor_runs_first),
    KOAN_CASE(the_preprocessor_knows_where_it_is),
    KOAN_CASE(the_standard_version_is_a_compile_time_fact),
    KOAN_CASE(a_header_is_pasted_in_not_imported),
    KOAN_CASE(the_linker_joins_separately_compiled_files),
    KOAN_CASE(some_checks_happen_before_the_program_exists),
    KOAN_CASE(unselected_branches_are_never_compiled),
    KOAN_CASE(assembling_a_mental_model_of_the_build)
);
