#ifdef GRAPHICS
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <malloc.h>
#include <math.h>

#include "config.h"
#include "version.h"
#include "bitmaps/f0.h"
#include "bitmaps/f1.h"
#include "bitmaps/f2.h"
#include "bitmaps/f3.h"
#include "bitmaps/f4.h"
#include "bitmaps/f5.h"
#include "bitmaps/f6.h"
#include "bitmaps/f7.h"
#include "bitmaps/f8.h"
#include "bitmaps/f9.h"
#include "bitmaps/f10.h"
#include "bitmaps/f11.h"
#include "bitmaps/f12.h"
#include "bitmaps/f13.h"
#include "bitmaps/f14.h"
#include "bitmaps/f15.h"
#include "bitmaps/f16.h"
#include "bitmaps/f17.h"
#include "bitmaps/f18.h"
#include "bitmaps/f19.h"
#include "bitmaps/f20.h"
#include "bitmaps/f21.h"
/*#include "bitmaps/fills.h"*/

#define NUMCOLS 24
#define WINWIDTH 600
#define WINHEIGHT 400
#define MAXNAMELEN 20
#define MAXPOINTS 40
#define BORDERSIZE 50
#define BOXHEIGHT 20
#define COLSIZE ( WINWIDTH / NUMCOLS )
#define MAXLINEWIDTH 50
#define BUFLEN ( MAXPOINTS * 4 )
#define FONT "-misc-fixed-*-*-*-*-15-*-*-*-*-*-*-*"
#define TEXTFONT "-*-charter-*-r-*-*-%d-*-*-*-*-*-*-*"
#define NUMFONTSIZES 10
#define NUMTOOLS 10
#define SPRAYDENSITY 1
#define BASE 74
#define BORDER 100

#define DRAWTOOL 0
#define LINETOOL 1
#define SPRAYTOOL 2
/*#define FLOODTOOL 3*/
#define CIRCLETOOL 3
#define FCIRCLETOOL 4
#define RECTTOOL 5
#define FRECTTOOL 6
#define POLYTOOL 7
#define FPOLYTOOL 8
#define TEXTTOOL 9

/* Stuff for flood fill algorithm */
/*
static int      xMin, yMin, xMax, yMax;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define pixelwrite(x,y, im) {                       \
                XPutPixel(im, x, y, fore);    \
                xMin = MIN(xMin, x);            \
                yMin = MIN(yMin, y);            \
                xMax = MAX(xMax, x);            \
                yMax = MAX(yMax, y);            \
        }

#define pixelread(x,y, im)  (XGetPixel(im, x, y))

typedef struct {short y, xl, xr, dy;} Segment;

#define STACKSIZE 20000              /* max depth of stack

#define PUSH(Y, XL, XR, DY)  /* push new segment on stack  \
    if (sp<stack+STACKSIZE && Y+(DY)>=0 && Y+(DY)<WINHEIGHT) \
    {sp->y = Y; sp->xl = XL; sp->xr = XR; sp->dy = DY; sp++;}

#define POP(Y, XL, XR, DY)    /* pop segment off stack \
    {sp--; Y = sp->y+(DY = sp->dy); XL = sp->xl; XR = sp->xr;}
*/
/* End of flood fill stuff */


extern void sent_out();
extern shell_print();
extern void bsx_redisplay();
extern int bsxexpose();
extern void bsxresize();
extern Window bsxwin;
Display *display = NULL;
Window mainwin = 0, win, colwins[NUMCOLS], toolwins[NUMTOOLS];
Window currentcolwin, brushsizewin, brushincwin, brushdecwin, clearwin;
XGCValues gcvalues;
GC gc[NUMCOLS], blackgc, whitegc, xorgc;
Colormap cmap, mymap = 0;
XColor colours[NUMCOLS];
XFontStruct *xfs, *fonts[NUMFONTSIZES];
Cursor cursor;
int colour, mainwinmapped = 0, bsxwinmapped = 0;
int screen, col = 1, linewidth = 10;
int fontsize = NUMFONTSIZES / 2;
int tool = DRAWTOOL, state = 0, oldx, oldy, prevx, prevy, firstx, firsty;
char buffer[BUFLEN], *bufp;
int buflen, numpoints;
XPoint points[MAXPOINTS], polypoints[MAXPOINTS];
int numpolypoints;
long sprayseed;
char fontname[80];
unsigned long whitep, blackp;
extern shell *current_shell, *first_shell;
void blankpixmap();

int forcecolour = -1;

char colnames[NUMCOLS][20] = {"white", "black", "gray40", "navy", "brown4", "green4", "blue3", "brown3", "blue", "red", "purple", "gray70", "green3", "hot pink", "dodger blue", "pink", "orange", "gold", "wheat1", "green", "gray90", "pale green", "cyan", "yellow"}; 

int fontsizes[NUMFONTSIZES] = {8, 10, 12, 14, 17, 19, 25, 33, 40, 60};

void send_buffer() {
  buffer[buflen] = '\r';
  buffer[buflen + 1] = '\n';
  buffer[buflen + 2] = '\0';
  write(current_shell->sock_desc, buffer, buflen + 3);
}

/* For whenever a shell switch occurs */
void redisplay() {
  bsx_redisplay();
  if (current_shell->grmode > GRBSX)
    if (current_shell->frame != 0)
      XCopyArea(display, current_shell->frame, win, gc[1], 0, 0, 
		WINWIDTH, WINHEIGHT, 0, 0);
    else 
      XFillRectangle(display, win, gc[0], 0, 0, WINWIDTH, WINHEIGHT);
  else 
    if (mainwin != 0)
      XFillRectangle(display, win, gc[0], 0, 0, WINWIDTH, WINHEIGHT);
}

char *putbyte(str, n)
char *str;
int n;
{
  *str++ = '0' + n;
  buflen++;
  return str;
}

char *putcol(str)
char *str;
{
  str = putbyte(str, col);
  return str;
}

char *putnum(str, n)
char *str;
int n;
{
  int low;
  n += BORDER;
  low = n % BASE;
  str = putbyte(str, (n - low) / BASE);
  str = putbyte(str, low);
  return str;
}

char *putcom(str)
char *str;
{
  strcpy(str, "graphstr ");
  str += 9;
  buflen += 9;
  return str;
}

char *getbyte(str, n)
char *str;
int *n;
{
  if (*str != '\0') {
    *n = (int) (*str - '0');
    return str+1;
  }
  else
    return str;
}

char *getnum(str, n)
char *str;
int *n;
{
  int tmp;
  str = getbyte(str, &tmp);
  *n = tmp * BASE;
  str = getbyte(str, &tmp);
  *n = *n + tmp - BORDER;
  return str;
}

char *getcol(str, n)
char *str;
int *n;
{
  str = getbyte(str, n);
  if (*n >= NUMCOLS)
    *n = 1;
  return str;
}

void drawline(str, sh, type) 
char *str;
shell *sh;
int type;
{
  int lcol;
  int x, y, width;

  numpoints = 0;
  str = getcol(str, &lcol);
  str = getnum(str, &width);
  while (*str != '\0' && *str != '\n' && *str != '\r') {
    str = getnum(str, &x);
    points[numpoints].x = x;
    str = getnum(str, &y);
    points[numpoints].y = y;
    numpoints++;
  }
  if (width != linewidth) {
    gcvalues.line_width = width;
    XChangeGC(display, gc[lcol], GCLineWidth, &gcvalues);
  }
  if (type == 1) {
    gcvalues.cap_style = CapRound;
    gcvalues.join_style = JoinRound;
    XChangeGC(display, gc[lcol], GCCapStyle | GCJoinStyle, &gcvalues);
  }
  else {
    gcvalues.join_style = JoinBevel;
    XChangeGC(display, gc[lcol], GCJoinStyle, &gcvalues);
  }
  XDrawLines(display, sh->frame, gc[lcol], points, numpoints, CoordModeOrigin);
  if (sh == current_shell)
    XDrawLines(display, win, gc[lcol], points, numpoints, CoordModeOrigin);
  gcvalues.cap_style = CapButt;
  gcvalues.join_style = JoinMiter;
  XChangeGC(display, gc[col], GCCapStyle | GCJoinStyle, &gcvalues);
  if (width != linewidth) {
    gcvalues.line_width =linewidth;
    XChangeGC(display, gc[lcol], GCLineWidth, &gcvalues);
  }
}

/*
int doflood(image, x, y, fore, ov)
XImage *image;
int x, y;
unsigned long fore, ov;
{
  int start, x1, x2, dy = 0;
  Segment stack[STACKSIZE], *sp = stack;   /* stack of filled segments 

  PUSH(y, x, x, 1);                      /* needed in some cases 
  PUSH(y+1, x, x, -1);                /* seed segment (popped 1st) 

  while (sp>stack) {
    /* pop segment off stack and fill a neighboring scan line 
    POP(y, x1, x2, dy);
    /*
     * segment of scan line y-dy for x1<=x<=x2 was previously filled,
     
    for (x = x1; x >= 0 && pixelread(x, y, image) == ov; x--)
      pixelwrite(x, y, image);
    if (x >= x1) {
      for (x++; x<=x2 && pixelread(x, y, image)!=ov; x++);
      start = x;
      if (x > x2)
        continue;
    } else {
      start = x+1;
      if (start<x1) PUSH(y, start, x1-1, -dy);       /* leak on left? 
      x = x1+1;
    }
    do {
      for (; x<WINWIDTH && pixelread(x, y, image)==ov; x++)
        pixelwrite(x, y, image);
      PUSH(y, start, x-1, dy);
      if (x>x2+1) PUSH(y, x2+1, x-1, -dy);       /* leak on right? 
      for (x++; x<=x2 && pixelread(x, y, image)!=ov; x++);
      start = x;
    } while (x<=x2);
  }
}
*/
/* Flood filling disabled. */
/*
void floodfill(str, sh)
char *str;
shell *sh;
{
  XImage *image;
  int fcol, x, y;
  unsigned long backpix;
  str = getcol(str, &fcol);
  str = getnum(str, &x);
  str = getnum(str, &y);
  cursor = XCreateFontCursor(display, XC_watch);
  XDefineCursor(display, win, cursor);
  image = XGetImage(display, sh->frame, 0, 0, WINWIDTH,WINHEIGHT,~0,XYPixmap);
  backpix = XGetPixel(image, x, y);
  if (backpix != colours[fcol].pixel) {
    doflood(image, x, y, colours[fcol].pixel, backpix);
    XPutImage(display, sh->frame, blackgc, image, 0, 0,0,0,WINWIDTH,WINHEIGHT);
    if (sh == current_shell)
      XPutImage(display, win, blackgc, image, 0, 0, 0, 0,WINWIDTH,WINHEIGHT);
  }
  XDestroyImage(image);
  switch(tool) {
  case DRAWTOOL:
    cursor = XCreateFontCursor(display, XC_pencil);
    break;
  case SPRAYTOOL:
    cursor = XCreateFontCursor(display, XC_spraycan);
    break;
  case FLOODTOOL:
    cursor = XCreateFontCursor(display, XC_diamond_cross);
    break;
  default:
    cursor = XCreateFontCursor(display, XC_cross);
    break;
  }
  XDefineCursor(display, win, cursor);
}
*/

void circle(str, sh, filled)
char *str;
shell *sh;
int filled;
{
  int ccol, x, y, x1, y1, width, w, h;
  str = getcol(str, &ccol);
  if (!filled)
    str = getnum(str, &width);
  str = getnum(str, &x);
  str = getnum(str, &y);
  str = getnum(str, &x1);
  str = getnum(str, &y1);
  w = abs(x1 - x);
  h = abs(y1 - y);
  if (filled) {
    XFillArc(display, sh->frame, gc[ccol], x - w, y - h, w * 2, h*2,0,360*64);
    if (sh == current_shell)
      XFillArc(display, win, gc[ccol], x - w, y - h, w * 2, h * 2, 0,360*64);
  }
  else {
    if (width != linewidth) {
      gcvalues.line_width = width;
      XChangeGC(display, gc[ccol], GCLineWidth, &gcvalues);
    }
    XDrawArc(display, sh->frame, gc[ccol], x - w, y - h, w * 2, h*2,0,360*64);
    if (sh == current_shell)
      XDrawArc(display, win, gc[ccol], x - w, y - h, w * 2, h * 2, 0,360*64);
    if (width != linewidth) {
      gcvalues.line_width = linewidth;
      XChangeGC(display, gc[ccol], GCLineWidth, &gcvalues);
    }
  }
}

void rectangle(str, sh, filled)
char *str;
shell *sh;
int filled;
{
  int rcol, x, y, x1, y1, width, w, h;
  str = getcol(str, &rcol);
  if (!filled)
    str = getnum(str, &width);
  str = getnum(str, &x);
  str = getnum(str, &y);
  str = getnum(str, &x1);
  str = getnum(str, &y1);
  w = abs(x1 - x);
  h = abs(y1 - y);
  if (x1 < x)
    x = x1;
  if (y1 < y)
    y = y1;
  if (filled) {
    XFillRectangle(display, sh->frame, gc[rcol], x, y, w, h);
    if (sh == current_shell)
      XFillRectangle(display, win, gc[rcol], x, y, w, h);
  }
  else {
    if (width != linewidth) {
      gcvalues.line_width = width;
      XChangeGC(display, gc[rcol], GCLineWidth, &gcvalues);
    }
    XDrawRectangle(display, sh->frame, gc[rcol], x, y, w, h);
    if (sh == current_shell)
      XDrawRectangle(display, win, gc[rcol], x, y, w, h);
    if (width != linewidth) {
      gcvalues.line_width = linewidth;
      XChangeGC(display, gc[rcol], GCLineWidth, &gcvalues);
    }
  }
}

void drawspray(str, sh) 
char *str;
shell *sh;
{
  int scol;
  int x, y, width, density;
  long seed, oldseed;
  int dir, l, i;
  XPoint spoints[3*(SPRAYDENSITY+MAXLINEWIDTH/2)+1];

  numpoints = 0;
  str = getcol(str, &scol);
  str = getnum(str, &width);
  str = getnum(str, &density);
  str = getnum(str, &seed); 
  while (*str != '\0' && *str != '\n' && *str != '\r') {
    str = getnum(str, &x);
    str = getnum(str, &y);
    numpoints = 0;
    srandom(seed);
    for (i = 0; i < density; i++) {
      dir = random() % 360;
      l = random() % ((width + 1) / 2);
      spoints[numpoints].x = x + l * cos((double) dir / 180.0 * M_PI);
      spoints[numpoints].y = y + l * sin((double) dir / 180.0 * M_PI);
      numpoints++;
    }
    oldseed = seed;
    seed = random();
    if (seed == oldseed)
      seed += 10;
    XDrawPoints(display, sh->frame,gc[scol],spoints,numpoints,CoordModeOrigin);
    if (sh == current_shell)
      XDrawPoints(display, win, gc[scol], spoints, numpoints, CoordModeOrigin);
  }
}

void polygon(str, sh, filled) 
char *str;
shell *sh;
int filled;
{
  int pcol;
  int x, y, width;

  numpoints = 0;
  str = getcol(str, &pcol);
  if (!filled)
    str = getnum(str, &width);
  while (*str != '\0' && *str != '\n' && *str != '\r') {
    str = getnum(str, &x);
    points[numpoints].x = x;
    str = getnum(str, &y);
    points[numpoints].y = y;
    numpoints++;
  }
  points[numpoints].x = points[0].x;
  points[numpoints].y = points[0].y;
  numpoints++;
  if (filled) {
    XFillPolygon(display, sh->frame, gc[pcol], points, numpoints, Complex,
		 CoordModeOrigin);
    if (sh == current_shell)
      XFillPolygon(display, win, gc[pcol], points, numpoints, Complex,
		   CoordModeOrigin);
  }
  else {
    if (width != linewidth) {
      gcvalues.line_width = width;
      XChangeGC(display, gc[pcol], GCLineWidth, &gcvalues);
    }
    XDrawLines(display, sh->frame, gc[pcol], points,numpoints,CoordModeOrigin);
    if (sh == current_shell)
      XDrawLines(display, win, gc[pcol], points, numpoints, CoordModeOrigin);
    if (width != linewidth) {
      gcvalues.line_width =linewidth;
      XChangeGC(display, gc[pcol], GCLineWidth, &gcvalues);
    }
  }
}

void changestate(str, sh)
char *str;
shell *sh;
{
  switch (str[0]) {
  case '0':
    blankpixmap(&(sh->frame));
    sh->grmode = GRNONE;
    redisplay();
    break;
  case '1':
    sh->grmode = GRWRITE;
    break;
  case '2':
    sh->grmode = GRREAD;
    break;
  }
}

void dotext(str, sh)
char *str;
shell *sh;
{
  int tcol;
  int x, y, size, i, fsize, mindif, dif;

  str = getcol(str, &tcol);
  str = getnum(str, &size);
  str = getnum(str, &x);
  str = getnum(str, &y);
  mindif = 100;
  for (i = 0; i < NUMFONTSIZES; i++) {
    dif = abs(size - fontsizes[i]);
    if (dif < mindif) {
      mindif = dif;
      fsize = i;
    }
  }
  if (fonts[fsize] == NULL) {
    sprintf(fontname, TEXTFONT, fontsizes[fsize]);
    fonts[fsize] = XLoadQueryFont(display, fontname);
  }
  if (fonts[fsize] == NULL) {
    shell_print(current_shell, "Failed to load font.");
    return;
  }
  gcvalues.font = fonts[fsize]->fid;
  XChangeGC(display, gc[tcol], GCFont, &gcvalues);
  XDrawString(display, sh->frame, gc[tcol], x, y, str, strlen(str));
  if (sh == current_shell)
    XDrawString(display, win, gc[tcol], x, y, str, strlen(str));
  gcvalues.font = fonts[fontsize]->fid;
  XChangeGC(display, gc[tcol], GCFont, &gcvalues);
}

void versionnum(sh)
shell *sh;
{
  char temp[30];
  sprintf(temp,"graphstr xtush %s\n",VERSION);
  write(sh->sock_desc, temp, strlen(temp));
}

void clear(sh)
shell *sh;
{
  XFillRectangle(display, sh->frame, gc[0], 0, 0, WINWIDTH, WINHEIGHT);
  if (sh == current_shell)
    XFillRectangle(display, win, gc[0], 0, 0, WINWIDTH, WINHEIGHT);
}

void draw_current_col() {
  XFillRectangle(display, currentcolwin, gc[col], 0, 0, WINWIDTH,
		 BORDERSIZE / 2);
  if (!colour)
    XDrawString(display, currentcolwin, 
		(col < NUMCOLS / 2 + 4  && col > 0 ? whitegc : blackgc),
		(WINWIDTH - XTextWidth(xfs, colnames[col], strlen(colnames[col]))) / 2,
		(BORDERSIZE/2 + xfs->ascent) / 2, colnames[col],
		strlen(colnames[col]));
}

void draw_brush_size() {
  XClearWindow(display, brushsizewin);
  if (tool == TEXTTOOL)
    XDrawString(display, brushsizewin, gc[1], 
		(BORDERSIZE - XTextWidth(fonts[fontsize], "ABC", 3)) / 2,
		(BORDERSIZE + fonts[fontsize]->ascent - 
		 fonts[fontsize]->descent) / 2, "ABC", 3);
  else {
    gcvalues.cap_style = CapRound;
    XChangeGC(display, gc[1], GCCapStyle, &gcvalues);
    XDrawLine(display, brushsizewin, gc[1], BORDERSIZE/2, BORDERSIZE/2, 
	      BORDERSIZE/2, BORDERSIZE/2);
    gcvalues.cap_style = CapButt;
    XChangeGC(display, gc[1], GCCapStyle, &gcvalues);
  }
}

/* arrows for changing brush/font size */
void draw_left_arrow(w)
Window w;
{
  XClearWindow(display, w);
  XDrawLine(display, w, blackgc, 5, BOXHEIGHT/2, BORDERSIZE/2 - 5, 
              BOXHEIGHT/2);
  XDrawLine(display, w, blackgc, 5, BOXHEIGHT/2, 10, 5);
  XDrawLine(display, w, blackgc, 5, BOXHEIGHT/2, 10, BOXHEIGHT - 5);
}

void draw_right_arrow(w)
Window w;
{
  XClearWindow(display, w);
  XDrawLine(display, w, blackgc, 5, BOXHEIGHT/2, BORDERSIZE/2 - 5, 
              BOXHEIGHT/2);
  XDrawLine(display, w, blackgc, BORDERSIZE/2 - 5, BOXHEIGHT/2, 
                                                BORDERSIZE/2 - 10, 5);
  XDrawLine(display, w, blackgc, BORDERSIZE/2 - 5, BOXHEIGHT/2, 
                               BORDERSIZE/2 - 10, BOXHEIGHT - 5);
}

/* Text buttons for tools */
void draw_textbutton(win, str)
Window win;
char *str;
{
  XDrawString(display, win, blackgc,
    (BORDERSIZE - XTextWidth(xfs, str, strlen(str))) / 2,
    (BOXHEIGHT + xfs->ascent) / 2, str, strlen(str));
}

/* Draw tool buttons, inverted if current tool */
void draw_tool(n)
int n;
{
  XClearWindow(display, toolwins[n]);
  switch(n) {
  case DRAWTOOL:
    draw_textbutton(toolwins[n], "DRAW");
    break;
  case LINETOOL:
    draw_textbutton(toolwins[n], "LINE");
    break;
  case SPRAYTOOL:
    draw_textbutton(toolwins[n], "SPRAY");
    break;
/*  case FLOODTOOL:
    draw_textbutton(toolwins[n], "FFILL");
    break;*/
  case CIRCLETOOL:
    draw_textbutton(toolwins[n], "CIRC");
    break;
  case FCIRCLETOOL:
    draw_textbutton(toolwins[n], "FCIRC");
    break;
  case RECTTOOL:
    draw_textbutton(toolwins[n], "RECT");
    break;
  case FRECTTOOL:
    draw_textbutton(toolwins[n], "FRECT");
    break;
  case POLYTOOL:
    draw_textbutton(toolwins[n], "POLY");
    break;
  case FPOLYTOOL:
    draw_textbutton(toolwins[n], "FPOLY");
    break;
  case TEXTTOOL:
    draw_textbutton(toolwins[n], "TEXT");
    break;
  }
  if (n == tool)
    XFillRectangle(display, toolwins[n], xorgc, 0, 0, BORDERSIZE, BOXHEIGHT);
}

/* Get the display and open it */
void open_display() {
  char *display_name;
  display_name = getenv("DISPLAY");
  if (!display_name) {
    shell_print(current_shell, "DISPLAY variable not set.");
    return;
  }
  display = XOpenDisplay(display_name);
  if (display == (Display *) NULL) {
    shell_print(current_shell, "Could not open display.");
    return;
  }
  screen = DefaultScreen(display);
  blackp = BlackPixel(display, screen);
  whitep = WhitePixel(display, screen);
  cmap = DefaultColormap(display, screen);
  if (DefaultDepth(display, screen) > 1)
    if (DefaultDepth(display, screen) >= 16)
      colour = 2;
    else
      colour = 1;
  else
    colour = 0;
  if (forcecolour != -1)
    colour = forcecolour;
}

/* Make the drawing window, gc's and things */
void create_window()
{
  XWMHints wmhints;
  XSizeHints xsh;
  XColor dummy;
  int i, flags; 
  unsigned long pixels[NUMCOLS - 2];
  Pixmap tiles[NUMCOLS - 2];

  /* Initialise fonts. */
  for (i = 0; i < NUMFONTSIZES; i++)
    fonts[i] = NULL;
  sprintf(fontname, TEXTFONT, fontsizes[fontsize]);
  fonts[fontsize] = XLoadQueryFont(display, fontname);

  /* Create gc's for black, white and xor */
  gcvalues.foreground = blackp;
  xfs = XLoadQueryFont(display, FONT);
  gcvalues.font = xfs->fid;
  blackgc = XCreateGC(display,RootWindow(display,screen),GCForeground | GCFont,
                      &gcvalues);
  if (colour)
    gcvalues.function = GXxor;
  else
    gcvalues.function = GXinvert;
  if (blackp == 0)
    gcvalues.foreground = whitep;
  xorgc = XCreateGC(display,RootWindow(display,screen),GCForeground|GCFunction,
                      &gcvalues);
  gcvalues.foreground = whitep;
  whitegc = XCreateGC(display,RootWindow(display,screen),GCForeground | GCFont,
                      &gcvalues);

  /* Create root window */
  mainwin = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 
             WINWIDTH + BORDERSIZE, WINHEIGHT + BORDERSIZE, 0, blackp, whitep);
  xsh.flags = PSize | PMaxSize;
  xsh.width = xsh.max_width = WINWIDTH + BORDERSIZE;
  xsh.height = xsh.max_height = WINHEIGHT + BORDERSIZE;
  XSetWMNormalHints(display, mainwin, &xsh);
  wmhints.flags = InputHint;
  wmhints.input = True;
  XSetWMHints(display, mainwin, &wmhints);
  XStoreName(display, mainwin, "xtush graphics");
  XMapWindow(display, mainwin);
  mainwinmapped = 1;

  /* Set up flags for drawing gc's */
  gcvalues.cap_style = CapButt;
  gcvalues.line_width = linewidth;
  gcvalues.join_style = JoinMiter;
  gcvalues.font = fonts[fontsize]->fid;
  flags = GCForeground|GCLineWidth|GCCapStyle|GCJoinStyle|GCFont;

  /* Get black and white drawing colours allocated, so they're the same
     on the private colour map, if needed, and setup windows for them. */
  for (i = 0; i < 2; i++) {  
    XAllocNamedColor(display, cmap, colnames[i], &colours[i], &dummy);
    gcvalues.foreground = colours[i].pixel;
    gc[i] = XCreateGC(display, RootWindow(display, screen), flags, &gcvalues);
    colwins[i] = XCreateSimpleWindow(display, mainwin, BORDERSIZE + i*COLSIZE,
                0, COLSIZE, BORDERSIZE/2 - 1, 1, blackp, gcvalues.foreground); 
    XSelectInput(display, colwins[i], ButtonPressMask | ExposureMask);
    XMapWindow(display, colwins[i]);
  }

  /* Get space for colourcells, using private colourmap if needed, or
     create bitmaps for black and white. */
  if (colour) {
    if (colour == 1) {
      if (!XAllocColorCells(display, cmap, 0, 0, 0, pixels, NUMCOLS - 2)) {
	mymap = XCopyColormapAndFree(display, cmap);
	XAllocColorCells(display, mymap, 0, 0, 0, pixels, NUMCOLS - 2);
	XSetWindowColormap(display, mainwin, mymap);
	XInstallColormap(display, mymap);
	cmap = mymap;
      }
    }
  }
  else {
    tiles[0] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f0_bits, f0_width, f0_height );
    tiles[1] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f1_bits, f1_width, f1_height );
    tiles[2] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f2_bits, f2_width, f2_height );
    tiles[3] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f3_bits, f3_width, f3_height );
    tiles[4] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f4_bits, f4_width, f4_height );
    tiles[5] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f5_bits, f5_width, f5_height );
    tiles[6] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f6_bits, f6_width, f6_height );
    tiles[7] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f7_bits, f7_width, f7_height );
    tiles[8] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f8_bits, f8_width, f8_height );
    tiles[9] = XCreateBitmapFromData(display, RootWindow(display, screen),
				     f9_bits, f9_width, f9_height );
    tiles[10] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f10_bits,f10_width, f10_height );
    tiles[11] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f11_bits,f11_width, f11_height );
    tiles[12] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f12_bits,f12_width, f12_height );
    tiles[13] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f13_bits,f13_width, f13_height );
    tiles[14] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f14_bits,f14_width, f14_height );
    tiles[15] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f15_bits,f15_width, f15_height );
    tiles[16] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f16_bits,f16_width, f16_height );
    tiles[17] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f17_bits,f17_width, f17_height );
    tiles[18] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f18_bits,f18_width, f18_height );
    tiles[19] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f19_bits,f19_width, f19_height );
    tiles[20] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f20_bits,f20_width, f20_height );
    tiles[21] = XCreateBitmapFromData(display, RootWindow(display, screen),
				      f21_bits,f21_width, f21_height );
  }

  /* Create drawing area window */
  win = XCreateSimpleWindow(display, mainwin, BORDERSIZE, BORDERSIZE, 
                   WINWIDTH, WINHEIGHT, 1, blackp, whitep);
  cursor = XCreateFontCursor(display, XC_pencil);
  XDefineCursor(display, win, cursor);

  XSelectInput(display, win, ButtonPressMask | ButtonReleaseMask
    | ExposureMask | KeyPressMask | PointerMotionMask | LeaveWindowMask);
  XMapWindow(display, win);

  /* Create gc's and windows for remaining drawing colours */
  for (i = 2; i < NUMCOLS; i++) {
    if (colour == 1) {
      XStoreNamedColor(display, cmap,colnames[i],pixels[i-2], 
		       DoRed | DoGreen | DoBlue);
      XParseColor(display, cmap, colnames[i], &colours[i]);
      colours[i].pixel = pixels[i-2];
      gcvalues.foreground = colours[i].pixel;
    }
    else if (colour == 2) {
      XAllocNamedColor(display, cmap, colnames[i], &colours[i], &dummy);
      gcvalues.foreground = colours[i].pixel;
    }
    else {
      flags |= GCStipple|GCFillStyle|GCBackground;
      gcvalues.foreground = blackp;
      gcvalues.background = whitep;
      gcvalues.fill_style = FillOpaqueStippled;
      gcvalues.stipple=tiles[i-2];
    }
    gc[i] = XCreateGC(display, RootWindow(display, screen), flags, &gcvalues);
    colwins[i] = XCreateSimpleWindow(display, mainwin, BORDERSIZE + i*COLSIZE,
                0, COLSIZE, BORDERSIZE/2 - 1, 1, blackp, gcvalues.foreground); 
    XSelectInput(display, colwins[i], ButtonPressMask | ExposureMask);
    XMapWindow(display, colwins[i]);
  }

  /* Create tool windows */
  for (i = 0; i < NUMTOOLS; i++) {
    toolwins[i] = XCreateSimpleWindow(display, mainwin, 0, BORDERSIZE + 
        (i + 1) * BOXHEIGHT, BORDERSIZE - 1, BOXHEIGHT, 1, blackp, whitep);
    XSelectInput(display, toolwins[i], ExposureMask | ButtonPressMask);
    XMapWindow(display, toolwins[i]);
    draw_tool(i);
  }

  /* Currently used colour window */
  currentcolwin = XCreateSimpleWindow(display, mainwin,BORDERSIZE,BORDERSIZE/2,
                         WINWIDTH, BORDERSIZE / 2 - 1, 1, blackp, whitep);
  XSelectInput(display, currentcolwin, ExposureMask);
  XMapWindow(display, currentcolwin);
  draw_current_col();

  /* Brush size window */
  brushsizewin = XCreateSimpleWindow(display, mainwin, 0, 0, BORDERSIZE - 1,
      BORDERSIZE - 1, 1, blackp, whitep);
  XSelectInput(display, brushsizewin, ExposureMask);
  XMapWindow(display, brushsizewin);
  draw_brush_size();

  /* Arrow windows for increasing/decreasing brush size */
  brushdecwin = XCreateSimpleWindow(display, mainwin, 0, BORDERSIZE, 
                      BORDERSIZE/2 - 1, BOXHEIGHT, 1, blackp, whitep);
  XSelectInput(display, brushdecwin, ExposureMask | ButtonPressMask);
  XMapWindow(display, brushdecwin);
  draw_left_arrow(brushdecwin);
  brushincwin = XCreateSimpleWindow(display, mainwin, BORDERSIZE/2,BORDERSIZE, 
                      BORDERSIZE/2 - 1, BOXHEIGHT, 1, blackp, whitep);
  XSelectInput(display, brushincwin, ExposureMask | ButtonPressMask);
  XMapWindow(display, brushincwin);
  draw_right_arrow(brushincwin);

  /* Clear button window */
  clearwin = XCreateSimpleWindow(display, mainwin, 0, BORDERSIZE + 
      WINHEIGHT - BOXHEIGHT, BORDERSIZE - 1, BOXHEIGHT, 1, blackp, whitep);
  XSelectInput(display, clearwin, ExposureMask | ButtonPressMask);
  XMapWindow(display, clearwin);
  draw_textbutton(clearwin, "CLEAR");
} 

void create_frame(sh)
shell *sh;
{
  sh->frame = XCreatePixmap(display, win, WINWIDTH, WINHEIGHT,
                   DefaultDepth(display, screen));
  XFillRectangle(display, sh->frame, gc[0], 0, 0, WINWIDTH, WINHEIGHT);
}

int graphics_processing(sh, str)
shell *sh;
char *str;
{
  char *t;
  char name[MAXNAMELEN];
  char msg[MAXNAMELEN + 15];
  char command[3];
  int i;
  t = str;
  if (display == NULL)
    open_display();
  if (mainwin == 0 && display != NULL)
    create_window();
  if (mainwin && !mainwinmapped) {
    XMapWindow(display, mainwin);
    mainwinmapped = 1;
  }
  if (display != NULL) {
    if (sh->frame == 0)
      create_frame(sh);
    i = 0;
    while(*str != ':') {
      name[i] = *str;
      str++;
      i++;
    }
    name[i] = '\0';
    sprintf(msg, "Last drawer: %s", name);
    XStoreName(display, mainwin, msg);
    str++;
    command[0] = str[0];
    command[1] = str[1];
    command[2] = '\0';
    str += 2;
    while(str[strlen(str) - 1] == '\r' || str[strlen(str) - 1] == '\n')
      str[strlen(str) - 1] = '\0';
    if (strcmp(command, "LN") == 0)
      drawline(str, sh, 1);
    else if (strcmp(command, "LS") == 0)
      drawline(str, sh, 0);
    else if (strcmp(command, "SP") == 0)
      drawspray(str, sh);
/*    else if (strcmp(command, "FF") == 0)
      floodfill(str, sh); */
    else if (strcmp(command, "CC") == 0)
      circle(str, sh, 0);
    else if (strcmp(command, "FC") == 0)
      circle(str, sh, 1);
    else if (strcmp(command, "RR") == 0)
      rectangle(str, sh, 0);
    else if (strcmp(command, "FR") == 0)
      rectangle(str, sh, 1);
    else if (strcmp(command, "PP") == 0)
      polygon(str, sh, 0);
    else if (strcmp(command, "FP") == 0)
      polygon(str, sh, 1);
    else if (strcmp(command, "TX") == 0)
      dotext(str, sh);
    else if (strcmp(command, "CL") == 0)
      clear(sh);
    else if (strcmp(command, "ST") == 0)
      changestate(str, sh);
    else if (strcmp(command, "VN") == 0)
      versionnum(sh);
  }
  return strlen(t) + 1;
}

void drawstart(x, y)
int x, y;
{
  state = 1;
  oldx = x;
  oldy = y;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  *bufp++ = 'L';
  *bufp++ = 'N';
  buflen += 2;
  bufp = putcol(bufp);
  bufp = putnum(bufp, linewidth);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  gcvalues.cap_style = CapRound;
  XChangeGC(display, gc[col], GCCapStyle, &gcvalues);
  XDrawLine(display, win, gc[col], x, y, x, y);
  gcvalues.cap_style = CapButt;
  XChangeGC(display, gc[col], GCCapStyle, &gcvalues);
}

void drawend()
{
  send_buffer();  
  state = 0;
}

void drawpoint(x, y)
int x, y;
{
  if (x<-BORDER || y<-BORDER || x>WINWIDTH+BORDER || y>WINHEIGHT+BORDER) {
    if (state == 1) {
      drawend();
      state = 2;
    }
  }
  else 
    if (state == 2)
      drawstart(x, y);
    else {
      bufp = putnum(bufp, x);
      bufp = putnum(bufp, y);
      gcvalues.cap_style = CapRound;
      XChangeGC(display, gc[col], GCCapStyle, &gcvalues);
      XDrawLine(display, win, gc[col], oldx, oldy, x, y);
      gcvalues.cap_style = CapButt;
      XChangeGC(display, gc[col], GCCapStyle, &gcvalues);
      oldx = x;
      oldy = y;
      if (buflen > BUFLEN - 10) {
	drawend();
	drawstart(x, y);
      }
    }
}

void linestart(x, y)
int x, y;
{
  state = 1;
  oldx = x;
  oldy = y;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  *bufp++ = 'L';
  *bufp++ = 'S';
  buflen += 2;
  bufp = putcol(bufp);
  bufp = putnum(bufp, linewidth);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  XDrawLine(display, win, xorgc, x, y, x, y);
  prevx = x;
  prevy = y;
}

void lineend()
{
  if (state == 2)
    send_buffer();  
  XDrawLine(display, win, xorgc, oldx, oldy, prevx, prevy);
  state = 0;
}

void lineadd(x, y)
int x, y;
{
  if (buflen > BUFLEN - 10) {
    lineend();
    linestart(oldx, oldy);
  }
  if (state == 1)
    state = 2;
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  XDrawLine(display, win, xorgc, oldx, oldy, prevx, prevy);
  XDrawLine(display, win, gc[col], oldx, oldy, x, y);
  oldx = x;
  oldy = y;
  XDrawLine(display, win, xorgc, x, y, x, y);
  prevx = x;
  prevy = y;
}

void linerubber(x, y)
int x, y;
{
  XDrawLine(display, win, xorgc, oldx, oldy, prevx, prevy);
  XDrawLine(display, win, xorgc, oldx, oldy, x, y);
  prevx = x;
  prevy = y;
}

void drawspraypoint(x, y)
int x, y;
{
  int i, dir, l;
  long oldseed;
  XPoint spoints[3*(SPRAYDENSITY+MAXLINEWIDTH/2)+1];
  numpoints = 0;
  srandom(sprayseed);
  for (i = 0; i < state * (SPRAYDENSITY + linewidth / 2); i++) {
    dir = random() % 360;
    l = random() % ((linewidth + 1) / 2);
    spoints[numpoints].x = x + l * cos((double) dir / 180.0 * M_PI);
    spoints[numpoints].y = y + l * sin((double) dir / 180.0 * M_PI);
    numpoints++;
  }
  XDrawPoints(display, win, gc[col], spoints,numpoints,CoordModeOrigin);
  oldseed = sprayseed;
  sprayseed = random();
  if (sprayseed == oldseed)
    sprayseed += 10; 
}

void spraystart(x, y, n)
int x, y, n;
{
  int seed;
  state = n;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  *bufp++ = 'S';
  *bufp++ = 'P';
  buflen += 2;
  bufp = putcol(bufp);
  bufp = putnum(bufp, linewidth);
  bufp = putnum(bufp, state * (SPRAYDENSITY + linewidth / 2));
  sprayseed = random() % 255;
  bufp = putnum(bufp, sprayseed); 
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  drawspraypoint(x, y);
}

void sprayend()
{
  send_buffer();  
  state = 0;
}

void spraypoint(x, y)
int x, y;
{
  int tmp;
  if ( !(x<-BORDER || y<-BORDER || x>WINWIDTH+BORDER || y>WINHEIGHT+BORDER)) {
    if (buflen > BUFLEN - 10) {
      tmp = state;
      sprayend();
      spraystart(x, y, tmp);
    }
    else {
      bufp = putnum(bufp, x);
      bufp = putnum(bufp, y);
      drawspraypoint(x, y);
    }
  }
}

/* Flood fill.. removed
void sendfill(x, y)
int x, y;
{
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  *bufp++ = 'F';
  *bufp++ = 'F';
  buflen += 2;
  bufp = putcol(bufp);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  send_buffer();
}
*/

void circlestart(x, y, filled)
int x, y, filled;
{
  state = 1;
  oldx = x;
  oldy = y;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  if (filled)
    *bufp++ = 'F';
  else
    *bufp++ = 'C';
  *bufp++ = 'C';
  buflen += 2;
  bufp = putcol(bufp);
  if (!filled)
    bufp = putnum(bufp, linewidth);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  XDrawArc(display, win, xorgc, x, y, 0, 0, 0, 360 * 64);
  prevx = x;
  prevy = y;
}

void circleend(x, y)
int x, y;
{
  int width, height;
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  send_buffer();  
  width = abs(prevx - oldx);
  height = abs(prevy - oldy);
  XDrawArc(display, win, xorgc, oldx - width, oldy - height, 
	   width*2, height*2, 0, 360*64);
  state = 0;
}

void circlerubber(x, y)
int x, y;
{
  int width, height;
  width = abs(prevx - oldx);
  height = abs(prevy - oldy);
  XDrawArc(display, win, xorgc, oldx - width, oldy - height, 
	   width*2, height*2, 0, 360*64);
  width = abs(x - oldx);
  height = abs(y - oldy);
  XDrawArc(display, win, xorgc, oldx - width, oldy - height, 
	   width*2, height*2, 0, 360*64);
  prevx = x;
  prevy = y;
}

void rectanglestart(x, y, filled)
int x, y, filled;
{
  state = 1;
  oldx = x;
  oldy = y;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  if (filled)
    *bufp++ = 'F';
  else
    *bufp++ = 'R';
  *bufp++ = 'R';
  buflen += 2;
  bufp = putcol(bufp);
  if (!filled)
    bufp = putnum(bufp, linewidth);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  XDrawRectangle(display, win, xorgc, x, y, 0, 0);
  prevx = x;
  prevy = y;
}

void rectangleend(x, y)
int x, y;
{
  int width, height;
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  send_buffer();  
  width = abs(prevx - oldx);
  height = abs(prevy - oldy);
  if (prevx > oldx)
    prevx = oldx;
  if (prevy > oldy)
    prevy = oldy;
  XDrawRectangle(display, win, xorgc, prevx, prevy, width, height);
  state = 0;
}

void rectanglerubber(x, y)
int x, y;
{
  int width, height;
  width = abs(prevx - oldx);
  height = abs(prevy - oldy);
  if (prevx > oldx)
    prevx = oldx;
  if (prevy > oldy)
    prevy = oldy;
  XDrawRectangle(display, win, xorgc, prevx, prevy, width, height);
  width = abs(x - oldx);
  height = abs(y - oldy);
  prevx = x;
  prevy = y;
  if (x > oldx)
    x = oldx;
  if (y > oldy)
    y = oldy;
  XDrawRectangle(display, win, xorgc, x, y, width, height);
}

void polystart(x, y, filled)
int x, y, filled;
{
  state = 1;
  oldx = x;
  oldy = y;
  firstx = x;
  firsty = y;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  if (filled)
    *bufp++ = 'F';
  else
    *bufp++ = 'P';
  *bufp++ = 'P';
  buflen += 2;
  bufp = putcol(bufp);
  if (!filled)
    bufp = putnum(bufp, linewidth);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  prevx = x;
  prevy = y;
  polypoints[0].x = x;
  polypoints[0].y = y;
  numpolypoints = 1;
  XDrawLine(display, win, xorgc, x, y, x, y);
}

void polyend()
{
  if (state == 2) {
    send_buffer();
    XDrawLine(display, win, xorgc, oldx, oldy, firstx, firsty);
    XDrawLine(display, win, xorgc, oldx, oldy, prevx, prevy);
    polypoints[numpolypoints].x = polypoints[0].x;
    polypoints[numpolypoints].y = polypoints[0].y; 
    numpolypoints++;
    XDrawLines(display, win, xorgc, polypoints, numpolypoints,CoordModeOrigin);
    XDrawPoints(display, win,xorgc,polypoints,numpolypoints-1,CoordModeOrigin);
  }
  XDrawLine(display, win, xorgc, firstx, firsty, prevx, prevy);
  state = 0;
}

void polyadd(x, y)
int x, y;
{
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  polypoints[numpolypoints].x = x;
  polypoints[numpolypoints].y = y;
  numpolypoints++;
  XDrawLine(display, win, xorgc, oldx, oldy, prevx, prevy);
  XDrawLine(display, win, xorgc, oldx, oldy, x, y);
  if (state == 1) {
    state = 2;
    XDrawLine(display, win, xorgc, firstx, firsty, x, y);
  }
  oldx = x;
  oldy = y;
  XDrawLine(display, win, xorgc, x, y, x, y);
  prevx = x;
  prevy = y;
  if (buflen > BUFLEN - 10)
    polyend();
}

void polyrubber(x, y)
int x, y;
{
  XDrawLine(display, win, xorgc, oldx, oldy, prevx, prevy);
  XDrawLine(display, win, xorgc, oldx, oldy, x, y);
  if (state == 2) {
    XDrawLine(display, win, xorgc, firstx, firsty, prevx, prevy);
    XDrawLine(display, win, xorgc, firstx, firsty, x, y);
  }
  prevx = x;
  prevy = y;
}

void textstart(x, y)
int x, y;
{
  state = 1;
  bufp = buffer;
  buflen = 0;
  bufp = putcom(bufp);
  *bufp++ = 'T';
  *bufp++ = 'X';
  buflen += 2;
  bufp = putcol(bufp);
  bufp = putnum(bufp, fontsizes[fontsize]);
  bufp = putnum(bufp, x);
  bufp = putnum(bufp, y);
  prevx = x;
  prevy = y;
  XDrawLine(display, win, xorgc, prevx, prevy, 
	    prevx + XTextWidth(fonts[fontsize], " ", 1), prevy);
}

void textend()
{
  XDrawLine(display, win, xorgc, prevx, prevy, 
	    prevx + XTextWidth(fonts[fontsize], " ", 1), prevy);
  if (state == 2)
    send_buffer();
  state = 0;
}

void textadd(c)
char c;
{
  if (isprint(c)) {
    XDrawLine(display, win, xorgc, prevx, prevy, 
	      prevx + XTextWidth(fonts[fontsize], " ", 1), prevy);
    if (state == 1)
      state = 2;
    *bufp++ = c;
    buflen++;
    XDrawString(display, win, gc[col], prevx, prevy, &c, 1);
    prevx += XTextWidth( fonts[fontsize], &c, 1);
    XDrawLine(display, win, xorgc, prevx, prevy, 
	      prevx + XTextWidth(fonts[fontsize], " ", 1), prevy);
  }
  else if (c == '\n' || c == '\r')
      textend();
  else if (((int) c == 8 || (int) c == 127) && state == 2) {
    XDrawLine(display, win, xorgc, prevx, prevy, 
	      prevx + XTextWidth(fonts[fontsize], " ", 1), prevy);
    prevx -= XTextWidth( fonts[fontsize], --bufp, 1);
    buflen--;
    XCopyArea(display, current_shell->frame, win, gc[1], 
	      prevx, prevy - fonts[fontsize]->ascent,
	      XTextWidth( fonts[fontsize], bufp, 1), 
	      fonts[fontsize]->ascent + fonts[fontsize]->descent,
	      prevx, prevy - fonts[fontsize]->ascent);
    XDrawLine(display, win, xorgc, prevx, prevy, 
	      prevx + XTextWidth(fonts[fontsize], " ", 1), prevy);
    if (buflen == 18)
      state = 1;
  }    
}

void do_clear() {
  if (current_shell->grmode == GRREAD)
    XBell(display, 0);
  else {
    bufp = buffer;
    buflen = 0;
    bufp = putcom(bufp);
    *bufp++ = 'C';
    *bufp++ = 'L';
    buflen += 2;
    send_buffer();
  }
}

/* Change all gc's for new linewidth */
void update_linewidth() {
  int i;
  gcvalues.line_width = linewidth;
  for (i = 0; i < NUMCOLS; i++)
    XChangeGC(display, gc[i], GCLineWidth, &gcvalues);
}

/* Get font if necassary then update all gc's with new font */
void update_fontsize() {
  int i;
  if (fonts[fontsize] == NULL) {
    sprintf(fontname, TEXTFONT, fontsizes[fontsize]);
    fonts[fontsize] = XLoadQueryFont(display, fontname);
  }
  gcvalues.font = fonts[fontsize]->fid;
  for (i = 0; i < NUMCOLS; i++)
    XChangeGC(display, gc[i], GCFont, &gcvalues);
}

void end_current() {
  switch(tool) {
  case LINETOOL:
    if (state > 0)
      lineend();
    break;
  case POLYTOOL:
  case FPOLYTOOL:
    if (state > 0)
      polyend();
    break;
  case TEXTTOOL:
    if (state > 0)
      textend();
    break;
  }
}

/* Big horrible event handling loop type thing */
void xevent_handler() 
{
  int i, tmp;
  XEvent event;
  KeySym key;
  char text;

  if (display != NULL && current_shell->frame != 0) {
    while (XPending(display)) {
      XNextEvent(display, &event);
      if (event.xany.window == win) {
	if (current_shell->grmode == GRREAD) {
	  if (event.type == Expose)
	    XCopyArea(display, current_shell->frame, win,gc[1],event.xexpose.x,
		      event.xexpose.y,event.xexpose.width,event.xexpose.height,
		      event.xexpose.x, event.xexpose.y);
	  else if (event.type == ButtonPress)
	    XBell(display, 0);
	}
	else {
	  switch (event.type) {
	  case Expose:
	    XCopyArea(display, current_shell->frame, win,gc[1],event.xexpose.x,
		      event.xexpose.y,event.xexpose.width,event.xexpose.height,
		      event.xexpose.x, event.xexpose.y);
	    break;
	  case ButtonPress:
	    switch (tool) {
	    case DRAWTOOL:
	      if (state == 0)
		drawstart(event.xbutton.x, event.xbutton.y);
	      break;
	    case LINETOOL:
	      if (event.xbutton.button == 1)
		if (state == 0)
		  linestart(event.xbutton.x, event.xbutton.y);
		else 
		  lineadd(event.xbutton.x, event.xbutton.y);
	      else
		if (state > 0)
		  lineend();
	      break;
	    case SPRAYTOOL:
	      if (state > 0)
		sprayend();
	      spraystart(event.xbutton.x,event.xbutton.y,event.xbutton.button);
	      break;
	      /*	  case FLOODTOOL:
			  if (state > 0 && event.xbutton.button != state)
			  sendfill(event.xbutton.x, event.xbutton.y);
			  else if (state == 0)
			  state = event.xbutton.button;
			  break;*/
	    case CIRCLETOOL:
	      if (event.xbutton.button == 1)
		circlestart(event.xbutton.x, event.xbutton.y, 0);
	      break;
	    case FCIRCLETOOL:
	      if (event.xbutton.button == 1)
		circlestart(event.xbutton.x, event.xbutton.y, 1);
	      break;
	    case RECTTOOL:
	      if (event.xbutton.button == 1)
		rectanglestart(event.xbutton.x, event.xbutton.y, 0);
	      break;
	    case FRECTTOOL:
	      if (event.xbutton.button == 1)
		rectanglestart(event.xbutton.x, event.xbutton.y, 1);
	      break;
	    case POLYTOOL:
	      if (event.xbutton.button == 1)
		if (state == 0)
		  polystart(event.xbutton.x, event.xbutton.y, 0);
		else 
		  polyadd(event.xbutton.x, event.xbutton.y);
	      else
		if (state > 0)
		  polyend();
	      break;
	    case FPOLYTOOL:
	      if (event.xbutton.button == 1)
		if (state == 0)
		  polystart(event.xbutton.x, event.xbutton.y, 1);
		else 
		  polyadd(event.xbutton.x, event.xbutton.y);
	      else
		if (state > 0)
		  polyend();
	      break;
	    case TEXTTOOL:
	      if (event.xbutton.button == 1)
		if (state == 0) {
		  state = 1;
		  textstart(event.xbutton.x, event.xbutton.y);
		}
		else if (state != 0) {
		  state = 0;
		  redisplay();
		}
	    }
	    break;
	  case ButtonRelease:
	    switch (tool) {
	    case DRAWTOOL:
	      if (state == 1)
		drawpoint(event.xbutton.x, event.xbutton.y);
	      drawend();
	      break;
	    case SPRAYTOOL:
	      if (event.xbutton.button == state)
		sprayend();
	      break;
	      /*	  case FLOODTOOL:
			  if (event.xbutton.button == state)
			  state = 0;
			  break;*/
	    case CIRCLETOOL:
	    case FCIRCLETOOL:
	      if (event.xbutton.button == 1 && state == 1)
		circleend(event.xbutton.x, event.xbutton.y);
	      break;
	    case RECTTOOL:
	    case FRECTTOOL:
	      if (event.xbutton.button == 1 && state == 1)
		rectangleend(event.xbutton.x, event.xbutton.y);
	      break;
	    }
	    break;
	  case MotionNotify:
	    if (state > 0)
	      switch(tool) {
	      case DRAWTOOL:
		drawpoint(event.xmotion.x, event.xmotion.y);
		break;
	      case LINETOOL:
		linerubber(event.xmotion.x, event.xmotion.y);
		break;
	      case SPRAYTOOL:
		spraypoint(event.xmotion.x, event.xmotion.y);
		break;
	      case CIRCLETOOL:
	      case FCIRCLETOOL:
		circlerubber(event.xmotion.x, event.xmotion.y);
		break;
	      case RECTTOOL:
	    case FRECTTOOL:
		rectanglerubber(event.xmotion.x, event.xmotion.y);
		break;
	      case POLYTOOL:
	      case FPOLYTOOL:
		polyrubber(event.xmotion.x, event.xmotion.y);
		break;
	      }
	    break;
	  case KeyPress:
	    if (tool == TEXTTOOL && state > 0) 
	      if (XLookupString(&event.xkey, &text, 1, &key, 0))
		textadd(text);
	    break;
	  }
	}
      }
      else {
        switch (event.type) {
	case ButtonPress:
	  for (i = 0; i < NUMCOLS; i++) {
	    if (event.xbutton.window == colwins[i]) {
	      end_current();
	      col = i;
	      draw_current_col();
	    }
	  }
          for (i = 0; i < NUMTOOLS; i++) {
	    if (event.xbutton.window == toolwins[i] && i != tool) {
	      end_current();
	      tmp = tool; 
	      tool = i;
	      draw_tool(i);
	      draw_tool(tmp);
	      if (tool == TEXTTOOL || tmp == TEXTTOOL)
		draw_brush_size();
	      switch(tool) {
	      case DRAWTOOL:
		cursor = XCreateFontCursor(display, XC_pencil);
		break;
	      case SPRAYTOOL:
		cursor = XCreateFontCursor(display, XC_spraycan);
		break;
/*	      case FLOODTOOL:
		cursor = XCreateFontCursor(display, XC_diamond_cross);
		break;*/
	      default:
		cursor = XCreateFontCursor(display, XC_cross);
	      break;
	      }
	      XDefineCursor(display, win, cursor);
	    }
	  }
	  if (event.xbutton.window == brushdecwin) {
	    end_current();
	    if (tool == TEXTTOOL) {
	      if (fontsize > 0) {
		fontsize -= event.xbutton.button;
		if (fontsize < 0)
		  fontsize = 0;
		update_fontsize();
		draw_brush_size();
	      }
	    }
	    else {
	      if (linewidth > 1) {
		linewidth -= (event.xbutton.button - 1) * 5 + 1;
		if (linewidth < 1)
		  linewidth = 1;
		update_linewidth();
		draw_brush_size();
	      }
	    }
	  }
	  else if (event.xbutton.window == brushincwin) {
	    end_current();
	    if (tool == TEXTTOOL) {
	      if (fontsize < NUMFONTSIZES - 1) {
		fontsize += event.xbutton.button;
		if (fontsize >= NUMFONTSIZES)
		  fontsize = NUMFONTSIZES - 1;
		update_fontsize();
		draw_brush_size();
	      }
	    }
	    else {
	      if (linewidth < MAXLINEWIDTH) {
		linewidth+= (event.xbutton.button - 1) * 5 + 1;
		if (linewidth > MAXLINEWIDTH)
		  linewidth = MAXLINEWIDTH;
		update_linewidth();
		draw_brush_size();
	      }
	    }
	  }
          else if(event.xbutton.window == clearwin) {
	    end_current();
            do_clear();
          }
	  break;
	case ConfigureNotify:
	  bsxresize(&event);
	  break;
	case Expose:
          for (i = 0; i < NUMTOOLS; i++) {
            if (event.xexpose.window == toolwins[i])
              if (event.xexpose.count == 0)
		draw_tool(i);
	  }
	  for (i=0; i < NUMCOLS; i++) {
	    if (event.xexpose.window == colwins[i])
	      if (event.xexpose.count == 0)
		XFillRectangle(display, colwins[i], gc[i],0,0,COLSIZE,
			       BORDERSIZE/2-1);
	  }
	  if (bsxexpose(&event))
	    ;
	  else if (event.xexpose.window == currentcolwin) {
	    if (event.xexpose.count == 0) {
	      draw_current_col();
	    }
	  }
	  else if (event.xexpose.window == brushsizewin) {
	    if (event.xexpose.count == 0)
	      draw_brush_size();
	  }
	  else if (event.xexpose.window == brushdecwin) {
	    if (event.xexpose.count == 0)
	      draw_left_arrow(brushdecwin);
	  }
	  else if (event.xexpose.window == brushincwin) {
	    if (event.xexpose.count == 0)
	      draw_right_arrow(brushincwin);
	  }
          else if (event.xexpose.window == clearwin) {
            if (event.xexpose.count == 0)
              draw_textbutton(clearwin, "CLEAR");
	  }
	  break;
	}
      }
    }
  }
  else
    if (display != NULL)
      while (XPending(display)) {
        XNextEvent(display, &event);
        if (event.type == Expose && event.xany.window != win) {
          for (i = 0; i < NUMTOOLS; i++) {
            if (event.xexpose.window == toolwins[i])
              if (event.xexpose.count == 0)
		draw_tool(i);
	  }
	  if (event.xexpose.window == currentcolwin) {
	    XFillRectangle(display, currentcolwin, gc[col], event.xexpose.x,
		   event.xexpose.y, event.xexpose.width, event.xexpose.height);
	  }
	  else if (event.xexpose.window == brushsizewin) {
	    if (event.xexpose.count == 0)
	      draw_brush_size();
	  }
	  else if (event.xexpose.window == brushdecwin) {
	    if (event.xexpose.count == 0)
	      draw_left_arrow(brushdecwin);
	  }
	  else if (event.xexpose.window == brushincwin) {
	    if (event.xexpose.count == 0)
	      draw_right_arrow(brushincwin);
	  }
          else if (event.xexpose.window == clearwin) {
            if (event.xexpose.count == 0)
              draw_textbutton(clearwin, "CLEAR");
	  }
        }
      } 
}

/* Close the connection to the display */
void closedisplay() {
  if (display != NULL)
    XCloseDisplay(display);
}

/* Free a frame, and remove the whole window if it was the last one */
void blankpixmap(frame) 
Pixmap *frame;
{
  shell *test;
  int clear = 0;
  int bsxclear = 0;
  if (*frame != 0)
    XFreePixmap(display, *frame);
  *frame = 0;
  test = first_shell;
  for (test = first_shell;test;test = test->next)
    if (test->frame && test->grmode > GRBSX)
      clear = 1;
    else if (test->frame && test->grmode == GRBSX)
      bsxclear = 1;
  if (!clear && mainwin) {
    XUnmapWindow(display, mainwin);
    mainwinmapped = 0;
  }
  if (!bsxclear && bsxwin) {
    XUnmapWindow(display, bsxwin);
    bsxwinmapped = 0;
  }
}
#endif



