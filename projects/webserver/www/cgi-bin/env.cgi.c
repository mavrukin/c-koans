/*
 * A CGI program, in C.
 *
 * CGI is the oldest and simplest way to run a program per HTTP request: the
 * server sets environment variables, runs the program, and forwards whatever
 * it writes to stdout. The program emits its own headers, then a blank line,
 * then the body.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *env_or(const char *name, const char *fallback)
{
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

int main(void)
{
    time_t now = time(NULL);
    char   when[64];
    strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S UTC", gmtime(&now));

    printf("Content-Type: text/plain; charset=utf-8\r\n");
    printf("\r\n");

    printf("Hello from a CGI program written in C.\n\n");
    printf("time            %s\n", when);
    printf("REQUEST_METHOD  %s\n", env_or("REQUEST_METHOD", "?"));
    printf("SCRIPT_NAME     %s\n", env_or("SCRIPT_NAME", "?"));
    printf("QUERY_STRING    %s\n", env_or("QUERY_STRING", "(empty)"));
    printf("REMOTE_ADDR     %s\n", env_or("REMOTE_ADDR", "?"));
    printf("SERVER_SOFTWARE %s\n", env_or("SERVER_SOFTWARE", "?"));
    return 0;
}
