/*
 * About Directories
 *
 * A directory is a file whose contents are names. You do not read it with
 * read(); you open it with opendir and pull entries out one at a time.
 *
 * Two rules that catch everyone:
 *
 *   - "." and ".." are always present, and a recursive walk that forgets to
 *     skip them will not terminate.
 *   - readdir gives you a *name*, not a path. To stat it you must join it to
 *     the directory you got it from.
 */
#include "koan.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Build a small tree the koans can walk, and tear it down afterwards. */
static const char *TREE = "koan_tree";

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) KOAN_FAIL("could not create %s", path);
    fputs(text, f);
    fclose(f);
}

static void build_tree(void)
{
    mkdir(TREE, 0700);
    mkdir("koan_tree/sub", 0700);
    write_file("koan_tree/a.txt", "aaa");
    write_file("koan_tree/b.txt", "bb");
    write_file("koan_tree/sub/c.txt", "c");
}

static void destroy_tree(void)
{
    unlink("koan_tree/sub/c.txt");
    rmdir("koan_tree/sub");
    unlink("koan_tree/a.txt");
    unlink("koan_tree/b.txt");
    rmdir(TREE);
}

/*
 * mkdir creates one level and fails if the parent is missing or the directory
 * exists. The mode is masked by the process umask, so the permissions you ask
 * for are an upper bound.
 */
KOAN(mkdir_creates_exactly_one_level)
{
    KOAN_EQ_INT(__, mkdir("koan_mk", 0700));

    /* Creating it twice fails with EEXIST. */
    errno = 0;
    KOAN_EQ_INT(__, mkdir("koan_mk", 0700));
    KOAN_EQ_INT(__, errno);

    /* A missing parent fails with ENOENT — there is no mkdir -p syscall. */
    errno = 0;
    KOAN_EQ_INT(__, mkdir("koan_absent/child", 0700));
    KOAN_EQ_INT(__, errno);

    /* rmdir only removes an empty directory. */
    write_file("koan_mk/f", "x");
    errno = 0;
    KOAN_EQ_INT(__, rmdir("koan_mk"));
    KOAN_TRUE(errno == ENOTEMPTY || errno == EEXIST);

    unlink("koan_mk/f");
    KOAN_EQ_INT(__, rmdir("koan_mk"));
}

/*
 * readdir returns entries in unspecified order, and always includes "." and
 * "..". Never assume alphabetical order — collect and sort if you need it.
 */
KOAN(readdir_includes_dot_and_dotdot)
{
    build_tree();

    DIR *d = opendir(TREE);
    KOAN_TRUE(d != nullptr);

    int entries = 0, dots = 0, named = 0;
    struct dirent *e;

    while ((e = readdir(d)) != nullptr) {
        entries++;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) dots++;
        else named++;
    }
    closedir(d);

    KOAN_EQ_INT(__, dots);
    KOAN_EQ_INT(__, named);      /* a.txt, b.txt, sub */
    KOAN_EQ_INT(__, entries);

    destroy_tree();
}

/*
 * The entry gives a bare name. To learn anything about it you must build the
 * full path and stat that — a very common bug is statting the name alone,
 * which silently examines the wrong file in the current directory.
 */
static bool join_path(char *out, size_t cap, const char *dir, const char *name)
{
    int n = snprintf(out, cap, "%s/%s", dir, name);
    return n > 0 && (size_t)n < cap;
}

KOAN(an_entry_is_a_name_not_a_path)
{
    build_tree();

    char path[512];
    KOAN_EQ_INT(__, join_path(path, sizeof path, TREE, "a.txt"));
    KOAN_EQ_STR(__STR, path);

    struct stat st;
    KOAN_EQ_INT(__, stat(path, &st));
    KOAN_EQ_INT(__, (int)st.st_size);

    /* Statting the bare name looks in the *current* directory instead, and
     * usually fails — or worse, succeeds on the wrong file. */
    errno = 0;
    KOAN_EQ_INT(__, stat("a.txt", &st));

    /* A path too long for the buffer is refused rather than truncated. */
    char tiny[8];
    KOAN_EQ_INT(__, join_path(tiny, sizeof tiny, TREE, "a.txt"));

    destroy_tree();
}

/*
 * Distinguishing files from directories is what makes a walk possible.
 * d_type is a useful shortcut but is not in POSIX and is not filled in on
 * every filesystem — so stat is the answer that always works.
 */
KOAN(stat_distinguishes_files_from_directories)
{
    build_tree();

    struct stat st;
    char        path[512];

    join_path(path, sizeof path, TREE, "sub");
    stat(path, &st);
    KOAN_EQ_INT(__, S_ISDIR(st.st_mode) != 0);
    KOAN_EQ_INT(__, S_ISREG(st.st_mode) != 0);

    join_path(path, sizeof path, TREE, "b.txt");
    stat(path, &st);
    KOAN_EQ_INT(__, S_ISREG(st.st_mode) != 0);
    KOAN_EQ_INT(__, (int)st.st_size);

    destroy_tree();
}

/*
 * The working directory is process-wide state. Changing it affects every
 * relative path in the program, including in other threads — which is why a
 * library should never call chdir.
 */
KOAN(the_working_directory_is_process_wide)
{
    build_tree();

    char before[1024];
    KOAN_TRUE(getcwd(before, sizeof before) != nullptr);

    KOAN_EQ_INT(__, chdir(TREE));

    /* Relative paths now resolve inside the tree. */
    struct stat st;
    KOAN_EQ_INT(__, stat("a.txt", &st));
    KOAN_EQ_INT(__, (int)st.st_size);

    char inside[1024];
    getcwd(inside, sizeof inside);
    KOAN_TRUE(strcmp(before, inside) != 0);
    KOAN_TRUE(strstr(inside, "koan_tree") != nullptr);

    /* Always restore it, or every later path in the program is wrong. */
    KOAN_EQ_INT(__, chdir(before));
    KOAN_EQ_INT(__, stat("a.txt", &st));

    destroy_tree();
}

/*
 * Bringing it together.
 *
 * Walk a directory tree recursively, summing file sizes and counting
 * directories — `du`, in thirty lines. It must skip "." and ".." (or recurse
 * forever), build full paths, stat rather than guess, and bound its own depth
 * so a symlink loop cannot run away with it.
 */
typedef struct {
    size_t files;
    size_t dirs;
    size_t total_bytes;
    size_t max_depth_seen;
    bool   depth_exceeded;
} WalkStats;

static void walk(const char *dir, int depth, int max_depth, WalkStats *out)
{
    if (depth > max_depth) { out->depth_exceeded = true; return; }
    if ((size_t)depth > out->max_depth_seen) out->max_depth_seen = (size_t)depth;

    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
        /* Without this the walk descends into itself, forever. */
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;

        char path[1024];
        if (!join_path(path, sizeof path, dir, e->d_name)) continue;

        struct stat st;
        if (lstat(path, &st) != 0) continue;   /* lstat: do not follow links */

        if (S_ISDIR(st.st_mode)) {
            out->dirs++;
            walk(path, depth + 1, max_depth, out);
        } else if (S_ISREG(st.st_mode)) {
            out->files++;
            out->total_bytes += (size_t)st.st_size;
        }
    }
    closedir(d);
}

KOAN(assembling_a_recursive_walk)
{
    build_tree();

    WalkStats s = { 0 };
    walk(TREE, 0, 16, &s);

    /* Three files: a.txt (3), b.txt (2), sub/c.txt (1). */
    KOAN_EQ_SZ(__SZ, s.files);
    KOAN_EQ_SZ(__SZ, s.total_bytes);

    /* One directory below the root. */
    KOAN_EQ_SZ(__SZ, s.dirs);

    /* The walk reached depth 1 and no further. */
    KOAN_EQ_SZ(__SZ, s.max_depth_seen);
    KOAN_EQ_INT(__, s.depth_exceeded);

    /* A depth limit of zero refuses to descend, and says so. */
    WalkStats shallow = { 0 };
    walk(TREE, 0, 0, &shallow);
    KOAN_EQ_SZ(__SZ, shallow.files);        /* only the top level */
    KOAN_EQ_SZ(__SZ, shallow.total_bytes);
    KOAN_EQ_INT(__, shallow.depth_exceeded);

    /* Walking something that is not a directory yields nothing, not a crash. */
    WalkStats none = { 0 };
    walk("koan_tree/a.txt", 0, 16, &none);
    KOAN_EQ_SZ(__SZ, none.files);

    destroy_tree();
}

KOAN_LESSON(lesson_about_directories, "About Directories",
    KOAN_CASE(mkdir_creates_exactly_one_level),
    KOAN_CASE(readdir_includes_dot_and_dotdot),
    KOAN_CASE(an_entry_is_a_name_not_a_path),
    KOAN_CASE(stat_distinguishes_files_from_directories),
    KOAN_CASE(the_working_directory_is_process_wide),
    KOAN_CASE(assembling_a_recursive_walk)
);
