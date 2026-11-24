#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "deco.h"
#include "dir.h"
#include "env.h"
#include "scr.h"

#define SHELL    (usecshell ? cshname : shname)
#define ABSSHELL (usecshell ? cshabsname : shabsname)

#define MAXARGV 400

#ifdef __GONWARE__
#include <gonware/syscall.h>

/* GDOS: the shell is COMMAND.EXE (COMSPEC); /C runs one command line, no
 * argument is the interactive shell */
static char shname[]  = "COMMAND";
static char cshname[] = "COMMAND";
#define shabsname  comspec()
#define cshabsname comspec()
#define SHELL_C    "/C"
#define SHELL_I    0
static char *comspec(void)
{
    char *s = EnvGet("COMSPEC");
    return s && *s ? s : "\\GONWARE\\COMMAND.EXE";
}
#else
static char shname[]     = "sh";
static char cshname[]    = "csh";
static char shabsname[]  = "/bin/sh";
static char cshabsname[] = "/bin/csh";
#define SHELL_C "-c"
#define SHELL_I "-i"
#endif

int usecshell;
int useredit, userview;
int showhidden = 1;
char editname[160], viewname[160];

static int metas(char *s);
static int cmpfil(char *d1, char *f1, char *d2, char *f2);

char *sysmsg[] = {
    0,
    "Hangup",              /* SIGHUP  1  hangup */
    "Interrupt",           /* SIGINT  2  interrupt (rubout) */
    "Quit",                /* SIGQUIT 3  quit */
    "Illegal instruction", /* SIGILL  4  illegal instruction */
    "Trace/BPT trap",      /* SIGTRAP 5  trace trap */
    "IOT trap",            /* SIGIOT  6  IOT instruction */
    "EMT trap",            /* SIGEMT  7  EMT instruction */
    "Floating exception",  /* SIGFPE  8  floating point exception */
    "Killed",              /* SIGKILL 9  kill */
    "Bus error",           /* SIGBUS  10 bus error */
    "Memory fault",        /* SIGSEGV 11 segmentation violation */
    "Bad system call",     /* SIGSYS  12 bad argument to system call */
    "Broken pipe",         /* SIGPIPE 13 write on a pipe with no one to read it */
    "Alarm call",          /* SIGALRM 14 alarm clock */
    "Terminated",          /* SIGTERM 15 software termination signal from kill */
};

int numsysmsg = (sizeof sysmsg / sizeof sysmsg[0]);

/* ARGSUSED */

void directory(int k, int sk)
{
    setdir(cur, sk == 'r' ? "/" : home);
}

void switchalign(int k)
{
    struct dir *d = k == 'l' ? &left : &right;

    d->alignext ^= 1;
}

void switchcmdreg()
{
    cmdreg ^= 1;
}

void switchhidden()
{
    showhidden ^= 1;
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
}

void switchpanels()
{
    cur = cur == &left ? &right : &left;
    chdir(cur->cwd);
}

void setstatus()
{
    if (cur == &left)
        right.status ^= 1;
    else
        left.status ^= 1;
}

void fullscreen(int k, int sk)
{
    VClearBox(1, 0, LINES - 2, COLS);
    if (H == LINES / 2 - 1)
        H = LINES - 7;
    else
        H = LINES / 2 - 1;
    while (left.topfile + PAGELEN(&left) <= left.curfile)
        left.topfile += H;
    while (right.topfile + PAGELEN(&right) <= right.curfile)
        right.topfile += H;
}

void redraw()
{
    VRedraw();
}

void reread(int k, int sk)
{
    struct dir *d = k == 'l' ? &left : &right;
    register char *p;

    if (!(p = getwstring(50, d->cwd, _(" Change directory "), _("Change directory to"))))
        return;
    if (!strcmp(p, d->cwd))
        setdir(d, 0);
    else if (chdir(p) < 0)
        error(_("Cannot chdir to %s"), p);
    else
        setdir(d, ".");
    chdir(cur->cwd);
}

void setdwid()
{
    VClearBox(1, 0, LINES - 2, COLS);
    widewin ^= 1;
    while (left.topfile + PAGELEN(&left) <= left.curfile)
        left.topfile += H;
    while (right.topfile + PAGELEN(&right) <= right.curfile)
        right.topfile += H;
}

void setfull(int k, int sk)
{
    register struct dir *c = k == 'l' ? &left : &right;

    switch (sk) {
    case 'b':
        c->view = VIEW_BRIEF;
        break;
    case 'l':
        c->view = VIEW_LONG;
        break;
    case 'w':
        c->view = VIEW_WIDE;
        break;
    case 'f':
        c->view = VIEW_FULL;
        if (!widewin)
            setdwid();
        break;
    case 'i':
        c->view = VIEW_INFO;
        if (!widewin)
            setdwid();
        break;
    }
    while (c->topfile + PAGELEN(c) <= c->curfile)
        c->topfile += H;
}

void setsort(int k, int sk)
{
    register struct dir *c = k == 'l' ? &left : &right;

    switch (sk) {
    case 'n':
        c->sort = SORTNAME;
        break;
    case 'x':
        c->sort = SORTEXT;
        break;
    case 't':
        c->sort = SORTTIME;
        break;
    case 'z':
        c->sort = SORTSIZE;
        break;
    case 'u':
        c->sort = SORTSKIP;
        break;
    }
    setdir(c, 0);
    chdir(cur->cwd);
}

void settypesort(int k)
{
    register struct dir *c = k == 'l' ? &left : &right;

    c->typesort ^= 1;
    setdir(c, 0);
    chdir(cur->cwd);
}

void setrevsort(int k)
{
    register struct dir *c = k == 'l' ? &left : &right;

    c->revsort ^= 1;
    setdir(c, 0);
    chdir(cur->cwd);
}

void setviewname()
{
    register char *p;

    switch (getchoice(0, _(" View "), _("Select which viewer to use for F3:"), 0, _(" Built-in "),
                      _(" External "), 0)) {
    case 0:
        userview = 0;
    default:
        return;
    case 1:
        break;
    }
    if (!(p = getstring(40, viewname, _(" View "), _("Viewer to use"))))
        return;
    snprintf(viewname, sizeof(viewname), "%s", p);
    userview = 1;
}

void setshellname()
{
    switch (getchoice(0, _(" Shell "), _("Select which shell to use:"), 0, _(" Shell "),
                      _(" Cshell "), 0)) {
    case 0:
        usecshell = 0;
        break;
    case 1:
        usecshell = 1;
        break;
    }
}

void seteditname()
{
    register char *p;

    switch (getchoice(0, _(" Edit "), _("Select which editor to use for F4:"), 0, _(" Built-in "),
                      _(" External "), 0)) {
    case 0:
        useredit = 0;
    default:
        return;
    case 1:
        break;
    }
    if (!(p = getstring(40, editname, _(" Edit "), _("Editor to use"))))
        return;
    snprintf(editname, sizeof(editname), "%s", p);
    useredit = 1;
}

void view()
{
    char buf[80];
    register char *name = cur->cat[cur->curfile].name;
    register int d;

    if (userview) {
        strcpy(buf, viewname);
        strcat(buf, " ");
        strcat(buf, name);
        VRestore();
        syscmd(buf);
        VReopen();
        VRedraw();
        setdir(cur == &left ? &right : &left, 0);
        setdir(cur, 0);
        return;
    }
    if ((d = open(name, 0)) < 0) {
        error(_("Cannot open %s"), name);
        return;
    }
    viewfile(d, name);
    close(d);
}

void viewinfo()
{
    char buf[80];
    char name[5000];
    register int d;

    snprintf(name, sizeof(name), "%s/.info/%s", cur->cwd, cur->cat[cur->curfile].name);
    if (userview) {
        strcpy(buf, viewname);
        strcat(buf, " ");
        strcat(buf, name);
        VRestore();
        syscmd(buf);
        VReopen();
        VRedraw();
        setdir(cur == &left ? &right : &left, 0);
        setdir(cur, 0);
        return;
    }
    if ((d = open(name, 0)) < 0) {
        error(_("Cannot open %s"), name);
        return;
    }
    viewfile(d, name);
    close(d);
}

static void editfnam(char *name)
{
    register int d;
    register char *p;
    char buf[80];
    struct stat st;

    if (stat(name, &st) >= 0) {
        if ((st.st_mode & S_IFMT) != S_IFREG) {
            error(_("Cannot edit special files"));
            return;
        }
    } else {
        d = creat(name, 0644);
        if (d < 0) {
            error(_("Cannot create %s"), name);
            return;
        }
        close(d);
    }
    if (useredit) {
        strcpy(buf, editname);
        strcat(buf, " ");
        strcat(buf, name);
        VRestore();
        syscmd(buf);
        VReopen();
        VRedraw();
        setdir(cur == &left ? &right : &left, 0);
        setdir(cur, 0);
        return;
    }
    if ((d = open(name, 2)) < 0) {
        error(_("Cannot open %s for writing"), name);
        return;
    }
    /* skip directory name */
    for (p = name; *p; ++p)
        ;
    for (; p >= name && *p != '/'; --p)
        ;
    editfile(d, name, p + 1);
    close(d);
}

void edit()
{
    register char *name = cur->cat[cur->curfile].name;
    char namebuf[50];

    if (strcmp(name, ".") == 0) {
        name = getstring(40, 0, _(" Edit "), _("Enter name of file to edit"));
        if (!name)
            return;
        strcpy(namebuf, name);
        editfnam(namebuf);
        return;
    }
    editfnam(name);
}

void editinfo()
{
    char name[5000];

    snprintf(name, sizeof(name), "%s/.info/%s", cur->cwd, cur->cat[cur->curfile].name);
    editfnam(name);
}

void menuedit()
{
    char buf[80];

    switch (getchoice(0, _(" Menu Edit "), _("Select which menu to edit:"), 0, _(" Current menu "),
                      _(" Home menu "), 0)) {
    case 0:
        editfnam(".menu");
        break;
    case 1:
        strcpy(buf, home);
        strcat(buf, "/.menu");
        editfnam(buf);
        break;
    }
}

void extedit()
{
    char buf[80];

    strcpy(buf, home);
    strcat(buf, "/.deco");
    editfnam(buf);
}

void quit()
{
    int choice;
    char quitmsg[100];

    snprintf(quitmsg, sizeof(quitmsg), _("Do you want to quit the %s ?"), CMDR_NAME);
    choice =
        getchoice(ppid == 1, " " CMDR_NAME " ", ppid == 1 ? _("Do you want to log out ?") : quitmsg,
                  0, _(" Yes "), _(" No "), _(" Exec shell "));
    if (choice == 0)
        quitdeco();
    if (choice == 2) {
        /* exec shell */
        VClear();
        VSync();
        VClose();
#ifdef __GONWARE__
        exit(runl(0, ABSSHELL, SHELL, SHELL_I, 0) >> 8);
#else
        execle(ABSSHELL, SHELL, "-i", NULL, EnvVector);
        exit(0);
#endif
    }
    /* else stay here */
}

void quitdeco()
{
    VMove(LINES - 1, 0);
    VClearLine();
    VSync();
    VClose();
    exit(0);
}

void swappanels()
{
    struct dir dir;
    int ro, lo;

    ro             = right.shortcwd - right.cwd;
    lo             = left.shortcwd - left.cwd;
    dir            = left;
    left           = right;
    right          = dir;
    left.basecol   = 0;
    right.basecol  = PANELWID;
    right.shortcwd = lo + right.cwd;
    left.shortcwd  = ro + left.cwd;
    cur            = (cur == &left ? &right : &left);
}

void shell()
{
    /* run shell */
    VClear();
    VSync();
    VRestore();
    runl(0, ABSSHELL, SHELL, SHELL_I, 0);
    VReopen();
    VClear();
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
}

int tagged()
{
    register int i, n;

    for (i = n = 0; i < cur->num; ++i)
        if (cur->cat[i].tag)
            ++n;
    return (n);
}

void copy()
{
    char buf[80];
    char *name;
    register struct file *p;
    register int c;
    int done, all, ch;
    int exd, exf;

    c = tagged();
    if (c) {
        /* copy group of files */

        sprintf(buf, P_("Copy %d file to", "Copy %d files to", c), c);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Copy "), buf)))
            return;
        runset(name);
        exd = exist(name);
        all = 0;
        for (p = cur->cat; p < cur->cat + cur->num; ++p)
            if (p->tag) {
                sprintf(buf, "%s/%s", name, p->name);
                exf = exd == 'd' ? exist(buf) : 0;
                if (exd == 'f' || exf == 'f') {
                    if (!all) {
                        ch = getchoice(1, _(" Copy "), _("File exists"), exf == 'f' ? buf : name,
                                       _(" Overwrite "), _(" All "), _(" Cancel "));
                        switch (ch) {
                        case 2: /* cancel */
                            --c;
                            continue;
                        case 1: /* all */
                            all = 1;
                        case 0: /* all */
                            break;
                        default:
                            runcancel();
                            return;
                        }
                    }
                } else if (exf == 'd') {
                    error(_("%s is a directory"), buf);
                    continue;
                }
                runarg(p->name);
                p->tag = 0;
            }
        if (c <= 0) {
            runcancel();
            return;
        }
        message(_(" Copy "), P_("Copying %d file to %s ...", "Copying %d files to %s ...", c), c,
                name);
        done = filesdone(FO_COPY);
        if (done) {
            endmesg();
            message(_(" Copy "), _("Done"));
        } else
            error(P_("Error while copying %d file to %s", "Error while copying %d files to %s", c),
                  c, name);
        endmesg();
    } else if ((c = (p = &cur->cat[cur->curfile])->mode & S_IFMT) == S_IFDIR) {
        /* copying directory */
    } else if (c == S_IFREG) {
        /* copy regular file */

        sprintf(buf, "Copy \"%s\" to", p->name);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Copy "), buf)))
            return;
        exd = exist(name);
        sprintf(buf, "%s/%s", name, p->name);
        exf = exd == 'd' ? exist(buf) : 0;
        if (exd == 'f' || exf == 'f') {
            if (getchoice(1, _(" Copy "), _("File exists"), exf == 'f' ? buf : name,
                          _(" Overwrite "), _(" Cancel "), 0))
                return;
        } else if (exf == 'd') {
            error(_("%s is a directory"), buf);
            return;
        }
        if (fileop(FO_COPY, p->name, name)) {
            error(_("Cannot copy %s to %s"), p->name, name);
            return;
        }
    }
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
}

void makelink()
{
    char buf[80];
    char *name;
    register struct file *p;
    register int c;
    int done;
    int exd, exf;

    c = tagged();
    if (c) {
        /* link group of files */

        sprintf(buf, P_("Link %d file to", "Link %d files to", c), c);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Link "), buf)))
            return;
        runset(name);
        exd = exist(name);
        for (p = cur->cat; p < cur->cat + cur->num; ++p)
            if (p->tag) {
                sprintf(buf, "%s/%s", name, p->name);
                exf = exd == 'd' ? exist(buf) : 0;
                if (exd == 'f' || exf == 'f') {
                    if (getchoice(1, _(" Link "), _("File exists"), exf == 'f' ? buf : name,
                                  _(" Overwrite "), _(" Cancel "), 0))
                        continue;
                } else if (exf == 'd') {
                    error(_("%s is a directory"), buf);
                    continue;
                }
                runarg(p->name);
                p->tag = 0;
            }
        message(_(" Link "), P_("Linking %d file to %s ...", "Linking %d files to %s ...", c), c,
                name);
        done = filesdone(FO_LINK);
        if (done) {
            endmesg();
            message(_(" Link "), _("Done"));
        } else
            error(P_("Error while linking %d file to %s", "Error while linking %d files to %s", c),
                  c, name);
        endmesg();
    } else if ((c = (p = &cur->cat[cur->curfile])->mode & S_IFMT) == S_IFDIR) {
        /* linking directory */
    } else if (c == S_IFREG) {
        /* link regular file */

        sprintf(buf, "Link \"%s\" to", p->name);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Link "), buf)))
            return;
        exd = exist(name);
        sprintf(buf, "%s/%s", name, p->name);
        exf = exd == 'd' ? exist(buf) : 0;
        if (exd == 'f' || exf == 'f') {
            if (getchoice(1, _(" Link "), _("File exists"), exf == 'f' ? buf : name,
                          _(" Overwrite "), _(" Cancel "), 0))
                return;
        } else if (exf == 'd') {
            error(_("%s is a directory"), buf);
            return;
        }
        if (fileop(FO_LINK, p->name, name)) {
            error(_("Cannot link %s to %s"), p->name, name);
            return;
        }
    }
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
}

#ifdef S_IFLNK
void makeslink()
{
    char buf[80];
    char *name;
    register struct file *p;
    register int c;
    int done;
    int exd, exf;

    c = tagged();
    if (c) {
        /* symlink group of files */

        sprintf(buf, P_("Symlink %d file to", "Symlink %d files to", c), c);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Symlink "), buf)))
            return;
        runset(name);
        exd = exist(name);
        for (p = cur->cat; p < cur->cat + cur->num; ++p)
            if (p->tag) {
                sprintf(buf, "%s/%s", name, p->name);
                exf = exd == 'd' ? exist(buf) : 0;
                if (exd == 'f' || exf == 'f') {
                    if (getchoice(1, _(" Symlink "), _("File exists"), exf == 'f' ? buf : name,
                                  _(" Overwrite "), _(" Cancel "), 0))
                        continue;
                } else if (exf == 'd') {
                    error(_("%s is a directory"), buf);
                    continue;
                }
                runarg(p->name);
                p->tag = 0;
            }
        message(_(" Symlink "), P_("Linking %d file to %s ...", "Linking %d files to %s ...", c), c,
                name);
        done = filesdone(FO_SYMLINK);
        if (done) {
            endmesg();
            message(_(" Symlink "), _("Done"));
        } else
            error(P_("Error while linking %d file to %s", "Error while linking %d files to %s", c),
                  c, name);
        endmesg();
    } else if ((c = (p = &cur->cat[cur->curfile])->mode & S_IFMT) == S_IFDIR) {
        /* symlinking directory */
    } else if (c == S_IFREG) {
        /* symlink regular file */

        sprintf(buf, "Symlink \"%s\" to", p->name);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Symlink "), buf)))
            return;
        exd = exist(name);
        sprintf(buf, "%s/%s", name, p->name);
        exf = exd == 'd' ? exist(buf) : 0;
        if (exd == 'f' || exf == 'f') {
            if (getchoice(1, _(" Symlink "), _("File exists"), exf == 'f' ? buf : name,
                          _(" Overwrite "), _(" Cancel "), 0))
                return;
        } else if (exf == 'd') {
            error(_("%s is a directory"), buf);
            return;
        }
        if (fileop(FO_SYMLINK, p->name, name)) {
            error(_("Cannot symlink %s to %s"), p->name, name);
            return;
        }
    }
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
}
#endif /* S_IFLNK */

void renmove()
{
    char buf[MAXPATHLEN + 20];
    char *name = 0;
    register struct file *p;
    register int c;
    int done, all, ch;
    int exd, exf;

    c = tagged();
    if (c) {
        /* move group of files */

        sprintf(buf, P_("Rename or move %d file to", "Rename or move %d files to", c), c);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Rename "), buf)))
            return;
        runset(name);
        exd = exist(name);
        all = 0;
        for (p = cur->cat; p < cur->cat + cur->num; ++p)
            if (p->tag) {
                sprintf(buf, "%s/%s", name, p->name);
                exf = exd == 'd' ? exist(buf) : 0;
                if (exd == 'f' || exf == 'f') {
                    if (!all) {
                        ch = getchoice(1, _(" Rename "), _("File exists"), exf == 'f' ? buf : name,
                                       _(" Overwrite "), _(" All "), _(" Cancel "));
                        switch (ch) {
                        default:
                        case 2: /* cancel */
                            continue;
                        case 1: /* all */
                            all = 1;
                        }
                    }
                } else if (exf == 'd') {
                    error(_("%s is a directory"), buf);
                    continue;
                }
                runarg(p->name);
            }
        message(_(" Move "), P_("Moving %d file to %s ...", "Moving %d files to %s ...", c), c,
                name);
        done = filesdone(FO_MOVE);
        if (done) {
            endmesg();
            message(_(" Move "), _("Done"));
        } else
            error(P_("Error while moving %d file to %s", "Error while moving %d files to %s", c), c,
                  name);
        endmesg();
        name = 0;
    } else if ((c = (p = &cur->cat[cur->curfile])->mode & S_IFMT) == S_IFDIR) {
        /* rename directory */

        sprintf(buf, "Rename \"%s\" to", strtail(p->name, '/', 60));
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Rename "), buf)))
            return;
        exd = exist(name);
        sprintf(buf, "%s/%s", name, p->name);
        exf = exd == 'd' ? exist(buf) : 0;
        if (exd == 'f' || exf == 'f') {
            if (getchoice(1, _(" Rename "), _("File exists"), exf == 'f' ? buf : name,
                          _(" Overwrite "), _(" Cancel "), 0))
                return;
        } else if (exf == 'd') {
            error(_("Directory %s exists"), buf);
            return;
        }
        if (fileop(FO_MOVE, p->name, name)) {
            error(_("Cannot move %s to %s"), p->name, name);
            return;
        }
    } else if (c == S_IFREG) {
        /* move regular file */

        sprintf(buf, "Rename or move \"%s\" to", p->name);
        if (!(name = getstring(60, (cur == &left ? &right : &left)->cwd, _(" Rename "), buf)))
            return;
        exd = exist(name);
        sprintf(buf, "%s/%s", name, p->name);
        exf = exd == 'd' ? exist(buf) : 0;
        if (exd == 'f' || exf == 'f') {
            if (getchoice(1, _(" Rename "), _("File exists"), exf == 'f' ? buf : name,
                          _(" Overwrite "), _(" Cancel "), 0))
                return;
        } else if (exf == 'd') {
            error(_("%s is a directory"), buf);
            return;
        }
        if (fileop(FO_MOVE, p->name, name)) {
            error(_("Cannot move %s to %s"), p->name, name);
            return;
        }
    }
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
    if (name)
        findfile(cur, name);
}

void makedir()
{
    register char *p;

    if (!(p = getstring(60, 0, _(" Make directory "), _("Create the directory"))))
        return;
    switch (exist(p)) {
    default:
    case 'f':
        error(_("File %s exists"), p);
        break;
    case 'd':
        error(_("Directory %s already exists"), p);
        break;
    case 0:
        if (mkdir(p, 0777) < 0) {
            error(_("Cannot create directory %s"), p);
            break;
        }
        setdir(cur == &left ? &right : &left, 0);
        setdir(cur, 0);
        findfile(cur, p);
        break;
    }
}

void delete()
{
    char buf[80];
    register struct file *p;
    register int c;

    c = tagged();
    if (c) {
        /* delete group of files */

        sprintf(buf, P_("You have selected %d file.", "You have selected %d files.", c), c);
        if (getchoice(0, _(" Delete "), buf, 0, _(" Delete "), _(" Cancel "), 0))
            return;
        sprintf(buf,
                P_("You are DELETING %d selected file from",
                   "You are DELETING %d selected files from", c),
                c);
        if (getchoice(1, _(" Delete "), buf, cur->cwd, _(" Ok "), _(" Cancel "), 0))
            return;
        for (p = cur->cat; p < cur->cat + cur->num; ++p)
            if (p->tag && unlink(p->name) < 0)
                error(_("Cannot delete %s"), p->name);
    } else if ((c = (p = &cur->cat[cur->curfile])->mode & S_IFMT) == S_IFDIR) {
        /* delete directory */

        if (getchoice(0, _(" Delete "), _("Do you wish to delete directory"), p->name,
                      _(" Delete "), _(" Cancel "), 0))
            return;
        if (rmdir(p->name) < 0) {
            error(_("Cannot delete directory %s"), p->name);
            return;
        }
    } else if (c == S_IFREG) {
        /* delete regular file */

        if (getchoice(0, _(" Delete "), _("Do you wish to delete"), p->name, _(" Delete "),
                      _(" Cancel "), 0))
            return;
        if (unlink(p->name) < 0) {
            error(_("Cannot delete %s"), p->name);
            return;
        }
    }
    setdir(cur == &left ? &right : &left, 0);
    setdir(cur, 0);
}

void findname()
{
    register char *s, *q;
    register char *p;
    register struct file *f;

    if (!(p = getwstring(20, "", _(" Find file "), _("Enter file name"))) || !*p)
        return;
    for (f = cur->cat; f < cur->cat + cur->num; ++f) {
        s = p;
        q = f->name;
        for (;;) {
            if (!*s) {
                cur->curfile = f - cur->cat;
                if (cur->topfile > cur->curfile || cur->topfile + PAGELEN(cur) <= cur->curfile)
                    cur->topfile = cur->curfile - PAGELEN(cur) + 1;
                while (cur->topfile + PAGELEN(cur) - H > cur->curfile)
                    cur->topfile -= H;
                if (cur->topfile < 0)
                    cur->topfile = 0;
                return;
            } else if (*s != *q)
                break;
            ++s;
            ++q;
        }
    }
    error(_("File not found"));
}

void tagall()
{
    register char *p;
    register struct file *f;
    static char pat[PATSZ];

    if (!pat[0])
        strcpy(pat, "*");
    if (!(p = getwstring(20, pat, _(" Select "), _("Select the files"))))
        return;
    snprintf(pat, sizeof(pat), "%s", p);
    for (f = cur->cat; f < cur->cat + cur->num; ++f)
        if ((f->mode & S_IFMT) == (unsigned)S_IFREG && (!pat[0] || match(f->name, pat)))
            f->tag = 1;
    counttag(cur);
}

void untagall()
{
    register char *p;
    register struct file *f;
    static char pat[PATSZ];

    if (!pat[0])
        strcpy(pat, "*");
    if (!(p = getwstring(20, pat, _(" Unselect "), _("Unselect the files"))))
        return;
    snprintf(pat, sizeof(pat), "%s", p);
    for (f = cur->cat; f < cur->cat + cur->num; ++f)
        if ((f->mode & S_IFMT) == (unsigned)S_IFREG && (!pat[0] || match(f->name, pat)))
            f->tag = 0;
    counttag(cur);
}

void setpattern(int k)
{
    struct dir *d = k == 'l' ? &left : &right;
    register char *p;

    if (!(p = getwstring(20, d->pattern, _(" Set pattern "), _("Set pattern for files"))))
        return;
    snprintf(d->pattern, PATSZ, "%s", p);
    setdir(d, 0);
    chdir(cur->cwd);
}

void cmpdir()
{
    register struct file *f, *of;
    register struct dir *d, *od;
    struct file *end, *oend;
    int cmp;

    d    = &left;
    od   = &right;
    end  = d->cat + d->num;
    oend = od->cat + od->num;
    for (of = od->cat; of < oend; ++of)
        if ((of->mode & S_IFMT) == (unsigned)S_IFREG)
            of->tag = 1;
    for (f = d->cat; f < end; ++f)
        if ((f->mode & S_IFMT) == (unsigned)S_IFREG)
            f->tag = 1;
    for (f = d->cat, of = od->cat; of < oend; ++of) {
        while ((cmp = compfile(f, of)) < 0)
            if (++f >= end)
                goto breakloop;
        if (!of->tag || !f->tag)
            continue;
        if (cmp || f->size != of->size)
            continue;
        if (cmpfil(d->cwd, f->name, od->cwd, of->name))
            f->tag = of->tag = 0;
    }
breakloop:
    counttag(d);
    counttag(od);
}

static int cmpfil(char *d1, char *f1, char *d2, char *f2)
{
    register int i;
    int fd1, fd2;
    char buf1[512], buf2[512];
    int n1, n2, rez;

    sprintf(buf1, "%s/%s", d1, f1);
    sprintf(buf2, "%s/%s", d2, f2);
    if ((fd1 = open(buf1, 0)) < 0)
        return (0);
    if ((fd2 = open(buf2, 0)) < 0) {
        close(fd1);
        return (0);
    }
    rez = 0;
    for (;;) {
        n1 = read(fd1, buf1, sizeof(buf1));
        n2 = read(fd2, buf2, sizeof(buf2));
        if (n1 < 0 || n2 < 0 || n1 != n2)
            break;
        if (n1 == 0) {
            rez = 1;
            goto breakloop;
        }
        for (i = 0; i < n1; ++i)
            if (buf1[i] != buf2[i])
                goto breakloop;
    }
breakloop:
    close(fd1);
    close(fd2);
    return (rez);
}

void syscmd(char *s)
{
    register int status, sig;

#ifdef __GONWARE__
    /* the shell has the commands (DIR, COPY ...) and finds the programs */
    runl(0, shabsname, shname, SHELL_C, s, 0);
    return;
#endif
    if (metas(s)) {
        if (usecshell)
            runl(0, cshabsname, cshname, "-f", "-c", s, 0);
        else
            runl(0, shabsname, shname, "-c", s, 0);
        return;
    }
    status = doexec(s);
    if (status < 0) {
        TtyReset();
        outerr("\n[%d] %s\n", -status, s);
        jobcmd(-status, s);
        return;
    }
    sig = status & 0177;
    if (!sig)
        return;
    if (sig == 0177)
        outerr("ptrace: ");
    else if (sig < numsysmsg && sysmsg[sig])
        outerr(sysmsg[sig]);
    else
        outerr("Signal %d", sig);
    if (status & 0200)
        outerr(" - core dumped\n");
    else
        outerr("\n");
}

/*
 * Are there are any Shell meta-characters?
 */
static int metas(char *s)
{
    while (*s)
        switch (*s++) {
        case '|':
        case '^':
        case ';':
        case '&':
        case '(':
        case ')':
        case '<':
        case '>':
        case '[':
        case ']':
        case '*':
        case '?':
        case '\'':
        case '\"':
        case '\\':
        case '`':
        case '$':
        case '~':
            /* case '\n': case '=': case ':': */
            return (1);
        }
    return (0);
}

int doexec(char *str)
{
    register char *t;
    char *argv[MAXARGV + 1];
    register char **p;
    int status;

    while (*str == ' ' || *str == '\t')
        ++str;
    if (!*str)
        return (-1); /* no command */
    t = malloc(strlen(str) + 1);
    strcpy(t, str);
    str = t;
    p   = argv;
    for (t = str; *t;) {
        if (p >= argv + MAXARGV) {
            error(_("%s: Too many arguments"), str);
            break;
        }
        *p++ = t;
        while (*t && *t != ' ' && *t != '\t')
            ++t;
        if (*t)
            for (*t++ = 0; *t == ' ' || *t == '\t'; ++t)
                ;
    }
    *p     = 0;
    status = runv(0, str, argv);
    free(str);
    return (status);
}

/*
 * The drives to choose a panel's directory from: on GDOS the volumes
 * with a drive letter, on Unix the mounted file systems on a device and
 * the home directory.
 */
#define MAXDRIVES 32

struct drive {
    char path[256];
    char text[320];
};

#ifndef __GONWARE__
/* a path of /proc/mounts: \040 for a space and so on */
static void unoctal(char *s)
{
    char *d = s;

    for (; *s; ++s)
        if (s[0] == '\\' && s[1] >= '0' && s[1] <= '3' && s[2] >= '0' && s[3] >= '0') {
            *d++ = (s[1] - '0') << 6 | (s[2] - '0') << 3 | (s[3] - '0');
            s += 3;
        } else
            *d++ = *s;
    *d = 0;
}
#endif

static int getdrives(struct drive *dv, int max)
{
    int n = 0, i;
#ifdef __GONWARE__
    struct gdos_volume v;
    long k;
    struct drive t;

    for (k = 0; n < max && gdos_call2(GDOS_VOL_LIST, k, &v) > 0; ++k) {
        if (!v.letter)
            continue; /* an EFI system partition without a letter */
        snprintf(dv[n].path, sizeof(dv[n].path), "%c:\\", (int)v.letter);
        snprintf(dv[n].text, sizeof(dv[n].text), "%c:  %-6.8s %.12s", (int)v.letter, v.type,
                 v.label);
        ++n;
    }
    for (i = 1; i < n; ++i) /* by letter */
        for (k = i; k > 0 && dv[k - 1].path[0] > dv[k].path[0]; --k)
            t = dv[k], dv[k] = dv[k - 1], dv[k - 1] = t;
#else
    char dev[256], dir[256], type[64];
    FILE *f = fopen("/proc/self/mounts", "r");

    if (!f)
        f = fopen("/etc/mtab", "r");
    while (f && n < max && fscanf(f, "%255s %255s %63s %*[^\n]", dev, dir, type) == 3) {
        if (dev[0] != '/' && strcmp(dir, "/"))
            continue; /* proc, sysfs, tmpfs ...: no device */
        unoctal(dir);
        for (i = 0; i < n && strcmp(dv[i].path, dir); ++i)
            ;
        if (i < n)
            continue;
        snprintf(dv[n].path, sizeof(dv[n].path), "%s", dir);
        snprintf(dv[n].text, sizeof(dv[n].text), "%-24s %s", dir, type);
        ++n;
    }
    if (f)
        fclose(f);
    if (home && n < max) {
        snprintf(dv[n].path, sizeof(dv[n].path), "%s", home);
        snprintf(dv[n].text, sizeof(dv[n].text), "%-24s ~", home);
        ++n;
    }
#endif
    return n;
}

/* Left/Right Drive (Alt-F1, Alt-F2): the panel to the root of a drive */
void setdrive(int k, int sk)
{
    struct dir *d = k == 'l' ? &left : &right;
    static struct drive dv[MAXDRIVES];
    char *items[MAXDRIVES];
    int n, i, sel = 0, best = -1, l;

    n = getdrives(dv, MAXDRIVES);
    if (!n)
        return;
    for (i = 0; i < n; ++i) {
        items[i] = dv[i].text;
        l        = strlen(dv[i].path);
        /* the drive the panel is on: the longest path its directory starts with */
#ifdef __GONWARE__
        if ((d->cwd[0] | 040) == (dv[i].path[0] | 040) && l > best)
#else
        if (!strncmp(d->cwd, dv[i].path, l) && (d->cwd[l] == '/' || !d->cwd[l] || l == 1) &&
            l > best)
#endif
            best = l, sel = i;
    }
    i = getlistat(_(" Drive "), items, n, sel, widewin ? COLS / 2 : d->basecol + PAGEWID / 2);
    if (i < 0)
        return;
    if (chdir(dv[i].path) < 0)
        error(_("Cannot chdir to %s"), dv[i].path);
    else
        setdir(d, ".");
    chdir(cur->cwd);
}
