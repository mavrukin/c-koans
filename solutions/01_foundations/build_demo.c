/*
 * build_demo.c — the "definition" half.
 *
 * Compiled on its own into build_demo.o, then linked with the rest. Nothing
 * here has blanks; it exists so about_compiling can call across a real
 * translation-unit boundary rather than describe one.
 */
#include "build_demo.h"

int build_demo_add(int a, int b)
{
    return a + b;
}

/*
 * Returns the macro from build_demo.h — as *this* file's preprocessor expanded
 * it. about_compiling compares it against its own copy, which is how you can
 * see that the header really was pasted into both files.
 */
int build_demo_answer(void)
{
    return BUILD_DEMO_MACRO_ANSWER;
}
