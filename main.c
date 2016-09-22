/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User Shell            Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/

/* 
   This is the file main.c
*/

#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/param.h>
#include <ctype.h>
#include <malloc.h>

#ifdef GRAPHICS
#include <X11/Xlib.h>
#endif

#include "config.h"

#ifndef SIGWINCH
#define SIGWINCH SIGWINDOW
#endif

extern void redraw_screen(),centre(),print(),handle_user(),newline(),
  execute(),update(),init_term(),restore_term(),cls(),restart(),
  window_change(),draw_input();

extern int wchange;

#ifdef GRAPHICS
extern int forcecolour;
extern void xevent_handler();
extern void closedisplay();
extern void redisplay();
extern void blankpixmap();
#endif

int quit=0;
shell *first_shell=0;
int no_of_shells=0;
shell *current_shell=0;
int key_buff_len=STANDARD_KEY_BUFF_LEN;
int key_history_len=STANDARD_KEY_HISTORY_LEN;
int screen_history_len=STANDARD_SCREEN_HISTORY_LEN;
int screen_buff_len=STANDARD_SCREEN_BUFF_LEN;
void *scratch;
int scratch_size=SCRATCH_SIZE;
char time_string[80]=STANDARD_TIME_STRING;
file site_list;
int error_type=0;
void restore_all();


/* 
        some misc routines 
*/


/* in case of problems ... */

void handle_error(str)
{
  printf("\n\nERROR - %s\n\n",str);
  restore_all();
  exit(-1);
}

/* print out the time in a nice format */

char *convert_time()
{
  static char buff[80];
  time_t t;
  
  t=time(0);
  strftime(buff,80,time_string,localtime(&t));
  return buff;
}

/* change and print pathname routines */

void pwd(sh,str)
shell *sh;
char *str;
{
  /*  char pathname[MAXPATHLEN];
#ifdef SYSV
  getcwd(pathname,MAXPATHLEN);
#else
  getwd(pathname);
#endif
  shell_print(sh,pathname);
  */
}


void change_dir(sh,str)
shell *sh;
char *str;
{
  if (!*str) {
    strcpy(scratch,getenv("HOME"));
    str=scratch;
  }
  if (*str=='~') {
    sprintf(scratch,"%s%s",getenv("HOME"),str+1);
    str=scratch;
  }
  if (chdir(str)) {
    shell_print(sh,"Error trying to change directory");
    switch(errno) {
    case EACCES:
      shell_print(sh,"Permission Denied.");
      break;
    case ENOENT:
      shell_print(sh,"Path does not exist.");
      break;
    case ENOTDIR:
      shell_print(sh,"Component not a directory.");
      break;
    }
  }
  pwd(sh,"");
}



/* handle file error if it happens  */

void filio_error(sh)
shell *sh;
{
  switch (errno) {
  case ENOENT:
    switch (error_type) {
    case 0:
      shell_print(sh,"No such file or directory.");
      break;
    case 1:
      shell_print(sh,"No .tushrc file to run.");
      break;
    case 2:
      shell_print(sh,"No .tush-sites file.");
      break;
    }
    break;
  case EACCES:
    switch (error_type) {
    case 0:
      shell_print(sh,"Permission Denied.");
      break;
    case 1:
      shell_print(sh,"Permission Denied, can't execute .tushrc");
      break;
    case 2:
      shell_print(sh,"Permission Denied, can't read .tush-sites");
      break;
    }
  case EEXIST:
    shell_print(sh,"File already exists.");
    break;
  default:
    sprintf(scratch,"%s Non-specific file error No. %d",SHELL_PRINT,errno);
    print(sh,scratch);
    break;
  }
}

/* load a file */

file load_file(sh,filename)
shell *sh;
char *filename;
{
  file f;
  int d,e;

  d=open(filename,O_RDONLY);
  if (d<0) {
    filio_error(sh);
    f.where=0;
    f.length=0;
    return f;
  }
  f.length=lseek(d,0,2);
  lseek(d,0,0);
  f.where=(char *)malloc(f.length+1);
  if (read(d,f.where,f.length)<0) {
    filio_error(sh);
    f.where=0;
    f.length=0;
    return f;
  }
  close(d);
 return f;
}

/* routines for scripting */

void stop_script(sh,str)
shell *sh;
char *str;
{
  if (sh->script_stream) {
    sprintf(scratch,"%s Scripting stopped %s.\n",SHELL_PRINT,convert_time());
    print(sh,scratch);
    fclose(sh->script_stream);
    sh->script_stream = 0;
  }
  else shell_print(sh,"Not scripting.");
}

void script(sh,str)
shell *sh;
char *str;
{
  if (sh->script_stream) {
    if (!fwrite(str,1,strlen(str),sh->script_stream)) {
      fclose(sh->script_stream);
      sh->script_stream = 0;
      shell_print(sh,"Scripting stopped due to error.");
      filio_error(sh);
    }
  }
}

void start_script(sh,str)
shell *sh;
char *str;
{
  FILE *stream;
  if (sh->script_stream) {
    stop_script(sh,str);
    return;
  }
  if (!*str) str=SCRIPT_FILE;
  stream=fopen(str,"a");
  if (!stream) {
    filio_error(sh);
    return;
  }
  fseek(stream,0,2);
  sh->script_stream=stream;
  sprintf(scratch,"%s Scripting started %s.\n",SHELL_PRINT,convert_time());
  print(sh,scratch);
}

/* switches to a different shell */

void switch_shell(sh)
shell *sh;
{
  current_shell=sh;
  if (sh != NULL)
    sh->flags &= ~MONITOR_GONEOFF;
#ifdef GRAPHICS
  if (sh != NULL)
    redisplay();
#endif
}

/* starts up a new shell, and switches to it */

shell *new_shell(str)
char *str;
{
  shell *new,*empty;

  new=(shell *)malloc(sizeof(shell));
  no_of_shells++;
  empty=first_shell;
  if (!empty) first_shell=new;
  else {
    while(empty->next) empty=empty->next;
    empty->next=new;
  }
  new->next=0;
  new->key_buff=(char *)malloc(key_buff_len);
  memset(new->key_buff,0,key_buff_len);
  new->screen_buff=(char *)malloc(screen_buff_len);
  memset(new->screen_buff,0,screen_buff_len);
  new->key_history=(char *)malloc(key_history_len);
  memset(new->key_history,0,key_history_len);
  new->screen_history=(char *)malloc(screen_history_len);
  memset(new->screen_history,0,screen_history_len);

  new->flags=COMMAND|NO_UPDATE|WORD_WRAP|GA_PROMPT;
  strncpy(new->prompt,STANDARD_PROMPT,PROMPT_LEN);
  new->kb_pointer=new->key_buff;
  new->sb_pointer=0;
  new->kh_pointer=0;
  new->kh_search=0;
  new->sh_pointer=0;
  new->script_stream = 0;
  new->alias_list=0;
  new->trigger_list=0;
  new->max_word=10;
  new->col=0;
  new->lastwasnl = 0;
  new->lastwaslf = 0;
  new->telnetcontrol = 0;
  new->len = 0;
#ifdef GRAPHICS
  new->frame=0;
  new->grmode = GRNONE;
  new->graphics = 0;
  new->bsxgraphics = 0;
  new->graphicslen = 0;
#endif

  switch_shell(new);
  cls();

  print(new,SHELL_HEADER);
  
  sprintf(scratch,"%s/%s",getenv("HOME"),STARTUP_FILE);
  error_type=1;
  execute(new,scratch);
  error_type=0;
  command_processing(new,str);
  delete_input(new);
  new->flags &= ~NO_UPDATE;
  return new;
}

/* shuts down a shell */

void close_shell(s)
shell *s;
{
  shell *p,*n;

  n=s->next;
  if (s!=first_shell) {
    p=first_shell;
    while(p->next!=s) p=p->next;
    p->next=n;
  }
  else first_shell=n;
  no_of_shells--;

  if (s==current_shell) {
    if (n) switch_shell(n);
    else switch_shell(first_shell);
    redraw_screen();
  }

  if (s->script_stream) stop_script(s,0);

  if (s->flags&CONNECTED) close(s->sock_desc);

  free((void *)s->key_buff);
  free((void *)s->screen_buff);
  free((void *)s->key_history);
  free((void *)s->screen_history);
#ifdef GRAPHICS
  blankpixmap(&(s->frame));
#endif
  free((void *)s);
}


/* handle ^c */

void control_c()
{
  signal(SIGINT,control_c);
  current_shell->flags |= CTRL_C;
  shell_print(current_shell,"^C Interrupt.");
}



/* start everything off */

void init_all()
{
  int i;
  char *scan;

#ifndef NO_FANCY_MALLOC
  mallopt(M_MXFAST,MAX_SMALL_BLOCK);
#endif

  scratch=malloc(SCRATCH_SIZE);
  init_term();
  signal(SIGPIPE,SIG_IGN);
  signal(SIGWINCH,window_change);
  signal(SIGCONT,restart);
  signal(SIGINT,control_c);

  sprintf(scratch,"%s/%s",getenv("HOME"),SITE_FILE);
  error_type=2;
  site_list=load_file(current_shell,scratch);
  error_type=0;

  scan=site_list.where;
  for(i=0;i<site_list.length;i++,scan++)
    if (isspace(*scan) && (*scan!='\n')) *scan=0;
  
  new_shell("");
}

/* and restore it all afterwards */

void restore_all()
{
  while(first_shell) 
    close_shell(first_shell);
  restore_term();
}

/* gotta have main() somewhere */

void main(argc,argv) 
int argc;
char *argv[];
{
  int i;
  fd_set set,*p;
  char *compose,*arg;
  shell *scan;
  struct timeval timeout;

  init_all();

  compose=argv[0];
  *compose++=toupper(*compose);
  *compose=toupper(*compose);


  p=&set;
  i = 1;
#ifdef GRAPHICS
  if (argc >= 2) {
    if (!strcmp(argv[1], "mono")) {
      forcecolour = 0;
      i++;
    }
    else if (!strcmp(argv[1], "true")) {
      forcecolour = 2;
      i++;
    }
  }
#endif
  for(compose=scratch;i<argc;i++) {
    arg=argv[i];
    while(*arg) *compose++ = *arg++;
    *compose++=' ';
  }
  *compose=0;

  strncpy(current_shell->key_buff,scratch,key_buff_len);
  current_shell->flags |= NO_UPDATE;
  newline();

  while(!quit) {

    FD_ZERO(p);
    FD_SET(0,p);

    for(scan=first_shell;scan;scan=scan->next) {
      if (scan->flags&CONNECTED) FD_SET(scan->sock_desc,p);
    }
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;
    if ((select(FD_SETSIZE,p,0,0,&timeout) > 0)) {
      for(scan=first_shell;scan;scan=scan->next) {
	if ((scan->flags&CONNECTED) && (FD_ISSET(scan->sock_desc,p))) 
	  read_in(scan);
      }
	

      if (FD_ISSET(0,p)) {
	if (current_shell->flags&HALF_LINE) {
	  current_shell->flags&=~HALF_LINE;
	  current_shell->flags|=NO_UPDATE;
	  print(current_shell,"\n");
	  current_shell->flags&=~NO_UPDATE;
	  draw_input(current_shell);
	}
	handle_user();
      }
    }
    if (wchange) restart();
#ifdef GRAPHICS
    xevent_handler(current_shell);
#endif
  }
  restore_all();
  printf("\n");
#ifdef GRAPHICS
  closedisplay();
#endif
}


