/*
 * About Processes, Signals and Pipes
 *
 * Below the C standard library sits the operating system, and on every system
 * you are likely to use, the interface to it is POSIX. This lesson is where
 * your programs stop being self-contained and start doing work in the world.
 *
 * Three ideas, each of which the koans build on in turn:
 *
 *   fork()   duplicates the calling process
 *   exec()   replaces the running program, keeping the process
 *   pipe()   connects one process's output to another's input
 *
 * Everything from a shell to a build system is those three, composed.
 */
#include "koan.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Every process has an id, and knows its parent's. These are the handles by
 * which the kernel and you refer to it.
 */
KOAN(a_process_has_an_identity)
{
    pid_t me = getpid();
    pid_t parent = getppid();

    KOAN_TRUE(me > 0);
    KOAN_TRUE(parent > 0);
    KOAN_TRUE(me != parent);
}

/*
 * fork() returns *twice*: zero in the new child, and the child's pid in the
 * parent. That single fact is the whole of process creation in POSIX, and it
 * takes a moment to accept.
 *
 * The child must not fall through into the parent's code. Ending it with
 * _exit() rather than exit() avoids flushing the parent's copy of stdio
 * buffers a second time.
 */
KOAN(fork_returns_twice)
{
    pid_t pid = fork();
    KOAN_TRUE(pid >= 0);            /* negative would mean the fork failed */

    if (pid == 0) {
        /* The child. Exit with a status the parent can read back. */
        _exit(7);
    }

    /* The parent waits, and decodes the child's exit status. */
    int status = 0;
    pid_t reaped = waitpid(pid, &status, 0);

    KOAN_TRUE(reaped == pid);
    KOAN_EQ_INT(__, WIFEXITED(status));
    KOAN_EQ_INT(__, WEXITSTATUS(status));
}

/*
 * A child that is killed by a signal is reported differently from one that
 * exited. Always ask WIFEXITED before trusting WEXITSTATUS — they answer
 * different questions.
 */
KOAN(a_signalled_child_is_reported_differently)
{
    pid_t pid = fork();
    KOAN_TRUE(pid >= 0);

    if (pid == 0) {
        raise(SIGKILL);         /* end this child by signal, not by exit */
        _exit(0);               /* not reached */
    }

    int status = 0;
    waitpid(pid, &status, 0);

    KOAN_EQ_INT(__, WIFEXITED(status));
    KOAN_EQ_INT(__, WIFSIGNALED(status));
    KOAN_EQ_INT(__, WTERMSIG(status));
}

/*
 * The child inherits a copy of the parent's memory, not a share of it.
 * Writes on either side are invisible to the other — which is precisely why
 * pipes and shared memory exist.
 */
KOAN(the_child_gets_a_copy_not_a_share)
{
    int value = 1;

    pid_t pid = fork();
    if (pid == 0) {
        value = 999;            /* only the child's copy changes */
        _exit(0);
    }
    waitpid(pid, nullptr, 0);

    KOAN_EQ_INT(__, value);
}

/*
 * A pipe is a one-way byte channel: fds[0] to read, fds[1] to write. Closing
 * the unused end in each process is not optional housekeeping — the reader
 * only sees end-of-file once *every* copy of the write end is closed.
 */
KOAN(a_pipe_carries_bytes_between_processes)
{
    int fds[2];
    KOAN_EQ_INT(__, pipe(fds));

    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);                          /* child will not read */
        const char *msg = "from the child";
        ssize_t n = write(fds[1], msg, strlen(msg));
        (void)n;
        close(fds[1]);
        _exit(0);
    }

    close(fds[1]);                              /* parent will not write */

    char    buf[64] = {};
    ssize_t got = read(fds[0], buf, sizeof buf - 1);
    close(fds[0]);
    waitpid(pid, nullptr, 0);

    KOAN_TRUE(got > 0);
    KOAN_EQ_STR(__STR, buf);
    KOAN_EQ_SZ(__SZ, (size_t)got);
}

/*
 * exec replaces the program running inside the process. The pid is unchanged;
 * everything else — the code, the heap, the stack — is discarded. On success
 * exec does not return at all, so any code after it is the error path.
 *
 * Combined with dup2, this is how a shell redirects output.
 */
KOAN(exec_replaces_the_program_and_dup2_redirects_it)
{
    int fds[2];
    pipe(fds);

    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);   /* the child's stdout is now the pipe */
        close(fds[1]);

        execlp("echo", "echo", "-n", "replaced", (char *)nullptr);
        _exit(127);                    /* only reached if exec failed */
    }

    close(fds[1]);
    char    buf[64] = {};
    ssize_t got = read(fds[0], buf, sizeof buf - 1);
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    KOAN_TRUE(got > 0);
    KOAN_EQ_STR(__STR, buf);
    KOAN_EQ_INT(__, WEXITSTATUS(status));
}

/*
 * Signals interrupt a process asynchronously. A handler runs between two
 * instructions of whatever was executing, which is why almost nothing is safe
 * to do inside one.
 *
 * The only variable type you may portably touch from a handler is
 * `volatile sig_atomic_t`. Not int, not a struct, and certainly not printf.
 */
static volatile sig_atomic_t signals_seen = 0;

static void count_signal(int sig)
{
    (void)sig;
    signals_seen++;             /* the one thing that is safe here */
}

KOAN(signal_handlers_run_between_instructions)
{
    struct sigaction sa = { 0 };
    struct sigaction previous;

    sa.sa_handler = count_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;   /* resume interrupted syscalls rather than EINTR */

    KOAN_EQ_INT(__, sigaction(SIGUSR1, &sa, &previous));

    signals_seen = 0;
    raise(SIGUSR1);
    raise(SIGUSR1);

    KOAN_EQ_INT(__, signals_seen);

    /* Put back whatever was there before: handlers are process-wide state. */
    sigaction(SIGUSR1, &previous, nullptr);
}

/*
 * Blocking a signal defers it rather than discarding it. This is how you make
 * a critical section safe from interruption: block, do the work, unblock, and
 * the pending signal is delivered at that moment.
 */
KOAN(blocking_a_signal_defers_it)
{
    struct sigaction sa = { 0 }, previous;
    sa.sa_handler = count_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, &previous);

    sigset_t block_usr1, saved;
    sigemptyset(&block_usr1);
    sigaddset(&block_usr1, SIGUSR1);

    signals_seen = 0;
    sigprocmask(SIG_BLOCK, &block_usr1, &saved);

    raise(SIGUSR1);                       /* now pending, not delivered */
    KOAN_EQ_INT(__, signals_seen);

    /* It is genuinely pending, and the kernel will say so. */
    sigset_t pending;
    sigpending(&pending);
    KOAN_EQ_INT(__, sigismember(&pending, SIGUSR1));

    sigprocmask(SIG_SETMASK, &saved, nullptr);   /* delivered right here */
    KOAN_EQ_INT(__, signals_seen);

    sigaction(SIGUSR1, &previous, nullptr);
}

/*
 * Bringing it together.
 *
 * Run a command, capture its standard output, and return its exit status —
 * the primitive that every build tool and test runner is built on. It needs
 * fork, exec, pipe, dup2, wait and careful fd hygiene, all at once.
 *
 * Note the order of the closes. Leaking a write end into the parent would
 * mean the read below never sees end-of-file, and the koan would hang forever.
 */
static int capture(const char *const argv[], char *out, size_t cap, bool *ok)
{
    int fds[2];
    *ok = false;
    if (pipe(fds) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }

    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }

    close(fds[1]);              /* essential: the parent must not hold it open */

    size_t used = 0;
    for (;;) {
        ssize_t n = read(fds[0], out + used, cap - 1 - used);
        if (n <= 0) break;
        used += (size_t)n;
        if (used >= cap - 1) break;
    }
    out[used] = '\0';
    close(fds[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;

    *ok = WIFEXITED(status);
    return *ok ? WEXITSTATUS(status) : -1;
}

KOAN(assembling_a_command_runner)
{
    char buf[256];
    bool ok = false;

    const char *const echo_args[] = { "echo", "-n", "captured", nullptr };
    KOAN_EQ_INT(__, capture(echo_args, buf, sizeof buf, &ok));
    KOAN_EQ_INT(__, ok);
    KOAN_EQ_STR(__STR, buf);

    /* A non-zero exit status is data, not a failure of the runner. */
    const char *const false_args[] = { "false", nullptr };
    KOAN_TRUE(capture(false_args, buf, sizeof buf, &ok) != 0);
    KOAN_EQ_INT(__, ok);

    /* A command that does not exist exits 127, by the convention exec's
     * error path above follows. */
    const char *const missing[] = { "koan-no-such-command", nullptr };
    KOAN_EQ_INT(__, capture(missing, buf, sizeof buf, &ok));
}

KOAN_LESSON(lesson_about_processes, "About Processes, Signals and Pipes",
    KOAN_CASE(a_process_has_an_identity),
    KOAN_CASE(fork_returns_twice),
    KOAN_CASE(a_signalled_child_is_reported_differently),
    KOAN_CASE(the_child_gets_a_copy_not_a_share),
    KOAN_CASE(a_pipe_carries_bytes_between_processes),
    KOAN_CASE(exec_replaces_the_program_and_dup2_redirects_it),
    KOAN_CASE(signal_handlers_run_between_instructions),
    KOAN_CASE(blocking_a_signal_defers_it),
    KOAN_CASE(assembling_a_command_runner)
);
