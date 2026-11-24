/* menu.h needs BOX from scr.h */
/* clang-format off */
#include "deco.h"
#include "dir.h"
#include "scr.h"
#include "menu.h"
/* clang-format on */

/* menu handler */

menufunc setfull, reread, directory, setpattern, makelink, viewinfo, editinfo;
menufunc setlang, redraw, shell, cmpdir, fullscreen, setshellname, setviewname;
menufunc seteditname, writeinitfile, switchalign, setsort, setrevsort;
menufunc settypesort, switchhidden, setdrive;

/* the keys of Left/Right Drive: Shift, as Alt-F1/Alt-F2 are often taken
 * (GonWare's screens, KDE's menu and KRunner); Alt-F1/Alt-F2 work too */
#define DRIVEKEY_L "S-F1"
#define DRIVEKEY_R "S-F2"

#ifdef S_IFLNK
menufunc makeslink;
#endif

static struct submenu lmenu[] = {
    // clang-format off
    { N_("Brief"), 0,                'b',  setfull },
    { N_("Long"), 0,                 'l',  setfull },
    { N_("Wide"), 0,                 'w',  setfull },
    { N_("Full"), 0,                 'f',  setfull },
    { N_("Info"), 0,                 'i',  setfull },
    { N_("Status"), "^L",  's',  (menufunc*)setstatus },
    { N_("Align extensions"), 0,     'a',  switchalign },
    { "", 0,                      0,    0 },
    { N_("Sort by name"), 0,         'n',  setsort },
    { N_("Extension"), 0,            'x',  setsort },
    { N_("Time"), 0,                 't',  setsort },
    { N_("Size"), 0,                 'z',  setsort },
    { N_("Unsorted"), 0,             'u',  setsort },
    { N_("Reverse"), 0,              'r',  setrevsort },
    { N_("Sort by type"), 0,         'y',  settypesort },
    { "", 0,                      0,    0 },
    { N_("Re-read"), "^R",  'r',  reread },
    { N_("Pattern"), 0,              'p',  setpattern },
    { N_("Drive..."), DRIVEKEY_L,    'd',  setdrive },
    { 0,                      0,    0 },
    // clang-format on
};

static struct submenu rmenu[] = {
    // clang-format off
    { N_("Brief"), 0,                'b',  setfull },
    { N_("Long"), 0,                 'l',  setfull },
    { N_("Wide"), 0,                 'w',  setfull },
    { N_("Full"), 0,                 'f',  setfull },
    { N_("Info"), 0,                 'i',  setfull },
    { N_("Status"), "^L",  's',  (menufunc*)setstatus },
    { N_("Align extensions"), 0,     'a',  switchalign },
    { "", 0,                      0,    0 },
    { N_("Sort by name"), 0,         'n',  setsort },
    { N_("Extension"), 0,            'x',  setsort },
    { N_("Time"), 0,                 't',  setsort },
    { N_("Size"), 0,                 'z',  setsort },
    { N_("Unsorted"), 0,             'u',  setsort },
    { N_("Reverse"), 0,              'r',  setrevsort },
    { N_("Sort by type"), 0,         'y',  settypesort },
    { "", 0,                      0,    0 },
    { N_("Re-read"), "^R",  'r',  reread },
    { N_("Pattern"), 0,              'p',  setpattern },
    { N_("Drive..."), DRIVEKEY_R,    'd',  setdrive },
    { 0,                      0,    0 },
    // clang-format on
};

static struct submenu fmenu[] = {
    // clang-format off
    { N_("Help"), "F1",  'h',  (menufunc*)genhelp },
    { N_("User menu"), "F2",  'u',  (menufunc*)usermenu },
    { N_("View"), "F3",  'v',  (menufunc*)view },
    { N_("View info"), 0,            'i',  viewinfo },
    { N_("Edit"), "F4",  'e',  (menufunc*)edit },
    { N_("Edit info"), 0,            'n',  editinfo },
    { N_("Copy"), "F5",  'c',  (menufunc*)copy },
    { N_("Link"), 0,                 'l',  makelink },
#ifdef S_IFLNK
    { N_("Symlink"), 0,              's',  makeslink },
#endif
    { N_("Rename/move"), "F6",  'r',  (menufunc*)renmove },
    { N_("Make directory"), "F7",  'm',  (menufunc*)makedir },
    { N_("Delete"), "F8",  'd',  (menufunc*)delete },
    { "", 0,                      0,    0 },
    { N_("Select group"), "+",   'g',  (menufunc*)tagall },
    { N_("Unselect group"), "-",   'u',  (menufunc*)untagall },
    { "", 0,                      0,    0 },
    { N_("Quit"), "F10", 'q',  (menufunc*)quit },
    { 0,                      0,    0 },
    // clang-format on
};

static struct submenu cmenu[] = {
    // clang-format off
    { N_("Run subshell"), 0,             's',  shell },
    { N_("Compare directories"), 0,      'c',  cmpdir },
    { N_("Find file"), "^K",  'f',  (menufunc*)findname },
    { N_("History"), "^B",  'i',  (menufunc*)histmenu },
    { "", 0,                          0,    0 },
    { N_("Home directory"), "^\\", 'o',  directory },
    { N_("Root directory"), "^^",  'r',  directory },
    { N_("Redraw screen"), "^]",  'd',  redraw },
    { "", 0,                          0,    0 },
    { N_("Menu file edit"), 0,           'm',  (menufunc*)menuedit },
    { N_("Extensions file edit"), 0,     'x',  (menufunc*)extedit },
    { 0,                          0,    0 },
    // clang-format on
};

static struct submenu omenu[] = {
    // clang-format off
    { N_("Switch panels"), "^I",  'w',  (menufunc*)switchpanels },
    { N_("Swap panels"), "^U",  's',  (menufunc*)swappanels },
    { N_("Full screen"), "^F",  'f',  fullscreen },
    { N_("Double width"), "^W",  'd',  (menufunc*)setdwid },
    { N_("Command line mode"), "^P",  'c',  (menufunc*)switchcmdreg },
    { N_("Show hidden files"), 0,      'n',  switchhidden },
    { "", 0,                        0,    0 },
    { N_("Language..."), 0,            'l',  setlang },
    { N_("Viewer..."), 0,              'v',  setviewname },
    { N_("Editor..."), 0,              'e',  seteditname },
    { N_("Shell..."), 0,               'h',  setshellname },
    { "", 0,                        0,    0 },
    { N_("Save setup"), 0,             'a',  writeinitfile },
    { 0,                        0,    0 },
    // clang-format on
};

struct menu menu[] = {
    // clang-format off
    { N_("Left"),         'l',  lmenu },
    { N_("Files"),        'f',  fmenu },
    { N_("Commands"),     'c',  cmenu },
    { N_("Options"),      'o',  omenu },
    { N_("Right"),        'r',  rmenu },
    { 0,              0,    0 },
    // clang-format on
};

int nmenu;

static int tag(int k, int sk);

void runmenu(int mkey)
{
    register struct menu *m;
    register struct submenu *sm;
    int ret;

    nmenu = 0;
    for (m = menu; m->name; ++m) {
        if (m->key == mkey)
            nmenu = m - menu;
        for (sm = m->submenu; sm->name; ++sm)
            sm->active = sm->exec != 0;
        m->nsm = -1;
        downmenu(m);
    }
oncemore:
    for (m = menu; m->name; ++m) {
        for (sm = m->submenu; sm->name; ++sm) {
            sm->active = sm->exec != 0;
            sm->tag    = tag(m->key, sm->key);
        }
    }
    ret = getmenu();
    if (!ret)
        return;
    m  = &menu[nmenu];
    sm = &m->submenu[m->nsm];
    if (sm->exec)
        (*sm->exec)(m->key, sm->key);
    if (ret > 1)
        goto oncemore;
}

static int tag(int k, int sk)
{
    switch (k) {
    case 'l':
        switch (sk) {
        case 'b':
            return (left.view == VIEW_BRIEF);
        case 'w':
            return (left.view == VIEW_WIDE);
        case 'l':
            return (left.view == VIEW_LONG || (!widewin && left.view == VIEW_FULL));
        case 'f':
            return (widewin && left.view == VIEW_FULL);
        case 'i':
            return (widewin && left.view == VIEW_INFO);
        case 's':
            return (right.status);
        case 'p':
            return (left.pattern[0]);
        case 'n':
            return (left.sort == SORTNAME);
        case 'x':
            return (left.sort == SORTEXT);
        case 't':
            return (left.sort == SORTTIME);
        case 'z':
            return (left.sort == SORTSIZE);
        case 'u':
            return (left.sort == SORTSKIP);
        case 'a':
            return (left.alignext);
        case 'y':
            return (left.typesort);
        case 'r':
            return (left.revsort);
        }
        break;
    case 'r':
        switch (sk) {
        case 'b':
            return (right.view == VIEW_BRIEF);
        case 'w':
            return (right.view == VIEW_WIDE);
        case 'l':
            return (right.view == VIEW_LONG || (!widewin && right.view == VIEW_FULL));
        case 'f':
            return (widewin && right.view == VIEW_FULL);
        case 'i':
            return (widewin && right.view == VIEW_INFO);
        case 's':
            return (left.status);
        case 'p':
            return (right.pattern[0]);
        case 'n':
            return (right.sort == SORTNAME);
        case 'x':
            return (right.sort == SORTEXT);
        case 't':
            return (right.sort == SORTTIME);
        case 'z':
            return (right.sort == SORTSIZE);
        case 'u':
            return (right.sort == SORTSKIP);
        case 'a':
            return (right.alignext);
        case 'y':
            return (right.typesort);
        case 'r':
            return (right.revsort);
        }
        break;
    case 'o':
        switch (sk) {
        case 's':
            return (left.basecol != 0);
        case 'f':
            return (H == LINES - 7);
        case 'h':
            return (usecshell);
        case 'd':
            return (widewin);
        case 'c':
            return (cmdreg);
        case 'v':
            return (userview);
        case 'e':
            return (useredit);
        case 'n':
            return (showhidden);
        }
        break;
    }
    return (0);
}
