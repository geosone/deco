#include <errno.h>
#include <time.h>
/* width of directory window */

/* The panels follow the width of the screen: each is half of it (40 of
 * 80 columns, side by side without a gap), or all of it in the wide
 * window mode. */
extern int LINES, COLS; /* scr.c */
#define PANELWID (COLS / 2)
#define PAGEWID  (widewin ? COLS : PANELWID)

/* number of displayable files in directory window */

#define PAGELEN(c) pagelen((c)->view)

#define BASECOL(c) (widewin ? 0 : (c)->basecol)

void drawkeys(const char *const labels[10]); /* F1...F10 bar */
void initcolors(void);                       /* the VC colour scheme */
int ncolumns(int view);                      /* columns of names in a panel in this view */
int colsep(int k, int view); /* column separator k (0 ... ncolumns) from the panel's left frame */

/* The GonWare Commander, grown from the Demos Commander (MIT): keep its
 * copyright next to ours wherever ours is shown. */
#define CMDR_NAME      "GonWare Commander"
#define CMDR_VERSION   "4.0"
#define CMDR_COPYRIGHT "Copyright (C) 2026 Mario Fetka"
#define DECO_ORIGIN    "based on the Demos Commander, Copyright (C) 1989-2000 Serge Vakulenko"
#define CMDR_SHORTCOPY "(C) 2026 M.Fetka, 1989-2000 S.Vakulenko" /* where room is short */

/* translations (intl.c): _() for a text, P_() for one with a count, N_()
 * marks a text translated where it is used */
char *tr(const char *msgid);
char *ntr(const char *msgid, const char *plural, unsigned long n);
void trinit(void);
struct trlang {
    const char *code, *name;
};
extern const struct trlang trlangs[];
void trsetlang(const char *code);
const char *trgetlang(void);
int trchosen(void);
void menuretranslate(void);
void setlang_help(const char *code);
void helpreset(void);
int getlist(char *head, char **items, int n, int sel);
#define _(s)        tr(s)
#define P_(s, p, n) ntr(s, p, (unsigned long)(n))
#define N_(s)       (s)
/* a text in a context, where the same English needs another translation
 * (the short labels of the F-key bar) */
char *trc(const char *ctx, const char *msgid);

/* UTF-8: a lead byte, a continuation byte */
#define UTF8LEAD(c) (((c) & 0300) == 0300)
#define UTF8CONT(c) (((c) & 0300) == 0200)
int utf8len(const char *p, const char *e);
char *viewutf8(char *p, char *e);
#define NC_(c, s) (s)

#define CMDLEN 512 /* length of command string */

struct dir;
struct file;
struct menu;

extern struct dir left, right;            /* left and right directories */
extern struct dir *cur;                   /* current directory */
extern int widewin;                       /* wide window mode */
extern int H;                             /* file window height */
extern int cmdreg;                        /* command line mode */
extern char command[];                    /* command line */
extern int cpos;                          /* command line cursor */
extern char *user;                        /* user name */
extern char *group;                       /* user group name */
extern char *tty;                         /* tty name */
extern char *machine;                     /* machine name */
extern int visualwin;                     /* cursor in command line */
extern int showhidden;                    /* show hidden files ".*" */
extern int uid;                           /* real user id */
extern int main_pid;                      /* our process id */
extern int ppid;                          /* parent id */
extern int lang;                          /* language */
extern char editname[160], viewname[160]; /* 40 cells of UTF-8 */
extern int useredit, userview, usecshell;
extern int viewraw, viewhex, viewtabs;
extern char *home;
extern struct palette palette, dflt_palette;

#define ENG 0 /* English */
#define RUS 1 /* Russian */
#define UKR 2 /* Ukranian */
#define DEU 3 /* German */
#define FRA 4 /* France */

#define VIEW_BRIEF 0
#define VIEW_LONG  1
#define VIEW_WIDE  2
#define VIEW_FULL  3
#define VIEW_INFO  4

struct palette {
    int fg, bg;
    int revfg, revbg;
    int boldfg, boldbg;
    int boldrevfg, boldrevbg;
    int dimfg, dimbg;
    int dimrevfg, dimrevbg;
};

/*
 * mcheck (type& array, type, int& bound, int quant, int index)
 *      Check index of dynamic array.
 *      Enlarge array if needed.
 */
#define mcheck(array, type, bound, quant, index) \
    if ((index) >= (bound))                      \
    (array) = (type)realloc(array, (int)((bound) += (quant)) * (int)sizeof(*(array)))

void error(char *s, ...);
void message(char *name, char *s, ...);
int setdir(struct dir *d, char *dirname);
void hidecursor(void);
int getchoice(int bold, char *head, char *msg, char *mesg2, char *c1, char *c2, char *c3);
char *username(int u);
char *groupname(int g);
void sigign(void);
void sigdfl(void);
void outerr(char *msg, ...);
int runv(int silent, char *name, char **a0);
char *getmachine(void);
const char *extension(const char *p);
char *ltoac(long l);
char *basename(char *p);
char *strtail(char *p, int delim, int maxlen);
int strbcmp(char *s, char *b);
int match(char *name, char *pat);
int getmenu(void);
void downmenu(struct menu *m);
char *timestr(time_t tim);
int hexview(char *filename);
void viewpage(int line, int col);
void viewchar(int c);
void viewline(int line, int col, int scrline);
int offset(int l, int n);
int column(int l, int n);
int search(char *str, int l, int c, int *pline, int *pcol);
void runhelp(char *name);
char *getstring(int w, char *str, char *head, char *mesg);
void endmesg(void);
char *mdup(char *s);
int pagelen(int f);
char *getwstring(int w, char *str, char *head, char *mesg);
void syscmd(char *s);
void viewfile(int d, char *filename);
void editfile(int d, char *filename, char *basename);
void quitdeco(void);
int doexec(char *str);
void runset(char *dest);
void runarg(char *name);
int rundone(char *name, char *a0, char *a1, char *a2);

/* copying, moving and linking files without external programs (run.c) */
#define FO_COPY    0 /* contents, mode and times */
#define FO_MOVE    1 /* rename(), or copy and delete to another file system */
#define FO_LINK    2
#define FO_SYMLINK 3
int fileop(int op, char *from, char *to); /* to: a name or a directory; 0 or -1 */
int filesdone(int op);                    /* the files collected with runset()/runarg() */
void runcancel(void);
int runl(int silent, char *name, ...);
void findfile(struct dir *d, char *name);
int compfile(const void *arg1, const void *arg2);
int exist(char *name);
void counttag(struct dir *d);
void rightcmd(void);
void mvcaddstr(int r, int c, char *s);
void inscmd(int key);
int quote(void);
void fullscreen(int k, int sk);
int tagged(void);
void draw(void);
void drawbanners(void);
int filetype(struct file *f);
char *filemode(int m);
void setinfo(struct file *f, struct dir *d);
char *filesize(struct file *f);
char *filedate(long d);
void execmd(int hit, int savhist);
void directory(int k, int sk);
void switchcmdreg(void);
void switchpanels(void);
void setstatus(void);
void reread(int k, int sk);
void setdwid(void);
void view(void);
void edit(void);
void menuedit(void);
void extedit(void);
void quit(void);
void swappanels(void);
void copy(void);
void renmove(void);
void makedir(void);
void delete (void);
void findname(void);
void tagall(void);
void untagall(void);
void drawcmd(void);
void namecmd(void);
void delcmd(void);
void homecmd(void);
void endcmd(void);
void leftcmd(void);
void upcmd(void);
void downcmd(void);
void nextcmd(void);
void prevcmd(void);
void histmenu(void);
void drawdir(struct dir *c, int statusflag);
void drawcursor(void);
void undrawcursor(void);
void excommand(char *cp, char *fname);
void usermenu(void);
void readinitfile(void);
void genhelp(void);
int hexview(char *filename);
void runmenu(int mkey);
void jobcmd(int pid, char *cmd);
void sigchild(int sig);
int mygroup(int gid);
