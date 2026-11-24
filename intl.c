/*
 * Message catalogues of the GonWare Commander: gettext, text domain
 * "cmdr". gettext gives UTF-8; the screen cells hold 8 bits, so a
 * translation is kept in Latin-1 (S-Lang shows a cell of 0xe4 as U+00E4,
 * on GonWare and in a UTF-8 terminal); a character outside Latin-1
 * becomes '?' until the cells take Unicode (with UTF-8 file names).
 */
#include <libintl.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deco.h"
#include "env.h"

struct trtext {
    const char *key; /* the text gettext returned */
    char *latin1;
    struct trtext *next;
};

static struct trtext *trtab[256];

/* UTF-8 to Latin-1, '?' for what Latin-1 lacks */
static char *latin1(const char *u)
{
    const unsigned char *p = (const unsigned char *)u;
    char *s = malloc(strlen(u) + 1), *d = s;
    unsigned long c;
    int n;

    if (!s)
        return (char *)u;
    while (*p) {
        if (*p < 0x80) {
            *d++ = *p++;
            continue;
        }
        n = *p >= 0xf0 ? 3 : *p >= 0xe0 ? 2 : *p >= 0xc0 ? 1 : 0;
        c = *p++ & (0x3f >> n);
        for (; n > 0 && (*p & 0xc0) == 0x80; --n)
            c = c << 6 | (*p++ & 0x3f);
        *d++ = c < 0x100 ? (char)c : '?';
    }
    *d = 0;
    return s;
}

/* the Latin-1 form of a translation, made once */
static char *keep(const char *t)
{
    unsigned h = (unsigned)(((uintptr_t)t >> 3) & 0xff);
    struct trtext *e;

    for (e = trtab[h]; e; e = e->next)
        if (e->key == t)
            return e->latin1;
    e = malloc(sizeof(*e));
    if (!e)
        return (char *)t;
    e->key    = t;
    e->latin1 = latin1(t);
    e->next   = trtab[h];
    trtab[h]  = e;
    return e->latin1;
}

char *tr(const char *msgid)
{
    const char *t;

    if (!msgid || !*msgid)
        return (char *)msgid;
    t = gettext(msgid);
    return t == msgid ? (char *)msgid : keep(t);
}

/* pgettext: the context and the text, joined by EOT, as msgfmt stores them */
char *trc(const char *ctx, const char *msgid)
{
    char key[256];
    const char *t;

    if (!msgid || !*msgid)
        return (char *)msgid;
    snprintf(key, sizeof(key), "%s\004%s", ctx, msgid);
    t = gettext(key);
    return t == key ? (char *)msgid : keep(t);
}

char *ntr(const char *msgid, const char *plural, unsigned long n)
{
    const char *t = ngettext(msgid, plural, n);

    return t == msgid || t == plural ? (char *)t : keep(t);
}

/* the languages to choose from: catalogue code and own name (UTF-8) */
const struct trlang trlangs[] = {
    { "en", "English" },
    { "de", "Deutsch" },
    { "fr",
      "Fran\xc3\xa7"
      "ais" },
    { "es", "Espa\xc3\xb1ol" },
    { "it", "Italiano" },
    { 0, 0 },
};

static char chosen[16]; /* the language chosen in the menu, "" none */

static void forgetall()
{
    struct trtext *e, *n;
    int i;

    for (i = 0; i < 256; ++i) {
        for (e = trtab[i]; e; e = n) {
            n = e->next;
            if (e->latin1 != e->key)
                free(e->latin1);
            free(e);
        }
        trtab[i] = 0;
    }
}

/* another language now: LANGUAGE for gettext (and the programs started
 * from here); the texts kept from the last are forgotten, the catalogue
 * they came from may be gone */
void trsetlang(const char *code)
{
#ifdef LC_MESSAGES
    const char *l = setlocale(LC_MESSAGES, 0);

    /* glibc ignores LANGUAGE in the "C" locale */
    if (l && (!strcmp(l, "C") || !strcmp(l, "POSIX")))
        setlocale(LC_MESSAGES, "C.UTF-8");
#endif
    snprintf(chosen, sizeof(chosen), "%s", code);
    setenv("LANGUAGE", code, 1);
    EnvPut("LANGUAGE", (char *)code);
    forgetall();
    textdomain("cmdr"); /* glibc: drop the cached translations */
}

/* the language now: the one chosen, else the first of the environment */
const char *trgetlang()
{
    static char now[16];
    const char *v;

    if (chosen[0])
        return chosen;
    if (!((v = getenv("LANGUAGE")) && *v) && !((v = getenv("LC_ALL")) && *v) &&
        !((v = getenv("LC_MESSAGES")) && *v))
        v = getenv("LANG");
    snprintf(now, sizeof(now), "%.2s", v && *v ? v : "en");
    return now;
}

/* a text of the program (not of a catalogue) in Latin-1 */
char *trlatin1(const char *s)
{
    return keep(s);
}

int trchosen()
{
    return chosen[0] != 0;
}

void trinit()
{
    char *dir = getenv("TEXTDOMAINDIR"); /* gettext's way to point elsewhere */

#ifdef LC_MESSAGES
    setlocale(LC_MESSAGES, "");
#endif
#ifdef CMDR_LOCALEDIR
    if (!dir || !*dir)
        dir = CMDR_LOCALEDIR;
#endif
    if (dir && *dir)
        bindtextdomain("cmdr", dir);
    bind_textdomain_codeset("cmdr", "UTF-8");
    textdomain("cmdr");
}
