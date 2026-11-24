/*
 *      Record-oriented file access package.
 *      -------------------------------------
 *
 *      REC *RecOpen (int fd, int mode)
 *              - initialize Rec package for file fd,
 *              for writing if mode!=0.
 *
 *      int RecSave (REC *r, char *filename)
 *              - and write out changes,
 *              returns 0 if ok or -1 if cannot write file.
 *
 *      int RecClose (REC *r)
 *              - close Rec discarding changes.
 *
 *      RecBroken (REC *r)
 *              - if Rec has broken lines, return 1, else 0.
 *
 *      LINE *RecGet (REC *r, int linenum)
 *              - get line.
 *
 *      RecPut (LINE *l, int newlen)
 *              - put line.
 *
 *      RecInsLine (REC *r, int linenum)
 *              - insert empty line before line #linenum
 *
 *      RecDelLine (REC *r, int linenum)
 *              - delete line #linenum
 *
 *      RecInsChar (REC *r, int line, int off, int sym)
 *              - insert char into line
 *
 *      RecDelChar (REC *r, int line, int off)
 *              - delete char from line
 */

#include "rec.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "deco.h"

#define SCANSZ   512 /* a read of the scan */
#define TEMPCELL 16  /* minimal size of temp space */
#define QUANT    512
#define MAXLINE  1024 /* maximum length of unbroken line */
#define NOINDEX  (POOLSZ + 2)

#define BAKSUFFIX ".b"

static char mask[8] = {
    1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80,
};

static int lenbuf;
static char *scanbuf, *pbuf;
static int eoln;
static int MAXLEN = ((unsigned int)(int)-1 >> 1) / sizeof(struct index);

static int ffcopy(int from, int to)
{
    char buf[512], *p;
    register int n, k;

    for (;;) {
        n = read(from, buf, sizeof(buf));
        if (n < 0) {
            error(_("Cannot read file"));
            return (-1);
        }
        if (n == 0)
            return (0);
        p = buf;
        while (n > 0) {
            k = write(to, p, (unsigned)n);
            if (k <= 0) {
                error(_("Cannot write file"));
                return (-1);
            }
            n -= k;
            p += k;
        }
    }
}

static void tempfree(REC *r, long seek, int len) /* mark temp space free */
{
    register int i;

    len = (len + TEMPCELL - 1) / TEMPCELL;
    seek /= TEMPCELL;
    for (i = 0; i < len; ++i, ++seek)
        r->tmap[seek >> 3] &= ~mask[seek & 7];
}

static long tempbusy(REC *r, int len) /* search & mark temp space busy */
{
    register int i, n;
    register long seek;

    len = (len + TEMPCELL - 1) / TEMPCELL;
    for (seek = 0; seek < MAXTEMP; ++seek) {
        for (n = 0; n < len; ++n)
            if (r->tmap[seek >> 3] & mask[seek & 7])
                break;
        if (n >= len)
            break;
    }
    if (seek >= MAXTEMP) {
        error(_("No temp space"));
        return (0);
    }
    for (n = seek, i = 0; i < len; ++i, ++n)
        r->tmap[n >> 3] |= mask[n & 7];
    return (seek * TEMPCELL);
}

static long tempsave(REC *r, char *str, int len) /* save string in temp file, return seek */
{
    register long seek;

    if (!len)
        return (0);
    seek = tempbusy(r, len);
    if (lseek(r->tfd, seek, 0) < 0)
        error(_("Cannot lseek on writing temp file"));
    if (write(r->tfd, str, (unsigned)len) != len)
        error(_("Cannot write temporary file"));
    return (seek);
}

static int scanline(int fd)
{
    register int len;

    len  = 0;
    eoln = 0;
    for (;;) {
        if (pbuf >= &scanbuf[lenbuf]) {
            lenbuf = read(fd, pbuf = scanbuf, (unsigned)SCANSZ);
            if (lenbuf <= 0)
                return (len ? len : -1);
        }
        if (*pbuf++ == '\n') {
            eoln = 1;
            return (len);
        }
        if (++len >= MAXLINE)
            return (len);
    }
}

/*
 * The temporary file for the changed lines, unlinked at once: in $TMPDIR,
 * $TEMP or $TMP, else /tmp, else the current directory (GonWare has no
 * /tmp). A name of its own each time: two editors do not share it.
 */
static int tempfile(void)
{
    static const char *const env[] = { "TMPDIR", "TEMP", "TMP", 0 };
    const char *dirs[5], *d;
    char name[1024];
    int n = 0, i, fd;

    for (i = 0; env[i]; ++i)
        if ((d = getenv(env[i])) && *d)
            dirs[n++] = d;
    dirs[n++] = "/tmp";
    dirs[n++] = ".";
    for (i = 0; i < n; ++i) {
        snprintf(name, sizeof(name), "%s/recXXXXXX", dirs[i]);
        if ((fd = mkstemp(name)) >= 0) {
            unlink(name);
            return fd;
        }
    }
    return -1;
}

REC *RecOpen(int fd, int wmode)
{
    register REC *r;
    register struct index *x;
    register int i;

    r        = (REC *)malloc(sizeof(REC));
    r->fd    = fd;
    r->bakfd = -1;
    r->nobak = 0;
    if (wmode) {
        r->tfd = tempfile();
        if (r->tfd < 0) {
            error(_("Cannot open temporary file"));
            free(r);
            return (0);
        }
    } else
        r->tfd = -1;

    for (i = 0; i < TEMPSZ; ++i)
        r->tmap[i] = 0;

    for (i = 0; i < POOLSZ; ++i)
        r->map[i].busy = 0;

    lseek(r->fd, 0L, 0);
    r->broken = 0;
    r->len    = 0;
    r->lindex = 0;
    r->nindex = 0;
    r->size   = 0;
    scanbuf   = malloc(SCANSZ);
    pbuf      = scanbuf;
    lenbuf    = 0;
    for (;;) {
        mcheck(r->lindex, struct index *, r->nindex, QUANT, r->len + 1);
        x            = &r->lindex[r->len];
        x->seek      = r->size;         /* store seek of current line */
        x->poolindex = NOINDEX;         /* clear pool index of current line */
        x->flags     = 0;               /* clear flags */
        x->len       = scanline(r->fd); /* scan next line */
        if (x->len < 0)
            break;
        r->size += x->len; /* increment seek by length of line */
        if (eoln)
            ++r->size; /* \n at end of line */
        else {
            x->flags |= XNOEOLN; /* no end of line */
            ++r->broken;
        }
        if (++r->len >= MAXLEN) {
            error(_("file too long"));
            return (0);
        }
    }
    free(scanbuf);
    return (r);
}

void RecClose(REC *r)
{
    register int i;

    for (i = 0; i < POOLSZ; ++i)
        if (r->map[i].busy) {
            if (r->pool[i].len)
                free(r->pool[i].ptr);
            r->lindex[r->map[i].index].poolindex = NOINDEX;
            r->map[i].busy                       = 0;
        }
    free(r->lindex);
    if (r->tfd >= 0)
        close(r->tfd);
    if (r->bakfd >= 0)
        close(r->bakfd);
    free(r);
}

/* the lines into the file name; -1 when it cannot be written */
static int recwrite(REC *r, char *filename)
{
    register int i, fd, err = 0;
    register LINE *p;

    fd = creat(filename, 0664);
    if (fd < 0) {
        error(_("Cannot create %s"), filename);
        return (-1);
    }
    for (i = 0; i < r->len && !err; ++i) {
        p = RecGet(r, i);
        if (p->len && write(fd, p->ptr, (unsigned)p->len) != p->len)
            err = 1;
        else if (write(fd, "\n", 1) != 1)
            err = 1;
    }
    if (close(fd) < 0)
        err = 1;
    if (err) {
        error(_("Cannot write %s"), filename);
        return (-1);
    }
    return (0);
}

/*
 * The file saved; the first time the old one is kept as the name with
 * BAKSUFFIX, and the lines not yet read are read from there.
 */
int RecSave(REC *r, char *filename)
{
    char *bak;

    if (r->bakfd < 0 && !r->nobak) {
        bak = malloc(strlen(filename) + sizeof(BAKSUFFIX));
        if (!bak)
            return (-1);
        strcpy(bak, filename);
        strcat(bak, BAKSUFFIX);
        r->bakfd = creat(bak, 0600);
        if (r->bakfd < 0) {
            error(_("Cannot create %s"), bak);
            free(bak);
            return (-1);
        }
        lseek(r->fd, 0L, 0);
        if (ffcopy(r->fd, r->bakfd) < 0) {
            close(r->bakfd);
            r->bakfd = -1;
            unlink(bak);
            free(bak);
            return (-1);
        }
        close(r->bakfd);
        r->bakfd = open(bak, 0);
        if (r->bakfd < 0) {
            error(_("Cannot open %s"), bak);
            unlink(bak);
            free(bak);
            return (-1);
        }
        free(bak);
        close(r->fd);
        r->fd = r->bakfd;
    }
    return recwrite(r, filename);
}

/*
 * The file saved under another name: the one it was read from stays as
 * it is (the lines not yet read are read from there), and saving again
 * makes no backup of it under the new name.
 */
int RecSaveAs(REC *r, char *filename)
{
    if (recwrite(r, filename) < 0)
        return (-1);
    r->nobak = 1;
    return (0);
}

void RecBreak(REC *r)
{
    register int i;
    register struct index *x;

    x = r->lindex;
    for (i = 0; i < r->len; ++i, ++x)
        x->flags &= ~XNOEOLN;
}

static void readline(int fd, long seek, int len, LINE *rez)
{
    register int l, n;
    register char *s;

    rez->len = len;
    rez->mod = 0;
    if (!len) {
        rez->ptr = "";
        return;
    }
    rez->ptr = malloc(rez->len);
    if (lseek(fd, seek, 0) < 0)
        error(_("Cannot lseek on reading fd=%d, seek=%ld"), fd, seek);
    for (l = rez->len, s = rez->ptr; l > 0; l -= n, s += n) {
        n = read(fd, s, (unsigned)l);
        if (n <= 0) {
            error(_("Cannot read line"));
            if (rez->len)
                free(rez->ptr);
            rez->len = 0;
            rez->ptr = "";
            return;
        }
    }
}

static int freeline(REC *r)
{
    register struct map *m;
    register struct index *x;
    register LINE *l;
    register int mintime, minindex;

    /* find free place in pool */
    for (m = r->map; m < r->map + POOLSZ; ++m)
        if (!m->busy)
            return (m - r->map);
    /* pool is full; find the oldest line */
    mintime  = r->map[0].time;
    minindex = 0;
    for (m = r->map; m < r->map + POOLSZ; ++m)
        if (m->time < mintime)
            minindex = m - r->map;
    m = &r->map[minindex];
    l = &r->pool[minindex];
    x = &r->lindex[m->index];
    /* remove line from pool */
    if (l->mod) { /* line is modified, save it in temp file */
        if ((x->flags & XTEMP) && l->oldlen)
            tempfree(r, x->seek, l->oldlen);
        x->seek = tempsave(r, l->ptr, l->len);
        x->len  = l->len;
        x->flags |= XTEMP;
    }
    if (l->len)
        free(l->ptr);
    x->poolindex = NOINDEX;
    m->busy      = 0;
    return (minindex);
}

LINE *RecGet(REC *r, int n)
{
    register struct index *x;
    register struct map *m;
    register LINE *p;
    static long timecount = 1; /* time stamp */

    if (n < 0 || n >= r->len)
        return (0);
    x = &r->lindex[n];
    if (x->poolindex != NOINDEX) { /* line is in cache */
        r->map[x->poolindex].time = ++timecount;
        return (&r->pool[x->poolindex]);
    }
    x->poolindex = freeline(r); /* get free pool index */
    p            = &r->pool[x->poolindex];
    m            = &r->map[x->poolindex];
    m->time      = ++timecount;
    m->index     = n;
    m->busy      = 1;
    /* read line from file */
    readline((x->flags & XTEMP) ? r->tfd : r->fd, x->seek, x->len, p);
    p->noeoln = (x->flags & XNOEOLN) != 0;
    return (p);
}

/* the length of line n without reading it */
int RecLen(REC *r, int n)
{
    struct index *x;

    if (n < 0 || n >= r->len)
        return (0);
    x = &r->lindex[n];
    return x->poolindex != NOINDEX ? r->pool[x->poolindex].len : x->len;
}

void RecPut(LINE *p, int newlen)
{
    if (!p->mod)
        p->oldlen = p->len;
    p->mod = 1;
    p->len = newlen;
}

void RecDelChar(REC *r, int line, int off)
{
    register LINE *p;
    char *s;

    if (!(p = RecGet(r, line)))
        return;
    if (p->len <= off)
        return;
    s = malloc(p->len - 1);
    if (off)
        memcpy(s, p->ptr, off);
    if (off <= p->len - 1)
        memcpy(s + off, p->ptr + off + 1, p->len - off - 1);
    free(p->ptr);
    p->ptr = s;
    RecPut(p, p->len - 1);
}

void RecInsChar(REC *r, int line, int off, int sym)
{
    register LINE *p;
    char *s;

    if (!(p = RecGet(r, line)))
        return;
    s = malloc(p->len + 1);
    if (off)
        memcpy(s, p->ptr, off);
    s[off] = sym;
    if (off < p->len)
        memcpy(s + off + 1, p->ptr + off, p->len - off);
    if (p->len)
        free(p->ptr);
    p->ptr = s;
    RecPut(p, p->len + 1);
}

void RecInsLine(REC *r, int n)
{
    register struct index *x, *i;
    register struct map *m;
    register int k;

    if (n < 0 || n > r->len)
        return;
    ++r->len;
    mcheck(r->lindex, struct index *, r->nindex, QUANT, r->len + 1);
    i = &r->lindex[n];
    for (x = &r->lindex[r->len - 1]; x > i; --x)
        x[0] = x[-1];
    i->seek      = 0;
    i->len       = 0;
    i->poolindex = NOINDEX;
    i->flags     = XTEMP;
    m            = r->map;
    for (k = 0; k < POOLSZ; ++k, ++m)
        if (m->index >= n)
            ++m->index;
}

void RecDelLine(REC *r, int n)
{
    register struct index *x, *i;
    register struct map *m;
    register LINE *l;
    register int k;

    if (n < 0 || n >= r->len)
        return;
    x = &r->lindex[n];
    m = r->map;
    if (x->poolindex != NOINDEX) { /* exclude line from pool */
        l = &r->pool[x->poolindex];
        if (x->flags & XTEMP) {
            if (l->mod) {
                if (l->oldlen)
                    tempfree(r, x->seek, (long)l->oldlen);
            } else if (l->len) {
                tempfree(r, x->seek, l->len);
            }
        }
        if (l->len) {
            free(l->ptr);
        }
        m[x->poolindex].busy = 0;
    }
    for (k = 0; k < POOLSZ; ++k, ++m)
        if (m->index > n)
            --m->index;
    i = &r->lindex[r->len - 1];
    for (x = &r->lindex[n]; x < i; ++x)
        x[0] = x[1];
    --r->len;
}
