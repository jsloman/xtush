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

#define NUMBSXCOLS 16
#define BSXWINWIDTH 1024
#define BSXWINHEIGHT 512
#define DBASESIZE 64
#define MAXDESCSIZE 32
#define MAXIDLEN 64
#define MAXCROWD 32

extern void sent_out();
extern shell_print();
extern void open_display();
extern Display *display;
extern Colormap cmap;
extern Cursor cursor;
extern int colour;
extern int screen;
extern unsigned long whitep, blackp;
extern shell *current_shell;
extern XGCValues gcvalues;
Window bsxwin = 0;
extern int bsxwinmapped;
GC bsxgc[NUMBSXCOLS];
XColor bsxcolours[NUMBSXCOLS];
int bsxwinx, bsxwiny;
char logging = 1;

struct polygon {                /* Structure describing 1 poly */
  unsigned char size;
  unsigned char colour;
  int x[32];
  int y[32];
};

struct desc {   /* Structure describing a scene of several polys */
  char id[MAXIDLEN];
  struct polygon poly[MAXDESCSIZE];
  unsigned char descsize;
};

struct object {
  int num;
  int depth;
  int position;
  char id[MAXIDLEN];
};

char bsxcolnames[NUMBSXCOLS][20] = { "Black", "Blue", "ForestGreen", "SkyBlue", "IndianRed", "HotPink", "Brown", "LightGrey", "DimGrey", "DeepSkyBlue", "Green", "Cyan", "Tomato", "Magenta", "Yellow", "White" };

static char stippledata[][8] = {
  { 0xff, 0xcc, 0xff, 0x33, 0xff, 0xcc, 0xff, 0x33 }, /* Blue */
  { 0xdd, 0xdd, 0x77, 0x77, 0xdd, 0xdd, 0x77, 0x77 }, /* ForestGreen */
  { 0x00, 0x33, 0x00, 0xcc, 0x00, 0x33, 0x00, 0xcc }, /* SkyBlue */
  { 0x77, 0xbb, 0xee, 0x77, 0xdd, 0xee, 0xbb, 0xdd }, /* IndianRed */
  { 0x88, 0x44, 0x11, 0x88, 0x22, 0x11, 0x44, 0x22 }, /* HotPink */
  { 0xfb, 0xdf, 0xfe, 0xf7, 0xbf, 0xfd, 0xef, 0x7f }, /* Brown */
  { 0x15, 0x88, 0x55, 0x22, 0x55, 0x88, 0x55, 0x22 }, /* LightGrey */
  { 0x55, 0xee, 0x55, 0xbb, 0x55, 0xee, 0x55, 0xbb }, /* DimGrey */
  { 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00 }, /* DeepSkyBlue */
  { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 }, /* Green */
  { 0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00, 0xaa }, /* Cyan */
  { 0x77, 0xbb, 0xdd, 0xee, 0x77, 0xbb, 0xdd, 0xee }, /* Tomato */
  { 0xf3, 0xfc, 0x3f, 0xcf, 0xf3, 0xfc, 0x3f, 0xcf }, /* Magenta */
  { 0x04, 0x20, 0x01, 0x08, 0x40, 0x02, 0x10, 0x80 }, /* Yellow */
};

struct desc dbase[DBASESIZE];          /* Last recently used scenes */
struct object objects[MAXCROWD];
int numobjects = 0;
int nextfree = 0;
int current_scene = -1;
char scene_id[MAXIDLEN];

void bsx_redisplay() {
  if (current_shell->grmode == GRBSX)
    if (current_shell->frame != 0)
      XCopyArea(display, current_shell->frame, bsxwin, bsxgc[1], 0, 0, 
		bsxwinx, bsxwiny, 0, 0);
    else
	XFillRectangle(display,bsxwin,bsxgc[0],0,0,bsxwinx,bsxwiny);
}

void create_bsxwindow()
{
  XWMHints wmhints;
  XSizeHints xsh;
  XColor dummy;
  int i; 

  bsxwinx = BSXWINWIDTH / 2;
  bsxwiny = BSXWINHEIGHT / 2;
  bsxwin = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 
             bsxwinx, bsxwiny, 0, blackp, whitep);
  xsh.flags = PSize | PMaxSize | PMinSize;
  xsh.width = bsxwinx;
  xsh.height = bsxwiny;
  xsh.max_width = BSXWINWIDTH;
  xsh.max_height = BSXWINHEIGHT;
  xsh.min_width = BSXWINWIDTH >> 3;
  xsh.min_height = BSXWINHEIGHT >> 3;
  XSetWMNormalHints(display, bsxwin, &xsh);
  wmhints.flags = InputHint;
  wmhints.input = False;
  XSetWMHints(display, bsxwin, &wmhints);
  XStoreName(display, bsxwin, "bsx graphics");

  cursor = XCreateFontCursor(display, XC_gumby);
  XDefineCursor(display, bsxwin, cursor);

  for (i = 0; i < NUMBSXCOLS; i++) {
    if (colour || i == 0 || i == NUMBSXCOLS-1) {
      XAllocNamedColor(display, cmap, bsxcolnames[i], &bsxcolours[i], &dummy);
      gcvalues.foreground = bsxcolours[i].pixel;
      bsxgc[i] = XCreateGC(display, RootWindow(display, screen), GCForeground,
			   &gcvalues);
    }
    else {
      gcvalues.stipple = XCreateBitmapFromData(display, RootWindow(display, screen), stippledata[i-1], 8, 8);
      gcvalues.fill_style = FillOpaqueStippled;
      gcvalues.foreground = blackp;
      gcvalues.background = whitep;
      bsxgc[i] = XCreateGC(display, RootWindow(display, screen), GCForeground
       		   | GCBackground | GCFillStyle | GCStipple, &gcvalues);
    }
  }
  XSelectInput(display, bsxwin, ExposureMask | StructureNotifyMask);
  XMapWindow(display, bsxwin);
  bsxwinmapped = 1;

  XSync(display, 1);
} 

void create_bsxframe(sh)
shell *sh;
{
  sh->frame = XCreatePixmap(display, bsxwin, BSXWINWIDTH, BSXWINHEIGHT,
                   DefaultDepth(display, screen));
  XFillRectangle(display, sh->frame, bsxgc[0], 0, 0, BSXWINWIDTH,BSXWINHEIGHT);
}

int find_id(id)         /* Check whether desc is already known */
char *id;
{
  int s;
  for (s=0; s<DBASESIZE; s++)
    if (strcmp(id,dbase[s].id)==0)
      return s;
  return(-1);
}

void visualise_poly(sh, p)
struct polygon p;
shell *sh;
{
  int i;
  XPoint q[64];
  for(i = 0; i < p.size; i++) {
    q[i].x = (p.x[i]*bsxwinx)/255;
    q[i].y = ((255 - p.y[i])*bsxwiny)/255;
  }
  XFillPolygon(display, sh->frame, bsxgc[p.colour], q, p.size, 0, 0);
}

void draw_object(sh, n)
shell *sh;
int n;
{
  struct polygon p;
  int i, j;
  for (i = 0; i < dbase[objects[n].num].descsize; i++ ) {
    p.size = dbase[objects[n].num].poly[i].size;
    p.colour = dbase[objects[n].num].poly[i].colour;
    for (j = 0; j < p.size; j++) {
      p.x[j] = dbase[objects[n].num].poly[i].x[j]-127 + objects[n].position*16;
      p.y[j] = dbase[objects[n].num].poly[i].y[j]-127 + objects[n].depth*4;
    }
    visualise_poly(sh, p);
  }
}

void draw_scene(sh, scene)
int scene;
shell *sh;
{
  int i;
  if (scene >= 0)
    current_scene = scene;
  if (current_scene >= 0)
    for (i = 0; i < dbase[current_scene].descsize; i++)
      visualise_poly(sh, dbase[current_scene].poly[i]); 
}

void draw_room(sh)
shell *sh;
{
  int i, depth;
  draw_scene(sh, -1);
  for (depth = 15; depth >= 0; depth--)
    for (i = 0; i < numobjects; i++)
      if (objects[i].num >= 0 && objects[i].depth == depth)
	draw_object(sh, i);
}

int gethex(str)
char *str;
{
  int num;
  if (*str >= '0' && *str <= '9')
    num = (int) *str - '0';
  else
    num = (int) *str - 'A' + 10;
  num *= 16;
  str++;
  if (*str >= '0' && *str <= '9')
    num += (int) *str - '0';
  else
    num += (int) *str - 'A' + 10;
  return num;
}

int checkfree(n)
int n;
{
  int i;
  if (n == current_scene)
    return 0;
  for (i = 0; i < numobjects; i++)
    if (n == objects[i].num)
      return 0;
  return 1;
}

int do_dfs(sh, str)
shell *sh;
char *str;
{
  int len, i, j;
  *((char *)strchr(str, '.')) = '\0';
  strcpy(dbase[nextfree].id, str);
  len = strlen(str) + 1;
  str += strlen(str) + 1;
  dbase[nextfree].descsize = gethex(str);
  str += 2; len += 2;
  for (i = 0; i < dbase[nextfree].descsize; i++) {
    dbase[nextfree].poly[i].size = gethex(str);
    str += 2; len += 2;
    dbase[nextfree].poly[i].colour = gethex(str);
    str += 2; len += 2;
    for (j = 0; j < dbase[nextfree].poly[i].size; j++) {
      dbase[nextfree].poly[i].x[j] = gethex(str);
      str += 2; len += 2;
      dbase[nextfree].poly[i].y[j] = gethex(str);
      str += 2; len += 2;
    }
  }
  if (strcmp(scene_id, dbase[nextfree].id) == 0)
    current_scene = nextfree;
  else {
    j = -1;
    for (i = 0; i < numobjects; i++)
      if (strcmp(objects[i].id, dbase[nextfree].id) == 0)
	j = i;
    if (j >= 0)
      objects[j].num = nextfree;
  }
  do {
    nextfree++;
    if (nextfree == DBASESIZE)
      nextfree = 0;
  } while (!checkfree(nextfree));
  return len;
}

int bsx_processing(sh, str)
shell *sh;
char *str;
{
  char reply[80];
  int len = 0, scene;
  int i;
  if (strncmp(str, "DFS", 3) == 0) {
    len = do_dfs(sh, str + 3) + 3;
  }
  if (strncmp(str, "SCE", 3) == 0) {
    *((char *)strchr(str + 3, '.')) = '\0';
    len = strlen(str) + 1;
    scene = find_id(str + 3);
    numobjects = 0;
    if (scene == -1) {
      sprintf(reply, "#RQS %s\n", str + 3);
      write(current_shell->sock_desc, reply, strlen(reply));
      strcpy(scene_id, str + 3);
    }
    else
      current_scene = scene;
  }
  if (strncmp(str, "RFS", 3) == 0) {
    if (display == NULL)
      open_display();
    if (bsxwin == 0 && display != NULL)
      create_bsxwindow();
    if (bsxwin && !bsxwinmapped) {
      XMapWindow(display, bsxwinmapped);
      bsxwinmapped = 1;
    }
    if (bsxwin && display != NULL) {
      if (sh->frame == 0)
	create_bsxframe(sh);
      draw_room(sh);
      bsx_redisplay();
    }
    len = 3;
  }
  if (strncmp(str, "RQV", 3) == 0) {
    sprintf(reply, "#VER %s %s\n", CLIENT, VERSION);
    write(current_shell->sock_desc, reply, strlen(reply));
    len = 3;
  }
  if (strncmp(str, "TXT", 3) == 0) {
    /* not implemented */
  }
  if (strncmp(str, "DFO", 3) == 0) {
    len = do_dfs(sh, str + 3) + 3;
  }
  if (strncmp(str, "VIO", 3) == 0) {
    *((char *)strchr(str + 3, '.')) = '\0';
    len = strlen(str) + 1;
    scene = find_id(str + 3);
    if (scene == -1 && numobjects < MAXCROWD) {
      sprintf(reply, "#RQO %s\n", str + 3);
      write(current_shell->sock_desc, reply, strlen(reply));
    }
    if (numobjects < MAXCROWD) {
      objects[numobjects].position = gethex(str + len);
      objects[numobjects].depth = gethex(str + len + 2);
      strcpy(objects[numobjects].id, str + 3);
      objects[numobjects].num = scene;
      numobjects++;
    }
    len += 4;
  }
  if (strncmp(str, "RMO", 3) == 0) {
    *((char *)strchr(str + 3, '.')) = '\0';
    len = strlen(str) + 1;
    scene = find_id(str + 3);
    if (scene >= 0)
      for (i = 0; i < numobjects; i++)
	if (objects[i].num == scene) {
	  numobjects--;
	  if (i < numobjects) {
	    objects[i].num = objects[numobjects].num;
	    objects[i].depth = objects[numobjects].depth;
	    objects[i].position = objects[numobjects].position;
	  }
	}
  }
  if (strncmp(str, "TMS", 3) == 0) {
    len = 3;
  }
  if (strncmp(str, "LOF", 3) == 0) {
    logging = 0;
    len = 3;
  }
  if (strncmp(str, "LON", 3) == 0) {
    logging = 1;
    len = 3;
  }
  if (strncmp(str, "PUR", 3) == 0) {
    *((char *)strchr(str + 3, '.')) = '\0';
    len = strlen(str) + 1;
    /* ignore purges */
  }
  if (strncmp(str, "PRO", 3) == 0) {
    len = 3;
  }
  if (strncmp(str, "BOM", 3) == 0) {
    *((char *)strchr(str + 3, '.')) = '\0';
    len = strlen(str) + 1;
  }
  if (strncmp(str, "EOM", 3) == 0) {
    len = 3;
  }
  return len;
}

int bsxexpose(event)
XEvent *event;
{
  if (event->xany.window == bsxwin) {
    XCopyArea(display, current_shell->frame, bsxwin,bsxgc[1],
	      event->xexpose.x, event->xexpose.y, event->xexpose.width, 
	      event->xexpose.height, event->xexpose.x, event->xexpose.y);
    return 1;
  }
  return 0;
}

void bsxresize(event)
XEvent *event;
{
  bsxwinx = event->xconfigure.width;
  bsxwiny = event->xconfigure.height;
  draw_room(current_shell);
  bsx_redisplay();
}

#endif
