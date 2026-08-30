/*
 * build_demo.h — the "promise" half of a two-file program.
 *
 * A header holds declarations: enough for the compiler to check a call, and no
 * more. The definitions live in build_demo.c, which is compiled separately and
 * joined to this file by the linker.
 *
 * Open build_demo.c alongside this while you work through about_compiling.
 */
#ifndef BUILD_DEMO_H
#define BUILD_DEMO_H

/*
 * A declaration: "a function called build_demo_add exists somewhere, it takes
 * two ints, and it gives back an int." The body is not here.
 */
int build_demo_add(int a, int b);

/* Also declared here, also defined in build_demo.c. */
int build_demo_answer(void);

/*
 * A header may also carry values, because a value costs no storage until it is
 * used. This one is a macro: the preprocessor replaces the name with the text
 * `42` before the compiler ever sees it.
 */
#define BUILD_DEMO_MACRO_ANSWER 42

#endif /* BUILD_DEMO_H */
