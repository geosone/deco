#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* menu.h needs BOX from scr.h */
/* clang-format off */
#include "deco.h"
#include "scr.h"
#include "menu.h"
/* clang-format on */

static void drawhead(int nm);
static void clrmenu(struct menu *m);
static void drawmenu(struct menu *m);
static void upmenu(struct menu *m);
static int hotkey(int c, struct menu *m);
static int menuclick(struct menu *m);

/* the label of an item translated, its key (after a gap of spaces) kept
 * and aligned to the right of the widest label of the menu */
static void trmenu(struct menu *m)
{
    struct submenu *sm;
    char *s;
    int w = 0, n;

    if (!m->id)
        m->id = m->name;
    m->name = _(m->id);
    for (sm = m->submenu; sm->name; ++sm) {
        if (!sm->name[0])
            continue;
        if (!sm->id)
            sm->id = sm->name;
        else if (sm->key2)
            free(sm->name); /* the padded one of the last language */
        sm->name = _(sm->id);
        n        = VStrLen(sm->name) + (sm->key2 ? 2 + VStrLen(sm->key2) : 0);
        if (n > w)
            w = n;
    }
    for (sm = m->submenu; sm->name; ++sm) {
        if (!sm->name[0] || !sm->key2)
            continue;
        /* the label, spaces, the key at the right: w cells */
        n = w - VStrLen(sm->name) - VStrLen(sm->key2);
        s = malloc(strlen(sm->name) + n + strlen(sm->key2) + 1);
        if (!s)
            continue;
        strcpy(s, sm->name);
        memset(s + strlen(s), ' ', n);
        strcpy(s + strlen(sm->name) + n, sm->key2);
        sm->name = s;
    }
}

/* the language changed: translate the menus anew when shown next */
void menuretranslate()
{
    menu[0].len = 0;
}

static void initmenu()
{
    register struct menu *m;
    register struct submenu *sm;
    int col, w;

    for (m = menu; m->name; ++m)
        trmenu(m);
    col = 3;
    for (m = menu; m->name; ++m) {
        m->len = 4 + VStrLen(m->name);
        m->col = col;
        col += m->len;
        m->hgh = 0;
        m->wid = 0;
        for (sm = m->submenu; sm->name; ++sm) {
            sm->row = m->hgh;
            w       = 4 + VStrLen(sm->name);
            if (w > m->wid)
                m->wid = w;
            ++m->hgh;
        }
    }
}

static void getboxes()
{
    register struct menu *m;

    for (m = menu; m->name; ++m) {
        if (m->box)
            VFreeBox(m->box);
        m->box = VGetBox(1, m->col - 1, m->hgh + 3, m->wid + 4);
    }
}

int getmenu()
{
    if (!menu[0].len)
        initmenu();

    getboxes();
    {
        static const char *const keys[10] = { "", "", "", "", "",
                                              "", "", "", "", NC_("keybar", "Quit") };

        drawkeys(keys);
    }
    for (;;) {
        drawhead(nmenu);
        for (;;) {
            int c, k;

            drawmenu(&menu[nmenu]);
            hidecursor();
            VSync();
            c = KeyGet();
            switch (c) {
            default:
                k = hotkey(c, &menu[nmenu]);
                if (k) {
                    clrmenu(&menu[nmenu]);
                    return (k);
                }
                VBeep();
                continue;
            case cntrl(']'): /* redraw screen */
                VRedraw();
                continue;
            case cntrl('M'):
                clrmenu(&menu[nmenu]);
                return (1);
            case cntrl('J'):
                clrmenu(&menu[nmenu]);
                return (2);
            case cntrl('C'):
            case cntrl('['):
            case meta('J'): /* f0 */
                clrmenu(&menu[nmenu]);
                return (0);
            case meta('r'): /* right */
                clrmenu(&menu[nmenu]);
                if (!menu[++nmenu].name)
                    nmenu = 0;
                break;
            case meta('l'): /* left */
                clrmenu(&menu[nmenu]);
                if (--nmenu < 0) {
                    for (nmenu = 0; menu[nmenu].name; ++nmenu)
                        ;
                    --nmenu;
                }
                break;
            case MOUSEKEY:
                k = menuclick(&menu[nmenu]);
                if (k == -2) /* a line, an item not to be had */
                    continue;
                if (k < 0) /* another menu */
                    break;
                clrmenu(&menu[nmenu]);
                return (k);
            case meta('u'): /* up */
                upmenu(&menu[nmenu]);
                continue;
            case meta('d'): /* down */
                downmenu(&menu[nmenu]);
                continue;
            }
            break;
        }
    }
}

/* the key of the menu whose name is at column col of the top line, 0
 * for none */
int menuat(int col)
{
    struct menu *m;

    if (!menu[0].len)
        initmenu();
    for (m = menu; m->name; ++m)
        if (col >= m->col - 1 && col < m->col - 1 + m->len)
            return m->key;
    return 0;
}

/* a click (MouseRow, MouseCol) while menu m is open: 1 an item of it is
 * chosen (m->nsm), -1 another menu's name (nmenu), -2 a line or an
 * inactive item (nothing), 0 elsewhere: closed */
static int menuclick(struct menu *m)
{
    struct menu *o;
    int i = MouseRow - 2;

    if (MouseRow == 0) {
        for (o = menu; o->name; ++o)
            if (MouseCol >= o->col - 1 && MouseCol < o->col - 1 + o->len) {
                if (o == m)
                    return 0;
                clrmenu(m);
                nmenu = o - menu;
                return -1;
            }
        return 0;
    }
    if (i < 0 || i >= m->hgh || MouseCol < m->col + 1 || MouseCol > m->col + m->wid)
        return 0;
    if (!m->submenu[i].name[0] || !m->submenu[i].active)
        return -2;
    m->nsm = i;
    return 1;
}

static void drawhead(int nm)
{
    register int i, reverse;
    int oldc = VSetColor(VCOL_BAR);

    VSetDim();
    reverse = VStandOut();
    VMove(0, 0);
    for (i = 0; i < COLS; ++i)
        VPutChar(' ');
    VMove(0, 2);
    for (i = 0; menu[i].name; ++i) {
        if (i == nm) {
            VStandEnd();
            VSetBold();
        }
        VPutChar(' ');
        VPutChar(i == nm && !reverse ? '[' : ' ');
        VPutString(menu[i].name);
        VPutChar(i == nm && !reverse ? ']' : ' ');
        VPutChar(' ');
        if (i == nm) {
            VStandOut();
            VSetDim();
        }
    }
    VSetNormal();
    VStandEnd();
    VSetColor(oldc);
}

static void clrmenu(struct menu *m)
{
    VUngetBox(m->box);
}

/* s in w cells, its first letter that is the hot key (in either case)
 * highlighted */
static void puthot(const char *s, int w, int key, int active, const char *key2)
{
    int hot  = active && key > ' ' && key < 0177;
    int text = VStrLen(s) - (key2 ? VStrLen(key2) : 0); /* not in "^F" */
    const char *e;

    for (; w > 0; --w, --text) {
        if (!*s) {
            VPutChar(' ');
            continue;
        }
        e = VStrSkip(s, 1); /* one character, in UTF-8 */
        if (hot && text > 0 && e - s == 1 && ((unsigned char)*s | 040) == (key | 040)) {
            VSetBold();
            VPutChar(*s++);
            VSetNormal();
            hot = 0;
        } else
            while (s < e)
                VPutChar(*s++);
    }
}

static void drawmenu(struct menu *m)
{
    register int i, reverse;
    int oldc = VSetColor(VCOL_MENU);

    VClearBox(1, m->col - 1, m->hgh + 3, m->wid + 4);
    VDrawBox(1, m->col, m->hgh + 2, m->wid + 2);
    VSetDim();
    for (i = 0; m->submenu[i].name; ++i)
        if (!m->submenu[i].name[0]) {
            if (!VUnicode) {
                VHorLine(i + 2, m->col + 2, m->wid - 2);
                continue;
            }
            /* as VC: from frame to frame, single in the double one */
            VLine(i + 2, m->col + 1, m->wid, 0, LINE_SINGLE);
            VJoint(i + 2, m->col, LINE_DOUBLE, LINE_DOUBLE, 0, LINE_SINGLE);
            VJoint(i + 2, m->col + m->wid + 1, LINE_DOUBLE, LINE_DOUBLE, LINE_SINGLE, 0);
        }
    VSetNormal();
    for (i = 0; m->submenu[i].name; ++i) {
        if (!m->submenu[i].name[0])
            continue;
        reverse = 0;
        if (i == m->nsm)
            reverse = VStandOut();
        else if (!m->submenu[i].active)
            VSetDim();
        VMove(2 + i, m->col + 1);
        VPutChar(i == m->nsm && !reverse ? '[' : ' ');
        VPrint("%c ", m->submenu[i].tag ? '*' : ' ');
        puthot(m->submenu[i].name, m->wid - 4, m->submenu[i].key, m->submenu[i].active,
               m->submenu[i].key2);
        VPutChar(i == m->nsm && !reverse ? ']' : ' ');
        if (i == m->nsm)
            VStandEnd();
        else if (!m->submenu[i].active)
            VSetNormal();
    }
    VSetColor(oldc);
}

static void upmenu(struct menu *m)
{
    for (;;) {
        if (--m->nsm < 0) {
            for (m->nsm = 0; m->submenu[m->nsm].name; ++m->nsm)
                ;
            --m->nsm;
        }
        if (m->submenu[m->nsm].active)
            break;
    }
}

void downmenu(struct menu *m)
{
    for (;;) {
        if (!m->submenu[++m->nsm].name)
            m->nsm = 0;
        if (m->submenu[m->nsm].active)
            break;
    }
}

static int hotkey(int c, struct menu *m)
{
    register int nsm, ret;

    if (c >= 'A' && c <= 'Z') {
        c += 'a' - 'A';
        ret = 2;
    } else
        ret = 1;
    for (nsm = 0; m->submenu[nsm].name; ++nsm)
        if (c == m->submenu[nsm].key && m->submenu[nsm].active) {
            m->nsm = nsm;
            return (ret);
        }
    return (0);
}
