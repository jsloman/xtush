/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User's Shell          Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/


/* 
        This is the file command.c

        Here are most of the TUsh commands
*/

#include <ctype.h>
#include <malloc.h>

#ifdef GRAPHICS
#include <X11/Xlib.h>
#endif

#include "config.h"
#include "clist.h"

extern void shell_print(),print(),redraw_screen(),send_out(),user_processing();
extern int quit,key_history_len,key_buff_len,screen_history_len,
        screen_buff_len,scratch_size,no_of_shells,bailout;
extern void *scratch;
extern shell *first_shell,*current_shell;
extern file load_file();
extern char *convert_time();
extern char time_string[];
extern file site_list;

file helpfile={ 0,0 };

/* this is the compare function for command processing */

char *compare(str1,str2)
char *str1,*str2;
{
  while(*str1)  
    if (*str1++ != *str2++) return 0;
  return str2;
}

/* this searches for a site in the site list */

int search_site(sh,str)
shell *sh;
char *str;
{
  int i=0;
  char *scan,*rol,*cpy;

  scan=site_list.where;
  if (!scan) return 0;

  for(cpy=str;*cpy;cpy++) *cpy=tolower(*cpy);
  
  while(i<site_list.length) {
    for(;(!*scan) || (*scan=='\n');scan++) i++;
    if (*scan=='#') 
      for (;*scan!='\n';scan++) i++; 
    else {
      strcpy(scratch,scan);
      for(cpy=scratch;*cpy;cpy++) *cpy=tolower(*cpy);
      rol=compare(scratch,str);
      if (rol && (!*rol || isspace(*rol))) {

	sprintf(scratch,"%s Opening connection to site %s\n",
		SHELL_PRINT,scan);
	print(sh,scratch);

	cpy=sh->key_buff;
	for (;*scan;scan++) i++;
	for (;!*scan;scan++) i++;
	strcpy(cpy,scan);
	while(*cpy) cpy++;
	*cpy++=' ';
	for (;*scan;scan++) i++;
	for (;!*scan;scan++) i++;
	strcpy(cpy,scan);
	open_com(sh,sh->key_buff);

	if (sh->flags&CONNECTED) {
	  for (;*scan;scan++) i++;
	  for (;!*scan;scan++) i++;
	  for(cpy=sh->key_buff;((*scan) && (*scan!='\n'));cpy++,scan++) *cpy = *scan;
	  *cpy++=0;
	  execute(sh,sh->key_buff);
	}
	return 1;
      }
      else
	for (;*scan!='\n';scan++) i++;
    }
  }
  return 0;
}


/* and here is the actual parser */

void command_processing(sh,str)
shell *sh;
char *str;
{
  struct com *com_search;
  char *rol,*sol;
  void (*fn)();
  
  if (!*str) return;
  com_search=coms;
  sol=str;
  while(*sol==' ') sol++;
  while(com_search->text) {  
    rol=compare(com_search->text,sol);
    if (rol) {
      while(*rol==' ') rol++;
      fn=com_search->function;
      (*fn)(sh,rol);
      return;
    }
    com_search++;
  }
  if (sh->flags&CONNECTED) shell_print(sh,"Unknown Command");
  else if (!search_site(sh,str)) shell_print(sh,"Unknown Command");
}

/* tab completion for commands */

void command_completion(sh)
shell *sh;
{
  struct com *search;
  char *sol,*cheat;
  int one_found=0;

  search=coms;
  sol=sh->key_buff;
  while(*sol==' ') sol++;
  
  while(search->text) {  
    if (compare(sol,search->text)) {
      if (one_found) {
        if (one_found==1) {
          for(cheat=scratch;*cheat;cheat++);
          cheat++;
          sprintf(cheat,"%s Multiple matches.\n%s %s\n",
                  SHELL_PRINT,SHELL_PRINT,scratch);
          print(sh,cheat);
        }
        shell_print(sh,search->text);
        one_found++;
      }
      else {
        strcpy(scratch,search->text);
        one_found=1;
      }
    }
    search++;
  }
  if (one_found==1) {
    strncpy(sh->key_buff,scratch,key_buff_len);
    sh->curspos=strlen(sh->key_buff);
    sh->kb_pointer=sh->key_buff+sh->curspos;
  }
  if (!one_found) shell_print(sh,"No Match.");
}


/* these are the actual commands */

/* quit the program */

void quit_out(sh,str)
shell *sh;
char *str;
{
  quit=1;
}

/* refresh display or draw history */

void refresh_shell(sh,str)
shell *sh;
char *str;
{
  int from=0,to=0,p,pointer,count;
  char c,*cpy,*scan;
  sh->flags |= (HISTORY_OFF|NO_UPDATE);
  from=atoi(str);
  if (from<=0) redraw_screen();
  else {
    scan=str;
    while(*scan && *scan!=' ') scan++;
    while(*scan==' ') scan++;
    to=atoi(scan);
    if (!to) { 
      to=from;
      from=1;
    }
    if (from>to) {
      p=from;
      from=to;
      to=p;
    }
    pointer=sh->sh_pointer;
    p=(pointer-1) % screen_history_len;
    if (p<0) p+=screen_history_len;
    count=0;
    while((p!=pointer) && (count<=to)) {    
      if (*(sh->screen_history+p)=='\n') count++;
      p=(p-1) % screen_history_len;
      if (p<0) p+=screen_history_len;
    }
    p=(p+2) % screen_history_len;
    cpy=(char *)scratch;
    while(count>from) {
      c = *(sh->screen_history+p);
      p=(p+1) % screen_history_len;
      if (c) {
        *cpy++=c;
        if (c=='\n') {
          *cpy=0;
	  count--;
          print(sh,scratch);
          cpy=(char *)scratch;
        }
      }
    }
  }
  sh->flags &= ~(HISTORY_OFF|NO_UPDATE);
  delete_input(sh);
}


/* new shell */

void start_new_shell(sh,str)
shell *sh;
char *str;
{
  new_shell(str);
}

/* kill shell */

void kill_shell(sh,str)
shell *sh;
char *str;
{
  if ((sh==first_shell) && (!(sh->next))) quit=1;
  else close_shell(sh);
}

/* next shell */

void next_shell(sh,str)
shell *sh;
char *str;
{
  if (sh->next) switch_shell(sh->next);
  else switch_shell(first_shell);
  redraw_screen();
}

/* previous shell */

void previous_shell(sh,str)
shell *sh;
char *str;
{
  shell *scan;
  scan=first_shell;
  if (sh==first_shell) while(scan->next) scan=scan->next;
  else while(scan->next!=sh) scan=scan->next;
  switch_shell(scan);
  redraw_screen();
}

/* print up the time */

void view_time(sh,str)
shell *sh;
char *str;
{
  shell_print(sh,convert_time());
}

/* set the time string to change format */

void set_time_str(sh,str)
shell *sh;
char *str;
{
  if (!*str) str=STANDARD_TIME_STRING;
  strncpy(time_string,str,80);
  shell_print(sh,convert_time());
}


/* change the prompt */

void change_prompt(sh,str)
shell *sh;
char *str;
{
  strncpy(sh->prompt,str,PROMPT_LEN);
}

/* the send command */

void send_com(sh,str)
shell *sh;
char *str;
{
  send_out(sh,str);
}

/* the echo command */

void echo_com(sh,str)
shell *sh;
char *str;
{
  print(sh,str);
  print(sh,"\n");
}

/* do absolutly nothing */

void null_com(sh,str)
shell *sh;
char *str;
{
}

/* pipe in a file to the server */

void pipe_com(sh,str)
shell *sh;
char *str;
{
  char *start,*end,*scan;
  unsigned int flags;
  file f;

  flags=sh->flags&COMMAND;     
  sh->flags &= ~COMMAND;

  f=load_file(sh,str);
  scan=f.where;
  if (!scan) return;
  end=scan+f.length;
  while(scan<end) {
    start=scan;
    while((scan<end) && (*scan!='\n')) scan++;
    *scan++=0;
    strncpy(sh->key_buff,start,key_buff_len);
    user_processing(sh,0);
  }
  free(f.where);
  sh->flags |= flags;
}


/* pipe in a file to the server but without alias processing */

void no_alias_pipe_com(sh,str)
shell *sh;
char *str;
{
  char *start,*end,*scan;
  file f;

  f=load_file(sh,str);
  scan=f.where;
  if (!scan) return;
  end=scan+f.length;
  while(scan<end) {
    start=scan;
    while((scan<end) && (*scan!='\n')) scan++;
    *scan++=0;
    send_out(sh,start);
  }
  free(f.where);
}

/* pipe in a file to the command parser, same as above really */

void execute(sh,str)
shell *sh;
char *str;
{
  char *start,*end,*scan;
  unsigned int flags;
  file f;

  flags=sh->flags&COMMAND;

  f=load_file(sh,str);
  scan=f.where;
  if (!scan) return;
  end=scan+f.length;
  while(scan<end) {
    start=scan;
    while((scan<end) && (*scan!='\n')) scan++;
    *scan++=0;
    strncpy(sh->key_buff,start,key_buff_len);
    sh->flags |= COMMAND;
    if ((compare("alias",sh->key_buff) ||
	 compare("unalias",sh->key_buff)))
      command_processing(sh,sh->key_buff);
    else user_processing(sh,0);
  }
  free(f.where);
  sh->flags &= ~COMMAND;
  sh->flags |= flags;
}

/* execute a shell command */

void shell_escape(sh,str)
shell *sh;
char *str;
{
  system(str);
}

/* set the sizes of various buffers */

void set_kb(sh,str)
shell *sh;
char *str;
{
  int new_len;
  shell *scan;
  new_len=atoi(str);
  if (new_len<=0) {
    shell_print(sh,"Argument is a number.");
    return;
  }
  if (new_len>key_history_len) {
    shell_print(sh,"Can't set key buffer larger than the key history.");
    return;
  }
  for(scan=first_shell;scan;scan=scan->next) {
    free(scan->key_buff);
    scan->key_buff=(char *)malloc(new_len);
    memset(scan->key_buff,0,new_len);
  }
  key_buff_len=new_len;
}

void set_kh(sh,str)
shell *sh;
char *str;
{
  int new_len;
  shell *scan;
  new_len=atoi(str);
  if (new_len<=0) {
    shell_print(sh,"Argument is a number.");
    return;
  }
  if (new_len<key_buff_len) {
    shell_print(sh,"Can't set key history shorter than the key buffer.");
    return;
  }
  for(scan=first_shell;scan;scan=scan->next) {
    free(scan->key_history);
    scan->key_history=(char *)malloc(new_len);
    memset(scan->key_history,0,new_len);
    scan->kh_pointer=0;
    scan->kh_search=0;
  }
  key_history_len=new_len;
}

void set_sb(sh,str)
shell *sh;
char *str;
{
  int new_len;
  shell *scan;
  new_len=atoi(str);
  if (new_len<=0) {
    shell_print(sh,"Argument is a number.");
    return;
  }
  if (new_len>screen_history_len) {
    shell_print(sh,"Can't set the screen buffer logner than the screen history.");
    return;
  }
  for(scan=first_shell;scan;scan=scan->next) {
    free(scan->screen_buff);
    scan->screen_buff=(char *)malloc(new_len);
    memset(scan->screen_buff,0,new_len);
  }
  screen_buff_len=new_len;
}

void set_sh(sh,str)
shell *sh;
char *str;
{
  int new_len;
  shell *scan;
  new_len=atoi(str);
  if (new_len<=0) {
    shell_print(sh,"Argument is a number.");
    return;
  }
  if (new_len<screen_buff_len) {
    shell_print(sh,"Can't set the screen history shorter than the screen buffer.");
    return;
  }
  for(scan=first_shell;scan;scan=scan->next) {
    free(scan->screen_history);
    scan->screen_history=(char *)malloc(new_len);
    memset(scan->screen_history,0,new_len);
    scan->sh_pointer=0;
  }
  screen_history_len=new_len;
}

/* set the size of the scratch area */

void set_scratch(sh,str)
shell *sh;
char *str;
{
  int new_len=0;
  new_len=atoi(str);
  if (new_len<=0) {
    shell_print(sh,"Argument is a number.");
    return;
  }
  free(scratch);
  scratch=(void *)malloc(new_len);
  scratch_size=new_len;
}


/* monitor a sh for activity */

void monitor_shell(sh,str)
shell *sh;
char *str;
{
  if (sh->flags&MONITOR) {
    shell_print(sh,"Shell will no longer be monitored for activity");
    sh->flags &= ~MONITOR;
  }
  else {
    shell_print(sh,"Shell will be monitored for activity");
    sh->flags |= MONITOR;
    sh->flags &= ~MONITOR_GONEOFF;
  }
}

/* toggle whether a shell responds to bells in another shell */

void set_bell_mode(sh,str)
shell *sh;
char *str;
{
  sh->flags &= ~SEE_BELL;
  if (!*str) sh->flags ^= SEE_BELL;
  if (!strcmp(str,"on")) sh->flags |= SEE_BELL;
  if (!strcmp(str,"off")) sh->flags &= ~SEE_BELL;
  if (sh->flags&SEE_BELL) shell_print(sh,"Bells in other windows will generate a message.");
  else shell_print(sh,"Bells in other windows will be ignored.");
}

/* manually change what happens when prompts get received */

void toggle_prompt_mode(sh,str)
shell *sh;
char *str;
{
  sh->flags &= ~PROMPT_IGNORE;
  if (!*str) sh->flags ^= GA_PROMPT;
  if (!strcmp(str,"on")) sh->flags |= GA_PROMPT;
  if (!strcmp(str,"off")) sh->flags &= ~GA_PROMPT;
  if (!strcmp(str,"ignore")) {
    sh->flags |= PROMPT_IGNORE;
    sh->flags |= GA_PROMPT;
    shell_print(sh,"Prompts recognised, but ignored.");
  }
  if (sh->flags&GA_PROMPT) 
    shell_print(sh,"Prompts recognised, with auto changing set on.");
  else shell_print(sh,"Prompts treated like any other text.");
}


/* toggle whether program does local echoing */

void toggle_echo(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= ECHO_CHARS;
  if (!strcmp(str,"on")) sh->flags |= ECHO_CHARS;
  if (!strcmp(str,"off")) sh->flags &= ~ECHO_CHARS;
  if (sh->flags&ECHO_CHARS) shell_print(sh,"Local echo on");
  else shell_print(sh,"Local echo off");
}


/* toggle whether shell dies or not upon connection close */

void toggle_output(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= NO_SCREEN_OUTPUT;
  if (!strcmp(str,"off")) sh->flags |= NO_SCREEN_OUTPUT;
  if (!strcmp(str,"on")) sh->flags &= ~NO_SCREEN_OUTPUT;
  if (sh->flags&NO_SCREEN_OUTPUT) shell_print(sh,"No output sent direct to screen");
  else shell_print(sh,"Output sent direct to screen");
}

/* toggle whether shell dies or not upon connection close */

void toggle_autodie(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= AUTODIE;
  if (!strcmp(str,"on")) sh->flags |= AUTODIE;
  if (!strcmp(str,"off")) sh->flags &= ~AUTODIE;
  if (sh->flags&AUTODIE) shell_print(sh,"Shell will die when connection closes");
  else shell_print(sh,"Shell will stay alive when connection closes");
}


/* toggle whether program does aliases or not */

void toggle_alias(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= NO_ALIASES;
  if (!strcmp(str,"off")) sh->flags |= NO_ALIASES;
  if (!strcmp(str,"on")) sh->flags &= ~NO_ALIASES;
  if (sh->flags&NO_ALIASES) shell_print(sh,"Alias processing off");
  else shell_print(sh,"Alias processing on");
}


/* toggle whether program does aliases or not */

void toggle_trigger(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= NO_TRIGGERS;
  if (!strcmp(str,"off")) sh->flags |= NO_TRIGGERS;
  if (!strcmp(str,"on")) sh->flags &= ~NO_TRIGGERS;
  if (sh->flags&NO_TRIGGERS) shell_print(sh,"Trigger processing off");
  else shell_print(sh,"Trigger processing on");

}

/* toggle whether the program prints blank lines on a newline */

void toggle_blanks(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= DO_BLANK;
  if (!strcmp(str,"on")) sh->flags |= DO_BLANK;
  if (!strcmp(str,"off")) sh->flags &= ~DO_BLANK;
  if (sh->flags&DO_BLANK) shell_print(sh,"Blank lines on newline.");
  else shell_print(sh,"No blank lines on newline.");
}

/* change the diagnostic status */

void set_diag(sh,str)
shell *sh;
char *str;
{
  if (!*str) sh->flags ^= DIAGNOSTICS;
  if (!strcmp(str,"on")) sh->flags |= DIAGNOSTICS;
  if (!strcmp(str,"off")) sh->flags &= ~DIAGNOSTICS;
  if (sh->flags&DIAGNOSTICS) shell_print(sh,"Telnet option viewing on");
  else shell_print(sh,"Telnet option viewing off");
}

/* show some stats */

void status(sh,str)
shell *sh;
char *str;
{
  char *cpy;
  sprintf(scratch,"%s -- Tush Status --\n%s scratch size = %d\n%s kb size = %d\n%s kh size = %d\n%s sb size = %d\n%s sh size = %d\n%s time format = %s \n%s             = %s\n%s %d shells opened.\n%s alias bailout = %d\n",
          SHELL_PRINT,SHELL_PRINT,scratch_size,SHELL_PRINT,key_buff_len,
          SHELL_PRINT,key_history_len,SHELL_PRINT,screen_buff_len,
          SHELL_PRINT,screen_history_len,SHELL_PRINT,time_string,
          SHELL_PRINT,convert_time(),SHELL_PRINT,no_of_shells,SHELL_PRINT,
          bailout);
  print(sh,scratch);
  if (sh->script_stream) shell_print(sh,"Current shell is scripting.");
  if (sh->flags&WORD_WRAP) {
    sprintf(scratch,"%s Word wrap is ON, and maximum word length is %d\n",
	    SHELL_PRINT,sh->max_word);
    print(sh,scratch);
  }
  else shell_print(sh,"Word Wrap is OFF");
}

#ifndef NO_FANCY_MALLOC

/* show malloc status */

void check_malloc(sh,str)
shell *sh;
char *str;
{
  char *cpy;
  struct mallinfo i;

  i=mallinfo();

  sprintf(scratch,"%s Total arena space\t%d\n\
%s Ordinary blocks\t\t%d\n\
%s Small blocks\t\t%d\n\
%s Holding blocks\t\t%d\n\
%s Space in headers\t\t%d\n\
%s Small block use\t\t%d\n\
%s Small blocks free\t%d\n\
%s Ordinary block use\t%d\n\
%s Ordinary block free\t%d\n\
%s Keep cost\t\t%d\n\
%s Small block size\t\t%d\n\
%s Small blocks in holding\t%d\n\
%s Rounding factor\t\t%d\n\
%s Ordinary block space\t%d\n\
%s Ordinary blocks alloc\t%d\n\
%s Tree overhead\t%d\n",
	  SHELL_PRINT,i.arena,SHELL_PRINT,i.ordblks,SHELL_PRINT,i.smblks,
	  SHELL_PRINT,i.hblks,SHELL_PRINT,i.hblkhd,SHELL_PRINT,i.usmblks,
	  SHELL_PRINT,i.fsmblks,SHELL_PRINT,i.uordblks,
	  SHELL_PRINT,i.fordblks,SHELL_PRINT,i.keepcost,
	  SHELL_PRINT,i.mxfast,SHELL_PRINT,i.nlblks,
	  SHELL_PRINT,i.grain,SHELL_PRINT,i.uordbytes,
	  SHELL_PRINT,i.allocated,SHELL_PRINT,i.treeoverhead);
  print(sh,scratch);
}

#endif

/* this is the help command */

void help(sh,str)
shell *sh;
char *str;
{
  char *scan,*start,*cpy;
  int count;


  for(cpy=str;*cpy;cpy++) *cpy=tolower(*cpy);

  if (!*str) str="help";

  if (!helpfile.where) {
    helpfile=load_file(sh,HELPPATH);
    if (!helpfile.where) return;
    count=helpfile.length;
    scan=helpfile.where;
    while(count>0) {
      if (isalpha(*scan)) {
	start=scan;
	for(cpy="TUsh command list.";*cpy;cpy++,start++)
	  if (*cpy!=*start) break;
	if (*cpy) {
	  while(*scan!='\n') {
	    scan++;
	    count--;
	  }
	  scan++;
	  count--;
	}
	else {
	  helpfile.length=count;
	  helpfile.where=scan;
	  break;
	}
      }
      else {
	while(*scan!='\n') {
	  scan++;
	  count--;
	}
	scan++;
	count--;
      }
    }
    if (count<=0) {
      shell_print(sh,"Error Parsing help file.");
      helpfile.where=0;
      helpfile.length=0;
      return;
    }
  }
  scan=helpfile.where;
  count=helpfile.length;
  while(count>0) {
    if (isalpha(*scan)) {
      start=scan;
      for(cpy=str;*cpy;cpy++,start++) 
	if (*cpy!=tolower(*start)) break;
      if (*cpy) {
	while(*scan!='\n') {
	  scan++;
	  count--;
	}
	scan++;
	count--;
      }
      else {
	shell_print(sh,"----- Help Info -----\n");
	cpy=scratch;
	while(*scan!='\n') {
	  *cpy++=*scan++;
	  count--;
	}
	*cpy++=*scan++;
	count--;
	while(!isalpha(*scan) && (count>0)) {
	  while(*scan!='\n') {
	    *cpy++=*scan++;
	    count--;
	  }
	  *cpy++=*scan++;
	  count--;
	} 
	*cpy=0;
	print(sh,scratch);
	shell_print(sh,"---------------------");
	return;
      }
    }
    else {
      while(*scan!='\n') {
	scan++;
	count--;
      }
      scan++;
      count--;
    }
  }
  sprintf(scratch,"%s Can't find subject '%s' in help file.\n",SHELL_PRINT,str);
  print(sh,scratch);
}


