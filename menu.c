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
        n        = strlen(sm->name) + (sm->key2 ? 2 + strlen(sm->key2) : 0);
        if (n > w)
            w = n;
    }
    for (sm = m->submenu; sm->name; ++sm) {
        if (!sm->name[0] || !sm->key2)
            continue;
        s = malloc(w + 1);
        if (!s)
            continue;
        sprintf(s, "%-*s%s", w - (int)strlen(sm->key2), sm->name, sm->key2);
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
        m->len = 4 + strlen(m->name);
        m->col = col;
        col += m->len;
        m->hgh = 0;
        m->wid = 0;
        for (sm = m->submenu; sm->name; ++sm) {
            sm->row = m->hgh;
            w       = 4 + strlen(sm->name);
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
    int text = strlen(s) - (key2 ? strlen(key2) : 0); /* not in "^F" */
    unsigned char c;

    for (; w > 0; --w, --text, s += *s != 0) {
        c = *s ? (unsigned char)*s : ' ';
        if (hot && *s && text > 0 && (c | 040) == (key | 040)) {
            VSetBold();
            VPutChar(c);
            VSetNormal();
            hot = 0;
        } else
            VPutChar(c);
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
        if (!m->submenu[i].name[0])
            VHorLine(i + 2, m->col + 2, m->wid - 2);
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
