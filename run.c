#define _POSIX_C_SOURCE 1L
#define _XOPEN_SOURCE   500
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
#ifdef __GONWARE__
#include <gonware/syscall.h>
#endif
#include "deco.h"
#include "dir.h"
#include "env.h"

#define QUANT 16

static char **arg;
static int arglen, argp;

static int run(char *name, char **ap, int (*exe)(), int silent);

void runset(char *dest)
{
    arg    = (char **)malloc(QUANT * sizeof(char *));
    arglen = QUANT;
    arg[0] = dest;
    argp   = 1;
}

void runarg(char *name)
{
    mcheck(arg, char **, arglen, QUANT, argp);
    arg[argp++] = name;
}

int rundone(char *name, char *a0, char *a1, char *a2)
{
    register char **p, **q, *a;
    int ret;

    if (a2)
        runarg(a2);
    if (a1)
        runarg(a1);
    runarg(a0);
    for (p = arg, q = arg + argp - 1; p < q; ++p, --q) {
        a  = *p;
        *p = *q;
        *q = a;
    }
    runarg(0);
    ret = runv(1, name, arg) == 0;
    free(arg);
    return (ret);
}

/* to as a directory: to/<last part of from> */
static char *target(char *from, char *to, char *buf, size_t size)
{
    struct stat st;
    char *base;

    if (stat(to, &st) < 0 || !S_ISDIR(st.st_mode))
        return to;
    base = strrchr(from, '/');
    snprintf(buf, size, "%s/%s", to, base ? base + 1 : from);
    return buf;
}

static int copyfile(char *from, char *to)
{
    char buf[8192];
    struct stat st;
    struct utimbuf times;
    int in, out, err = 0;
    ssize_t n;

    if ((in = open(from, O_RDONLY)) < 0)
        return -1;
    if (fstat(in, &st) < 0 ||
        (out = open(to, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777)) < 0) {
        close(in);
        return -1;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, n) != n) {
            err = -1;
            break;
        }
    if (n < 0)
        err = -1;
    close(in);
    if (close(out) < 0)
        err = -1;
    if (err) {
        unlink(to);
        return -1;
    }
    /* like cp -p, as far as the file system keeps it */
    (void)chmod(to, st.st_mode & 07777);
    times.actime  = st.st_atime;
    times.modtime = st.st_mtime;
    (void)utime(to, &times);
    return 0;
}

int fileop(int op, char *from, char *to)
{
    char buf[MAXPATHLEN];

    to = target(from, to, buf, sizeof(buf));
    switch (op) {
    case FO_COPY:
        return copyfile(from, to);
    case FO_MOVE:
        if (rename(from, to) == 0)
            return 0;
        if (errno != EXDEV || copyfile(from, to) < 0)
            return -1;
        return unlink(from);
    case FO_LINK:
        (void)unlink(to);
        return link(from, to);
    case FO_SYMLINK:
        (void)unlink(to);
        return symlink(from, to);
    }
    return -1;
}

int filesdone(int op)
{
    int i, ok = 1;

    for (i = 1; i < argp; ++i)
        if (fileop(op, arg[i], arg[0]) < 0)
            ok = 0;
    free(arg);
    return ok;
}

void runcancel()
{
    free(arg);
}

static char *execat(char *s1, char *s2, char *si)
{
    register char *s;

    s = si;
    while (*s1 && *s1 != ':')
        *s++ = *s1++;
    if (si != s)
        *s++ = '/';
    while (*s2)
        *s++ = *s2++;
    *s = 0;
    return (*s1 ? ++s1 : 0);
}

static int execvpe(char *name, char **argv, char **envstr)
{
    static char *pathstr;
    char fname[128];
    char *newargs[256];
    int i;
    register char *cp;
    register unsigned etxtbsy = 1;
    register int eacces       = 0;

    if (!pathstr && !(pathstr = EnvGet("PATH")))
        pathstr = ":/bin:/usr/bin";
    for (cp = name;; ++cp) { /* check if name contains '/' */
        if (!*cp) {
            cp = pathstr; /* if no, set cp to pathstr */
            break;
        }
        if (*cp == '/') {
            cp = ""; /* else path is empty */
            break;
        }
    }
    do {
        cp = execat(cp, name, fname);
    retry:
        (void)execve(fname, argv, envstr);
        switch (errno) {
        case ENOEXEC:
            newargs[0] = "sh";
            newargs[1] = fname;
            for (i = 1; (newargs[i + 1] = argv[i]); ++i) {
                if (i >= 254) {
                    errno = E2BIG;
                    return (-1);
                }
            }
            (void)execve("/bin/sh", newargs, envstr);
            return (-1);
        case ETXTBSY:
            if (++etxtbsy > 5)
                return (-1);
            (void)sleep(etxtbsy);
            goto retry;
        case EACCES:
            ++eacces;
            break;
        case ENOMEM:
        case E2BIG:
            return (-1);
        }
    } while (cp);
    if (eacces)
        errno = EACCES;
    return (-1);
}

/*
 * runl() can have no more than 10 arguments.
 */
#define RUNL_MAXARG 10

int runl(int silent, char *name, ...)
{
    char *args[RUNL_MAXARG];
    int num_args = 0;
    va_list ap;
    int err;

    /* Collect all arguments. */
    va_start(ap, name);
    for (;;) {
        char *a          = va_arg(ap, char *);
        args[num_args++] = a;
        if (!a) {
            break;
        }
        if (num_args >= RUNL_MAXARG) {
            outerr("%s: Too many arguments.\n", name);
            return -1;
        }
    }
    va_end(ap);

    err = run(name, args, execve, silent);
    return err;
}

int runv(int silent, char *name, char **a0)
{
    return (run(name, a0, execvpe, silent));
}

void sigchild(int sig)
{
    int status = 0;

    signal(SIGCHLD, sigchild);
    waitpid(-1, &status, WNOHANG | WUNTRACED);
}

void sigign()
{
    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigchild);
    signal(SIGTTOU, SIG_IGN);
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_IGN);
#endif
}

void sigdfl()
{
    signal(SIGTERM, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif
}

#ifdef __GONWARE__
/* GDOS starts a program and returns when it has ended (GDOS_SPAWN): no
 * fork, no process groups; the status comes back as wait() would give it */
static int run(char *name, char **a0, int (*exe)(char *, char **, char **), int silent)
{
    long r;

    (void)exe;
    r = gdos_call3(GDOS_SPAWN, name, a0, EnvVector);
    if (r < 0) {
        if (!silent)
            outerr("%s: Command not found.\n", name);
        return 0x7f00;
    }
    return (int)(r & 0xff) << 8;
}
#else
static int run(char *name, char **a0, int (*exe)(char *, char **, char **), int silent)
{
    register int t;
    int status = 0;
    static char **arg0;

    arg0 = a0;
    signal(SIGCHLD, SIG_IGN);
    t = fork();
    if (t == -1)
        /* cannot fork */
        return (0x7e00);
    if (t == 0) {
        /* Child. */
        sigdfl();
        if (silent) {
            close(0);
            close(1);
            close(2);
            open("/dev/null", 0);
            open("/dev/null", 1);
            dup(1);
        } else {
            /* Set for child the separate process group.
             * Give the terminal to the child. */
            int child_pid = getpid();
            setpgid(child_pid, child_pid);
            tcsetpgrp(2, child_pid);
        }
        signal(SIGTTOU, SIG_DFL);
        (*exe)(name, arg0, EnvVector);
        if (!silent)
            outerr("%s: Command not found.\n", name);
        _exit(0x7f); /* file not found */
    }

    /* Parent. */
    waitpid(t, &status, WUNTRACED);

    /* Get back the terminal. */
    if (!silent) {
        tcsetpgrp(2, main_pid);
        killpg(t, SIGCONT);
    }
    signal(SIGCHLD, sigchild);

    if (WIFSTOPPED(status))
        return -t;
    return (status);
}
#endif
