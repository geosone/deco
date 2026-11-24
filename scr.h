#ifndef cntrl
#define cntrl(c) (c & 037)
#endif
#ifndef meta
#define meta(c) (c | 0400)
#endif

/*
 * A screen cell: the character as a Unicode code point (bits 0-20) and
 * its attributes and colour role above it (scr.c)
 */
typedef int CELL;
#define CHARMASK 0x1fffff

typedef struct {
    short cury, curx;
    int flgs;
    char clear;
    CELL **y;
    short *firstch;
    short *lastch;
    short *lnum;
} WINDOW;

typedef struct {
    short y, x;
    short ny, nx;
    CELL *mem;
} BOX;

typedef long CURSOR;

extern int LINES, COLS;
extern WINDOW VScreen;

extern int TtyUpperCase;

#define VMPutChar(y, x, ch)    (VMove(y, x), VPutChar(ch))
#define VMPutString(y, x, str) (VMove(y, x), VPutString(str))
#define VMGetChar(yy, x)       (VMove(yy, x), (VScreen.y[VScreen.cury][VScreen.curx] & CHARMASK))

int VInit(void);
void VOpen(void);
void VReopen(void);
void VClose(void);
void VRestore(void);
void VRedraw(void);
void VSyncLine(int wy);
void VSync(void);
void VInsLine(int n);
void VDelLine(int n);
void VMove(int y, int x);
void VClearLine(void);
void VClear(void);
void VPutString(char *str);
int VStrLen(const char *s);             /* in cells: the code points */
char *VStrSkip(const char *s, int n);   /* past n cells */
void VPutStringN(const char *s, int n); /* at most n cells */
void VPrint(char *fmt, ...);
void VSetPalette(int n, int nb, int nr, int nrb, int b, int bb, int br, int brb, int d, int db,
                 int dr, int drb);
/*
 * Colour roles: each part of the screen has its own colours for the six
 * attributes (normal, bold, dim, each also reversed), like the VC colour
 * scheme. A cell keeps its role; VClearBox clears in the current role.
 */
enum {
    VCOL_DEFAULT, /* the command line and what is outside of the panels */
    VCOL_PANEL,   /* panels: names, frames, cursor, tagged files */
    VCOL_KEYS,    /* the F1...F10 bar */
    VCOL_BAR,     /* the top line and the menu bar */
    VCOL_MENU,    /* pull-down menus and pop-up lists */
    VCOL_DIALOG,  /* dialogs (dim) and errors (bold) */
    VCOL_VIEW,    /* viewer and editor */
    VCOL_LIST,    /* pop-up lists: history, jobs, user menu */
    VCOL_NROLES
};
int VSetColor(int role); /* returns the previous role */
void VSetRoleColors(int role, const int colors[12]);
int VStandOut(void);
void VStandEnd(void);
void VSetNormal(void);
void VSetPrev(void);
int VSetDim(void);
int VSetBold(void);
void VPutChar(int c);
void VFlush(void);
void VBeep(void);
BOX *VGetBox(int y, int x, int ny, int nx);
CURSOR VGetCursor(void);
void VSetCursor(CURSOR c);
void VUngetBox(BOX *box);
void VPrintBox(BOX *box);
void VFreeBox(BOX *box);
void VClearBox(int r, int c, int nr, int nc);
void VFillBox(int r, int c, int nr, int nc, int sym);
void VHorLine(int r, int c, int nc);
void VVertLine(int c, int r, int nr);
void VCorner(int r, int c, int n);
void VDrawBox(int r, int c, int nr, int nc);
void VExpandString(char *s, char *d);
void _prscreen(void);

struct KeyMap {
    char *tcap;
    char *str;
    short val;
};

void KeyInit(struct KeyMap *map, void (*flushproc)(void));
int KeyGetChar(void);
int KeyGet(void);
void KeyUnget(int key);

#define CAPNUM 1
#define CAPFLG 2
#define CAPSTR 3

struct CapTab {
    char tname[3];
    char ttype;
    char tdef;
    char *tc;
    int *ti;
    char **ts;
};

int CapInit(char *bp);
void CapGet(struct CapTab *t);
char *CapGoto(char *CM, int destcol, int destline);

void TtySet(void);
void TtyReset(void);
void TtyFlushInput(void);
