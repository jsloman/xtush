/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User Shell            Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/

/* 
   This is the file vscreen.c

   This contains screen and terminal handling routines
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#ifndef LINUX
#ifndef NO_FILIO
#include <sys/filio.h>
#endif
#endif
#include <sys/ioctl.h>
#ifdef SYSV
#include <termios.h>
#else
#ifdef LINUX
#include <bsd/sgtty.h>
#else
#include <sgtty.h>
#endif
#endif

#ifdef GRAPHICS
#include <X11/Xlib.h>
#endif

#include "config.h"

extern shell *current_shell,*first_shell;
extern int key_buff_len,quit,scratch_size;
extern void store_key_history(),user_processing(),command_processing(),
        do_history(),add_screen_history(),script();
extern void *scratch;
extern char *compare();

int wchange=0;

void hilight_in_current();

/* this array defines what each control char does */

/* must declare them first */                         

void null(), back_char(),forward_char(),newline(),toggle_command(),break_out()
,delete(),backspace(),ccomplete(),create_shell(),quit_shell(),go_next_shell()
,go_previous_shell(),del_line(),go_start_line(),transpose(),go_end_line()
,repaint(),toggle_secrecy();

extern void next_history();
extern void prev_history();

void *control_fns[]={
        null,                 /* 0 */
        go_start_line,        /* 1  - ^A start of line */
        back_char,            /* 2  - ^B back character */
        break_out,            /* 3  - ^C break */
	delete,               /* 4  - ^D delete */
        go_end_line,          /* 5  - ^E end of line */                 
        forward_char,         /* 6  - ^F forward character */
        null,                 /* 7  - ^G */                                
        backspace,            /* 8  - ^H backspace */
        ccomplete,            /* 9  - ^I command completion (tab) */
        newline,              /* 10 - ^J newline */                      
        del_line,             /* 11 - ^K delete line */ 
        repaint,              /* 12 - ^L repaint screen */
        newline,              /* 13 - ^M newline */            
        next_history,         /* 14 - ^N next history */  
        null,                 /* 15 - ^O */
        prev_history,         /* 16 - ^P previous history */
        null,                 /* 17 - ^Q */
        create_shell,         /* 18 - ^R new shell */ 
        null,                 /* 19 - ^S */
        transpose,            /* 20 - ^T transpose two chars */
        go_next_shell,        /* 21 - ^U next shell */
        null,                 /* 22 - ^V */                                
        toggle_secrecy,       /* 23 - ^W toggle secrecy */
        quit_shell,           /* 24 - ^X exit shell */
        go_previous_shell,    /* 25 - ^Y previous shell */
        null,                 /* 26 - ^Z (suspend) */ 
        toggle_command,       /* 27 - ESC  toggle command */  
        null,                 /* null */
        null,                 /* 29 */  
        null,                 /* 30 */  
        null, };          /* 31 */  


/* routine used by tputs to write out termcap strings */

int tput_write_fn(c)
char c;
{
  putchar(c);
}     


/* routines for initialisation and restoration of the terminal */


char *tcaps[10];
char *term_buff,*tcaps_buff,*term_name;
int lines,cols;


#define TERM_CLS 0
#define TERM_CLEAR_DOWN 1
#define TERM_BELL 2
#define TERM_PCURS 3
#define TERM_BOLD 4
#define TERM_OFF 5

void init_term()
{
  char *area,*start;
  int index[10],i,len,got_lites;

#ifdef SYSV
  struct termio terminal;
#else
  struct sgttyb terminal;
#endif
  struct winsize new_size;

#ifdef SYSV
  if (ioctl(0,TCGETA,&terminal)) handle_error("terminal ioctl\n");
  terminal.c_lflag &= ~(ICANON|ECHO);
  terminal.c_cc[VMIN]=1;
  terminal.c_cc[VTIME]=1;
  if (ioctl(0,TCSETA,&terminal)) handle_error("terminal ioctl\n");
#else
  if (ioctl(0,TIOCGETP,&terminal)) handle_error("terminal ioctl\n");
  terminal.sg_flags |= CBREAK;
  terminal.sg_flags &= ~ECHO;
  if (ioctl(0,TIOCSETP,&terminal)) handle_error("terminal ioctl\n");
#endif

  setbuf(stdout,0);

  term_buff=malloc(1024);
  term_name=getenv("TERM");
  if (tgetent(term_buff,term_name)!=1) 
    handle_error("Can't find termcap entry for this terminal\n");
  
  cols=tgetnum("co");
  lines=tgetnum("li");
  
  area=scratch;
  start=scratch;
  index[TERM_CLS]=(area-start);
  if (!tgetstr("cl",&area)) 
    handle_error("Can't find 'cl' code (cls) in termcap. (this is essential to TUsh)\n");
  *area++=0;
  index[TERM_CLEAR_DOWN]=(area-start);
  if (!tgetstr("cd",&area)) 
    handle_error("Can't find 'cd' code (clear to end) in termcap (this is essential to TUsh).\n");
  *area++=0;
  index[TERM_BELL]=(area-start);
  tgetstr("bl",&area);
  *area++=0;
  index[TERM_PCURS]=(area-start);
  if (!tgetstr("cm",&area)) 
    handle_error("Cant find 'cm' code (cursor movement) in termcap (this is essntial to TUsh.\n");
  *area++=0;
  got_lites=1;
  index[TERM_BOLD]=(area-start);
  if (!tgetstr("md",&area)) 
    if (!tgetstr("mr",&area)) {
      printf("Warning: Can't find termcap entries for high-lighting.\n");
      *area++=0;
      got_lites=0;
    }
  index[TERM_OFF]=(area-start);
  if (got_lites) {
    if (!tgetstr("me",&area)) {
      printf("Warning: Can't turn high-lights off, disabling.\n");
      *(start+index[TERM_BOLD])=0;
    }
  }
  *area++=0;
  
  len=area-start;
  tcaps_buff=malloc(len);
  memcpy(tcaps_buff,start,len);
  for(i=0;i<10;i++) tcaps[i]=tcaps_buff+index[i];
}


/* this may be called from the error handler so no traps for errors */

void restore_term()
{
#ifdef SYSV
  struct termio terminal;
  
  if (ioctl(0,TCGETA,&terminal)) handle_error("terminal ioctl\n");
  terminal.c_lflag |= (ICANON|ECHO);
  if (ioctl(0,TCSETA,&terminal)) handle_error("terminal ioctl\n");
  
#else
  
  struct sgttyb terminal;
  
  ioctl(0,TIOCGETP,&terminal);
  terminal.sg_flags &= ~CBREAK;
  terminal.sg_flags |= ECHO;
  ioctl(0,TIOCSETP,&terminal);
  
#endif
  
  free(tcaps_buff);
  free(term_buff);
}

void move_cursor(x,y)
{
  char *str;
  str=(char *)tgoto(tcaps[TERM_PCURS],x,y);
  tputs(str,1,tput_write_fn);
}

void position_cursor(sh)
shell *sh;
{
  int comlen,len,rowsup,colsacross;
  if (sh!=current_shell) return;
  if (sh->flags&COMMAND) comlen=strlen(COMMAND_PROMPT);
  else comlen=strlen(sh->prompt);
  
  len=strlen(sh->key_buff)+comlen;
    
  rowsup=lines-len/cols-1;
  rowsup+=(comlen+sh->curspos)/cols;
  colsacross=(sh->curspos+comlen)%cols;
  move_cursor(colsacross,rowsup);
}

/* deletes user input */

void delete_input(sh)
shell *sh;
{
 int len,rowsup;
 if (sh==current_shell) {
   len=strlen(sh->key_buff);
   if (sh->flags&COMMAND) len+=strlen(COMMAND_PROMPT);
   else len+=strlen(sh->prompt);
   rowsup=lines-len/cols-1;
   move_cursor(0,rowsup);
   tputs(tcaps[TERM_CLEAR_DOWN],1,tput_write_fn);
 }
}

void draw_key_buff(sh,start,length)
char *start;
int length;
shell *sh;
{
  int i;
  if (sh!=current_shell) return;
  if (sh->flags&SECRET)
    for(i=0;i<length;i++) putchar('*');
  else
    fwrite(start,1,length,stdout);
}

/* draws user input */

void draw_input(sh)
shell *sh;
{
  if (sh!=current_shell) return;
  if (sh->flags&COMMAND) fwrite(COMMAND_PROMPT,1,strlen(COMMAND_PROMPT),stdout);
  else fwrite(sh->prompt,1,strlen(sh->prompt),stdout);
  draw_key_buff(sh,sh->key_buff,strlen(sh->key_buff));
  putchar(' ');
  position_cursor(sh);
}

/* output text to virtual screen */

void print(sh,str)
char *str;
shell *sh;
{
  char *from, *t;
  int i, count;
  shell *scan;
  
  if (!sh) {
    fwrite(str,1,strlen(str),stdout);
    return;
  }  
  if (sh->flags&NO_SCREEN_OUTPUT) return;
  if  (sh->flags&WORD_WRAP) {
    from=(char *)scratch+scratch_size-2-strlen(str);
    strcpy(from,str);
    sh->col=0;
    str=from;
    while(*from) {
      if (*from=='\n') sh->col=-1;
      if (sh->col >= cols) {
	i=0;
        while(*from && (sh->col>0) && (*from!=' ')) {
          from--;
          i++;
          sh->col--;
        }
        if (!*from) break;
        if (*from==' ' && (i<sh->max_word)) {
          *from++='\n';
          sh->col=0;
        }
        else {
          if (*from==' ') from++;
          while(*from && (*from!=' ') && (*from!='\n')) {
            from++;
            sh->col = (sh->col+1) % cols;
          }
        }
      }
      else {
	/* what follows is a nasty bit of code to try to recognise ctrl
	   char type sequences so that they don't get counted on line wrap*/
	if (*from == 27)  /* escape, as used in control chars */
	  if (*(from+1) == '[') {
	    count = 0;
	    for (t = from + 2; *t && *t != '\n' && *t != 27 && *t != 'm';
		 count++,t++);
	    if (count > 6 || *t != 'm')
	      sh->col -= 2; /* didn't recognise an ansi type string, assume 2*/
	    else
	      sh->col -= 2+count;
	  }
	  else
	    sh->col -= 2; /* take off 2 chars as they wont be printed */
	else
	  sh->col++;
        from++;
      }
    }
  }

  if (sh==current_shell) {
    if (sh->flags&NO_UPDATE) fwrite(str,1,strlen(str),stdout);
    else {
      delete_input(sh);
      fwrite(str,1,strlen(str),stdout);
      if (!(sh->flags&HALF_LINE)) draw_input(sh);
    }
  }
  else {
    if (current_shell->flags&SEE_BELL) {
      for(from=str;*from;from++)
	if (*from==7) {
	  char tmp[80];
	  unsigned int flags;
	  flags=current_shell->flags&WORD_WRAP;
	  current_shell->flags &= ~WORD_WRAP;
	  for(i=1,scan=first_shell;scan;i++,scan=scan->next) if (scan==sh) break;
	  sprintf(tmp,"%s Bell in shell number %d.\007",SHELL_PRINT,i);
	  hilight_in_current(sh,tmp);
	  current_shell->flags |= flags;
	  break;
	}
    }
  }
  if (!(sh->flags&HISTORY_OFF)) add_screen_history(sh,str);
  if (sh->script_stream) script(sh,str);
}

/* output text with header */

void shell_print(sh,str)
shell *sh;
char *str;
{
  sprintf(scratch,"%s %s\n",SHELL_PRINT,str);
  print(sh,scratch);
}

/* update input line */

void update(sh)
shell *sh;
{
  int comlen,len,rowsup;
  if (sh==current_shell) {
    if (sh->flags&COMMAND) comlen=strlen(COMMAND_PROMPT);
    else comlen=strlen(sh->prompt);
    len=strlen(sh->key_buff)+comlen;
    rowsup=lines-len/cols-1;
    move_cursor(0,rowsup);
    draw_input(sh);
  }
}

/* echo character to screen */

void echo_to_scr(sh,c)
shell *sh;
char c;
{
  if (sh==current_shell) 
    putchar(c);
}


/* redraws everything on the screen from scratch */

void redraw_screen()
{
  char number[5];
  if (current_shell) {
    tputs(tcaps[TERM_CLS],1,tput_write_fn);
    sprintf(number,"%d",lines-1);
    refresh_shell(current_shell,number);
  }
}

void repaint()
{
  redraw_screen();
  update(current_shell);
}


/*
        user input routines 
*/


/* what happens if a normal character is entered */

void normal_char(c)
char c;
{
  shell *sh;
  char *move,*end;
  int count;
  sh=current_shell;
  sh->flags |= I_CHANGE;
  if (!(sh->flags&NO_INPUT)) {
    end=sh->key_buff+key_buff_len-2;
    if ((sh->kb_pointer)<end) {
      move=sh->kb_pointer;
      while(*move && (move<end)) move++;
      for(;move!=sh->kb_pointer;move--) *move = *(move-1);
      *(sh->kb_pointer)=c;
      draw_key_buff(sh,sh->kb_pointer,strlen(sh->kb_pointer));
      sh->kb_pointer++;
      sh->curspos++;
      count=strlen(sh->key_buff);
      if (sh->flags&COMMAND) count+=strlen(COMMAND_PROMPT);
      else count+=strlen(sh->prompt);
      if (!(count%cols)) putchar(' ');
      position_cursor(sh);
    }
  }
}

/* what happens if a control character is entered */

void control_char(c)
char c;
{
  void (*fn)();
  shell *sh;
  sh=current_shell;
  if (sh->flags&NO_INPUT) return;
  if (c<0) return;
  fn=control_fns[c];
  (*fn)();
  
}

void handle_user()
{
  char c;
  int chars_ready;
  static int set_escape = 0;
  static int set_cursor = 0;

  if (ioctl(0,TIOCINQ,&chars_ready) == -1) handle_error("Ioctl Failed");
  if (!chars_ready) return;

  for(;chars_ready;chars_ready--) {
    c=fgetc(stdin);
    if (set_cursor) {
      set_cursor = 0;
      if (c >= 'A' && c <= 'D') {
#ifdef GRAPHICS
	if (current_shell->grmode == GRBSX) {
	  switch(c) {
	  case 'A':
	    write(current_shell->sock_desc, "micro_n\n", 8);
	    break;
	  case 'B':
	    write(current_shell->sock_desc, "micro_s\n", 8);
	    break;
	  case 'C':
	    write(current_shell->sock_desc, "micro_e\n", 8);
	    break;
	  case 'D':
	    write(current_shell->sock_desc, "micro_w\n", 8);
	    break;
	  }
	}
	else {
#endif
	  switch(c) {
	  case 'A':
	    prev_history();
	    break;
	  case 'B':
	    next_history();
	    break;
	  case 'C':
	    forward_char();
	    break;
	  case 'D':
	    back_char();
	    break;
	  }
#ifdef GRAPHICS
	}
#endif
	c = -1;
      }
    }
    if (set_escape) {
      set_escape = 0;
      if (c == '[') {
	set_cursor = 1;
	c = -1;
      }
      else
	toggle_command();
    }
    if (c==127) c=8;
    if (c>31) normal_char(c);
    else if (c == 27)
      set_escape = 1;
    else control_char(c);
    current_shell->flags &= ~CTRL_C;
  }
  if (*(current_shell->key_buff)==0)
    current_shell->flags &= ~I_CHANGE;
}


/* control functions */


/* go to start of the line */

void go_start_line()
{
  current_shell->curspos=0;
  current_shell->kb_pointer=current_shell->key_buff;
  position_cursor(current_shell);
}

/* go to end of line */

void go_end_line()
{
  current_shell->curspos=0;
  current_shell->kb_pointer=current_shell->key_buff;
  for(;*(current_shell->kb_pointer);current_shell->kb_pointer++)
    current_shell->curspos++;
  position_cursor(current_shell);
}

/* delete line */

void del_line()
{
  char *clear;
  delete_input(current_shell);
  for(clear=current_shell->kb_pointer;*clear;clear++) *clear=0;
    update(current_shell);
}

/* delete char forward */

void delete()
{
  char *move;
  int a;
  a=strlen(current_shell->key_buff);
  if (current_shell->flags&COMMAND) a+=strlen(COMMAND_PROMPT);
  else a+=strlen(current_shell->prompt);
  a %= cols;
  if (!a) {
    delete_input(current_shell);
    move=current_shell->kb_pointer;
    for(;*move;move++) *move = *(move+1);
    move=current_shell->kb_pointer;
    putchar('\n');
    update(current_shell);
  }
  else {
    move=current_shell->kb_pointer;
    for(;*move;move++) *move = *(move+1);
    move=current_shell->kb_pointer;
    draw_key_buff(current_shell,move,strlen(move));
    putchar(' ');
    position_cursor(current_shell);
  }
}

/* delete char backward */

void backspace()
{
  char *move;
  int a;
  a=strlen(current_shell->key_buff);
  if (current_shell->flags&COMMAND) a+=strlen(COMMAND_PROMPT);
  else a+=strlen(current_shell->prompt);
  a %= cols;
  if ((current_shell->key_buff)!=(current_shell->kb_pointer)) {  
    if (!a) {
      delete_input(current_shell);
      current_shell->kb_pointer--;
      move=current_shell->kb_pointer;
      for(;*move;move++) *move = *(move+1);
      current_shell->curspos--;
      move=current_shell->kb_pointer;
      putchar('\n');
      update(current_shell);
    }
    else {
      current_shell->kb_pointer--;
      move=current_shell->kb_pointer;
      for(;*move;move++) *move = *(move+1);
      current_shell->curspos--;
      move=current_shell->kb_pointer;
      position_cursor(current_shell);
      draw_key_buff(current_shell,move,strlen(move));
      putchar(' ');
      position_cursor(current_shell);
    }     
  }
}

/* leave program */

void break_out()
{
  quit=1;
}

/* bugger all */

void null()
{
}

/* move back without delete */

void back_char()
{
  if ((current_shell->key_buff)!=(current_shell->kb_pointer)) {  
    current_shell->curspos--;
    current_shell->kb_pointer--;
    position_cursor(current_shell);
  }
}

/* move forward without delete */

void forward_char()
{
  if (*(current_shell->kb_pointer)) {
    current_shell->curspos++;
    current_shell->kb_pointer++;
    position_cursor(current_shell);
  }
}

/* what happens when return is pressed */

void newline()
{
  shell *sh;
  sh=current_shell;
  delete_input(sh);
  sh->flags|=NO_UPDATE;
  store_key_history(sh);
  if ((compare("alias",sh->key_buff) ||
       compare("unalias",sh->key_buff)) && sh->flags&COMMAND) 
    command_processing(sh,sh->key_buff);
  else user_processing(sh,0);
  sh=current_shell;
  if (!(sh->flags&NO_CLEAR)) {
    memset(sh->key_buff,0,key_buff_len);
    sh->kb_pointer=sh->key_buff;
    sh->curspos=0;
  }
  if ((sh->flags&CONNECTED) && (sh->flags&COMMAND))
    sh->flags &= ~COMMAND;  
  if (!(sh->flags&CONNECTED))
    sh->flags |= COMMAND; 
  sh->flags &= ~(NO_UPDATE|NO_CLEAR|NO_OUTPUT|NO_RECURSION);
  update(sh);
}

/* toggle between command and normal mode plus cursor key support etc*/
/* cursor key support? _what_ cursor key support! never mind, I'll add it..*/

void toggle_command()
{
  if (current_shell->flags&CONNECTED) {
    delete_input(current_shell);
    current_shell->flags ^= COMMAND;
    update(current_shell);
  }
}

/* tab completion */

void ccomplete()
{
  delete_input(current_shell);
  current_shell->flags |= NO_UPDATE;
  if (current_shell->flags&COMMAND) command_completion(current_shell);
  else do_history(current_shell,current_shell->key_buff);
  current_shell->flags &= ~(NO_UPDATE|NO_CLEAR);
  update(current_shell);
}

/* new shell */

void create_shell()
{
  shell *sh;
  delete_input(current_shell);
  current_shell->flags |= NO_CLEAR;

  new_shell("");

  sh=current_shell;
  if (!(sh->flags&NO_CLEAR)) {
    memset(sh->key_buff,0,key_buff_len);
    sh->kb_pointer=sh->key_buff;
    sh->curspos=0;
  }
  if ((sh->flags&CONNECTED) && (sh->flags&COMMAND))
    sh->flags &= ~COMMAND;  
  sh->flags &= ~(NO_UPDATE|NO_CLEAR);
  update(sh);
}

/* kill shell */


void quit_shell()
{
  shell *sh;
  
  if (!(current_shell->flags&CTRL_C)) {
    shell_print(current_shell,"Press ^c then ^x to kill shell.");
    return;
  }

  if ((current_shell==first_shell) && (!(current_shell->next))) {
    shell_print(current_shell,"Use 'quit' to kill the last shell.");
    return;
  }
  delete_input(current_shell);
  current_shell->flags |= NO_CLEAR;
  close_shell(current_shell);
  
  sh=current_shell;
  if (!(sh->flags&NO_CLEAR)) {
    memset(sh->key_buff,0,key_buff_len);
    sh->kb_pointer=sh->key_buff;
    sh->curspos=0;
  }
  if ((sh->flags&CONNECTED) && (sh->flags&COMMAND))
    sh->flags &= ~COMMAND; 
  sh->flags &= ~(NO_UPDATE|NO_CLEAR);
  update(sh);
}

/* next shell */

void go_next_shell()
{
  shell *sh;
  delete_input(current_shell);
  current_shell->flags |= NO_CLEAR;  

  if (current_shell->next) switch_shell(current_shell->next);
  else switch_shell(first_shell);
  redraw_screen();

  sh=current_shell;
  if (!(sh->flags&NO_CLEAR)) {
    memset(sh->key_buff,0,key_buff_len);
    sh->kb_pointer=sh->key_buff;
    sh->curspos=0;
  }
  if ((sh->flags&CONNECTED) && (sh->flags&COMMAND))
    sh->flags &= ~COMMAND; 
  sh->flags &= ~(NO_UPDATE|NO_CLEAR);
  update(sh);
}

/* previous shell */

void go_previous_shell()
{
  shell *sh;
  shell *scan;
  delete_input(current_shell);
  current_shell->flags |= NO_CLEAR;

  scan=first_shell;
  if (current_shell==first_shell) while(scan->next) scan=scan->next;
  else while(scan->next!=current_shell) scan=scan->next;
  switch_shell(scan);
  redraw_screen();

  sh=current_shell;
  if (!(sh->flags&NO_CLEAR)) {
    memset(sh->key_buff,0,key_buff_len);
    sh->kb_pointer=sh->key_buff;
    sh->curspos=0;
  }
  if ((sh->flags&CONNECTED) && (sh->flags&COMMAND))
    sh->flags &= ~COMMAND; 
  sh->flags &= ~(NO_UPDATE|NO_CLEAR);
  update(sh);
}


void transpose()
{
  char *pos,tmp;
  pos=current_shell->kb_pointer;
  if (pos==current_shell->key_buff) return;
  if ((!*pos) || (!*(pos-1))) return;
  tmp = *pos;
  *pos = *(pos-1);
  *(pos-1)=tmp;
  current_shell->curspos--;
  position_cursor(current_shell);
  draw_key_buff(current_shell,pos-1,2);
  current_shell->curspos+=2;
  current_shell->kb_pointer++;
}

void cls()
{
  tputs(tcaps[TERM_CLS],1,tput_write_fn);
}

void clear_screen(sh,str)
shell *sh;
char *str;
{
  if (sh==current_shell) {
    tputs(tcaps[TERM_CLS],1,tput_write_fn);
    delete_input(current_shell);
  }
}

void hilight(sh,str)
shell *sh;
char *str;
{
  if (sh!=current_shell) return;
  tputs(tcaps[TERM_BOLD],1,tput_write_fn);
  print(sh,str);
  tputs(tcaps[TERM_OFF],1,tput_write_fn);
  print(sh,"\n");
}


/* print something to the current shell */

void hilight_in_current(sh,str)
shell *sh;
char *str;
{
  unsigned int flags;
  if (sh!=current_shell && !(current_shell->flags&NO_UPDATE) && 
      !(current_shell->flags&HALF_LINE)) delete_input(current_shell);
  flags=current_shell->flags&NO_UPDATE;
  current_shell->flags |= NO_UPDATE;
  tputs(tcaps[TERM_BOLD],1,tput_write_fn);
  print(current_shell,str);
  tputs(tcaps[TERM_OFF],1,tput_write_fn);
  print(current_shell,"\n");
  current_shell->flags &= ~NO_UPDATE;
  current_shell->flags |= flags;
  if (sh!=current_shell && !(current_shell->flags&NO_UPDATE) && 
      !(current_shell->flags&HALF_LINE)) draw_input(current_shell);
}

/* print something to the current shell */

void print_in_current(sh,str)
shell *sh;
char *str;
{
  unsigned int flags;
  if (sh!=current_shell && !(current_shell->flags&NO_UPDATE) && 
      !(current_shell->flags&HALF_LINE)) delete_input(current_shell);
  flags=current_shell->flags&NO_UPDATE;
  current_shell->flags |= NO_UPDATE;
  print(current_shell,str);
  print(current_shell,"\n");
  current_shell->flags &= ~NO_UPDATE;
  current_shell->flags |= flags;
  if (sh!=current_shell && !(current_shell->flags&NO_UPDATE) && 
      !(current_shell->flags&HALF_LINE)) draw_input(current_shell);
}

/* toggle secrecy */

void toggle_secrecy()
{
  delete_input(current_shell);
  current_shell->flags ^= SECRET;
  update(current_shell);
}


/* this is what happens if the screen gets resized or corrupted */

void window_change()
{
  signal(SIGWINCH,window_change);
  wchange=1;
}

void restart()
{  
#ifdef SYSV
  struct termio terminal;
#else
  struct sgttyb terminal;
#endif
  struct winsize new_size;

  signal(SIGCONT,restart);
#ifdef SYSV
  if (ioctl(0,TCGETA,&terminal)) handle_error("terminal ioctl\n");
  terminal.c_lflag &= ~(ICANON|ECHO);
  if (ioctl(0,TCSETA,&terminal)) handle_error("terminal ioctl\n");
#else
  if (ioctl(0,TIOCGETP,&terminal)) handle_error("terminal ioctl\n");
  terminal.sg_flags |= CBREAK; 
  terminal.sg_flags &= ~ECHO;
  if (ioctl(0,TIOCSETP,&terminal)) handle_error("terminal ioctl\n");
#endif

  if (ioctl(0,TIOCGWINSZ,&new_size)) handle_error("terminal ioctl\n");
  lines=new_size.ws_row;
  cols=new_size.ws_col;
  
  redraw_screen();
  update(current_shell);
  wchange=0;
}


/* change word wrap size */

void set_wrap_size(sh,str)
shell *sh;
char *str;
{
  int new=0;
  if (!*str) str="10";
  new=atoi(str);
  if (new<=0) {
    shell_print(sh,"syntax - set wrap <number>");
    return;
  }
  sh->max_word=new;
  sprintf(scratch,"%s Maximum word length is now %d.\n",
          SHELL_PRINT,sh->max_word);
  print(sh,scratch);
}

/* change wrap status */

void change_wrap(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= WORD_WRAP;
  if (!strcmp(str,"on")) sh->flags |= WORD_WRAP;
  if (!strcmp(str,"off")) sh->flags &= ~WORD_WRAP;
  if (sh->flags&WORD_WRAP) shell_print(sh,"Word wrap ON");
  else shell_print(sh,"Word wrap OFF");
}


/* set title bar on an xterm display */

void set_title(sh,str)
shell *sh;
char *str;
{
  if (strcmp("xterm",term_name)) 
    shell_print(sh,"This command works only with xterms.");
  else {
    sprintf(scratch,"\033]2;%s\007",str);
    print(sh,scratch);
  }
}
