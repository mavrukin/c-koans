/*
 * build_demo.h — the "promise" half of a two-file program.
 *
 * A header contains declarations: enough for the compiler to check a call,
 * and no more. The definitions live in build_demo.c, which is compiled
 * separately and joined to this one by the linker.
 *
 * Open build_demo.c alongside this file while you work through
 * about_compiling.
 */
#ifndef BUILD_DEMO_H
#define BUILD_DEMO_H

/* A declaration: "this function exists somewhere, here is how to call it." */
int build_demo_add(int a, int b);

/* Declared here, defined in build_demo.c. The compiler never sees the body. */
const char *build_demo_unit_name(void);

/* Values a header may legitimately carry, because they cost no storage. */
#define BUILD_DEMO_MACRO_ANSWER 42
constexpr int BUILD_DEMO_CONSTEXPR_ANSWER = 42;

#endif /* BUILD_DEMO_H */
