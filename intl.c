/*
 * Message catalogues of the GonWare Commander: gettext, text domain
 * "cmdr", in UTF-8 like the screen (scr.c: a cell holds a code point).
 */
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "deco.h"
#include "env.h"

char *tr(const char *msgid)
{
    if (!msgid || !*msgid)
        return (char *)msgid;
    return gettext(msgid);
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
    return t == key ? (char *)msgid : (char *)t;
}

char *ntr(const char *msgid, const char *plural, unsigned long n)
{
    return ngettext(msgid, plural, n);
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

/* another language now: LANGUAGE for gettext (and the programs started
 * from here) */
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

int trchosen()
{
    return chosen[0] != 0;
}

/* "locale" beside the program, where the build puts the catalogues: a
 * commander run from its build directory speaks the language too; NULL
 * if there is none (or no /proc/self/exe) */
static char *besideme(void)
{
    static char path[1024];
    struct stat st;
    char *p;
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 8);

    if (n <= 0)
        return 0;
    path[n] = 0;
    if (!(p = strrchr(path, '/')))
        return 0;
    strcpy(p + 1, "locale");
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode) ? path : 0;
}

void trinit()
{
    char *dir = getenv("TEXTDOMAINDIR"); /* gettext's way to point elsewhere */

#ifdef LC_MESSAGES
    setlocale(LC_MESSAGES, "");
#endif
    if (!dir || !*dir)
        dir = besideme();
#ifdef CMDR_LOCALEDIR
    if (!dir || !*dir)
        dir = CMDR_LOCALEDIR;
#endif
    if (dir && *dir)
        bindtextdomain("cmdr", dir);
    bind_textdomain_codeset("cmdr", "UTF-8");
    textdomain("cmdr");
}
