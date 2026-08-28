/*
 * build_demo.c — the "definition" half.
 *
 * This file is compiled on its own into build_demo.o, then linked with the
 * rest. Nothing here has blanks; it exists so about_compiling can call across
 * a real translation-unit boundary.
 */
#include "build_demo.h"

int build_demo_add(int a, int b)
{
    return a + b;
}

const char *build_demo_unit_name(void)
{
    /* __FILE__ is filled in by the preprocessor, per file, at compile time. */
    return __FILE__;
}
