/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User Shell            Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/

/*
        This is alias.c

        This is where aliases, history and trigger stuff is
*/

#include <stdlib.h>
#include <ctype.h>

#ifdef GRAPHICS
#include <X11/Xlib.h>
#endif

#include "config.h"

extern void print(),send_out();
extern void *scratch;
extern int key_history_len,key_buff_len,screen_history_len,screen_buff_len,
        scratch_size;
extern shell *current_shell;
extern char *compare(),*convert_time();
int bailout=STANDARD_BAILOUT;
char wildcard='*';



/* change the wildcard to something else */

void set_wildcard(sh,str)
shell *sh;
char *str;
{
  if (!*str) wildcard='*';
  else wildcard=*str;
  sprintf(scratch,"%s Wildcard for aliases and triggers set to '%c'\n",SHELL_PRINT,wildcard);
  print(sh,scratch);
}


/* Takes user input and stores as a history */

void store_key_history(sh)
shell *sh;
{
  char *where,*lim;
  int len,i;
  len=strlen(sh->key_buff)+1;
  if (len==1) return;
  where=sh->key_history+sh->kh_pointer;
  if ((len+sh->kh_pointer)>=key_history_len) {
    for(lim=sh->key_history+key_history_len;where<lim;where++) *where=0;
    where=sh->key_history;
  }
  if (sh->flags&SECRET) 
    for(lim=sh->key_buff;*lim;lim++,where++) *where='*';
  else
    for(lim=sh->key_buff;*lim;lim++,where++) *where = *lim;
  *where++=0;
  sh->kh_pointer=where-sh->key_history;        
  while(*where) *where++=0;
  sh->kh_search=sh->kh_pointer;
}

/* takes stuff for screen and stores as a history */

void add_screen_history(sh,str)
shell *sh;
char *str;
{
  while(*str) {  
    *(sh->screen_history+sh->sh_pointer) = *str++;
    sh->sh_pointer=(sh->sh_pointer+1) % screen_history_len;
  }
}

/* user history commands */

/* displays next string in history */

void next_history()
{
  char *where;  
  shell *sh;
  sh=current_shell;
  if (sh->flags&I_CHANGE) 
    store_key_history(sh);
  delete_input(sh);
  where=sh->key_history+sh->kh_search;
  for(;(*where);sh->kh_search++,where++) 
    if (sh->kh_search>=key_history_len) {
      sh->kh_search=0;
      where=sh->key_history;
      break;
    }
  for(;(!*where);sh->kh_search++,where++)
    if (sh->kh_search>=key_history_len) {
      sh->kh_search=0;
      where=sh->key_history;
      break;
    }
  strncpy(sh->key_buff,where,key_buff_len);
  sh->curspos=strlen(sh->key_buff);
  sh->kb_pointer=sh->key_buff+sh->curspos;
  update(sh);
  sh->flags &= ~I_CHANGE;
}


/* returns a pointer to previous string in the history */
  
char *point_to_previous(sh)
shell *sh;
{
  char *where;
  if (!sh->kh_search) sh->kh_search=key_history_len;
  sh->kh_search--;
  where=sh->key_history+sh->kh_search;
  for(;((!*where) && (sh->kh_search));sh->kh_search--,where--);
  for(;(sh->kh_search);sh->kh_search--,where--)
    if (!*where) { 
      where++;
      sh->kh_search++;
      break;
    }
  return where;
}

/* recalls previous string in history */

void prev_history()
{
  char *where;
  shell *sh;
  sh=current_shell;
  if (sh->flags&I_CHANGE) {
    store_key_history(sh);
    where=point_to_previous(sh);
  }
  delete_input(sh);
  where=point_to_previous(sh);
  strncpy(sh->key_buff,where,key_buff_len);
  sh->curspos=strlen(sh->key_buff);
  sh->kb_pointer=sh->key_buff+sh->curspos;
  update(sh);
  sh->flags &= ~I_CHANGE;
}

/* show entries in the history */

void show_history(sh,str)
shell *sh;
char *str;
{
  int count=2,upto=0;
  char *scan,*first;
  if (*str) upto=atoi(str);
  first=point_to_previous(sh);
  shell_print(sh,"-- Command History --");
  sprintf(scratch,"%s (1) %s\n",SHELL_PRINT,first);
  print(sh,scratch);
  scan=point_to_previous(sh);
  for(;((upto!=1) && (scan!=first));count++,upto--) {
    sprintf(scratch,"%s (%d) %s\n",SHELL_PRINT,count,scan);
    print(sh,scratch);
    scan=point_to_previous(sh);
  }
  sh->kh_search=sh->kh_pointer;
}

/* pick strings from the history */

void do_history(sh,str)
shell *sh;
char *str;
{
  int count=0,upto=0;
  char *scan,*first;
  if (*str) upto=atoi(str);
  first=point_to_previous(sh);
  if (upto>0)
    for(;(count!=upto);count++) {
      scan=point_to_previous(sh);
      if (scan==first) {
        shell_print(sh,"Not enough lines in history.");
        return;
      }
    }
  else {
    scan=first;
    while(!compare(str,scan)) {
      scan=point_to_previous(sh);
      if (scan==first) {
        shell_print(sh,"No match.");
        return;
      }
    }
  }
  strncpy(sh->key_buff,scan,key_buff_len);
  sh->curspos=strlen(sh->key_buff);
  sh->kb_pointer=sh->key_buff+sh->curspos;
  sh->flags |= NO_CLEAR;
}


/* some functions to support alias and trigger handling */

/* sets the size of the alias bailout */

void set_bailout(sh,str)
shell *sh;
char *str;
{
  int b;
  if (!*str) b=STANDARD_BAILOUT;
  else b=atoi(str);
  if (b>0) bailout=b;
  else shell_print(sh, "Argument is a number.");
}

/* This is the routine that does all the hard work for the trigger
   and alias routines. It determines whether a string 'fits' a 
   particular macro */

char *test_string(str1,str2)
char *str1,*str2;
{
  while(*str2) {
    if (*str2==wildcard) return str1;
    else
      switch(*str2)  {
      case ' ':
	if ((*str1==' ') || (!*str1)) while(*str1==' ') str1++;
	else return 0;
	break;
      default:
	if (*str2 != *str1++) return 0;
	break;
      }
    str2++;
  }
  if (*str1) return 0;
  return str1;
}

int match(string,macro)
char *string,*macro;
{
  char *args;
  int argc=0;
  args=(char *)scratch;
  while(*macro) {
    string=test_string(string,macro);
    if (string) {
      if (!*string) return argc;
      while(*macro!=wildcard) macro++;
      macro++;
      while((!test_string(string,macro)) && (*string)) *args++ = *string++;
      *args++=0;
      argc++;
    }
    else return -1;
  }
  if (*string) return -1;
  return argc;
}

/* show a particular macro */

void display_macro(sh,macro)
shell *sh;
char *macro;
{
  char *cpy;
  macro+=sizeof(char **);

  sprintf(scratch,"%s \"%s\" -",SHELL_PRINT,macro);
  while(*macro) macro++;
  macro++;
  cpy=(char *)scratch;
  while(*cpy) cpy++;
  sprintf(cpy," %s\n",macro);
  print(sh,scratch);
}

/* this prints up all the triggers/aliases in a list */

void show_macros(sh,list)
shell *sh;
char *list;
{
  while(list) {
    display_macro(sh,list);
    list = *(char **)list;
  }
}

/* sees if a macro is already in a list */

char *find_macro(macro,list)
char *macro,*list;
{
  char *text,*next;
  while(list) {
    text=list+sizeof(char **);
    next = *(char **)list;
    if (!strcmp(text,macro)) return list;
    list=next;
  }
  return 0;
}

/* this converts a macro into the final output string */

/* decides whether something should be in capitals or not */

char do_caps_stuff(sh,c)
char c;
shell *sh;
{
  if (isalnum(c)) {
    if (sh->flags&UPPER_CASE) {
      c=toupper(c);
      sh->flags |= CAPS_DONE;
    }
    if (sh->flags&LOWER_CASE) {
      c=tolower(c);
      sh->flags |= CAPS_DONE;
    }
    if (sh->flags&CAPITALISE) 
      if (sh->flags&CAPS_DONE) c=tolower(c);
      else {
        c=toupper(c);
        sh->flags |= CAPS_DONE;
      }
  }
  else { 
    if (sh->flags&CAPS_DONE) 
      sh->flags &= ~(UPPER_CASE|LOWER_CASE|CAPITALISE|CAPS_DONE);
  }
  return c;
}

char *expand_macro(sh,alias,argc)
shell *sh;
char *alias;
int argc;
{
  int i,count,arg;
  char *start,*where,*scan,c,strip_char=0;
  start=(char *)scratch;
  for (i=0;i<=argc;i++) {
    while(*start) start++;
    start++;
  }
  start+=key_buff_len;           /* groan ! kludgy */
  where=start;
  alias+=sizeof(char **);
  while(*alias) alias++;
  alias++;

  while(*alias) 
    switch(*alias) {
    case '{':
      alias++;
      while(*alias && (*alias!='}')) *where++ = *alias++;
      if (*alias) alias++;
      break;
    case '%':
      alias++;
      count=atoi(alias);
      if (count>0) {
        while(isdigit(*alias)) alias++;
        if (count<=argc) {
          for(i=1,scan=(char *)scratch;i<count;i++) {
            while(*scan) scan++;
            scan++;
          }
          while(*scan) {
            c = *scan++;
            if (c!=strip_char) *where++=do_caps_stuff(sh,c);
	}
      }
    }
      else switch(*alias++) {
      case 'e':
	  sh->flags |= NO_RECURSION;
	  break;
      case 'n':
	  *where++='\n';
	  break;
      case 'p':
	  sh->flags &= ~PRINT_INPUT;
	  break;
      case 't':
	  strcpy(where,convert_time());
	  while(*where) where++;
	  break;
      case 'b':
	  *where++='\007';
	  break;
      case 'x':
	  arg=atoi(alias);
	  if (arg) {
	      while(isdigit(*alias)) alias++;
	      *where++=arg&255;
	  }
	  break;
      case 'c':
	  sh->flags |= COMMAND;
	  break;
      case 'u':
	  sh->flags |= UPPER_CASE;
	  break;
      case 'l':
	  sh->flags |= LOWER_CASE;
	  break;
      case 'v':
	  sh->flags |= CAPITALISE;
	  break;
      case 's':
	  if (strip_char) strip_char=0;
	  else {
	      strip_char = *alias++;
	      if (!strip_char) alias--;
	  }
	  break;
      case 'o':
	  sh->flags |= NO_OUTPUT;
	  break;
      default:
	  alias--;
	  *where++='%';
	  *where++ = *alias++;
	  break;
      }
      break;
  default:           
      c = *alias++;
      if (c!=strip_char) *where++=do_caps_stuff(sh,c);
      break;
  }
  *where=0;
  sh->flags &= ~(UPPER_CASE|CAPITALISE|LOWER_CASE|CAPS_DONE);
  return start;
}

/* these are alias functions for keyboard input */

/* this extracts the alias from the input string */

char *extract_alias(str)
char *str;
{
  char *cpy;
  cpy=(char *)scratch+sizeof(char **);
  while(isspace(*str)) str++;
  if (!*str) return 0;
  if (*str=='\"') {  /* " */
    str++;
    while((*str!='\"') && (*str)) *cpy++ = *str++;  /* " */
    *cpy=0;
    if (*str) {
      str++;
      while(isspace(*str)) str++;
    }
  }  
  else {
    while((*str!=' ') && (*str)) *cpy++ = *str++;
    *cpy++=' ';
    *cpy++=wildcard;
    *cpy++=0;
    while(isspace(*str)) str++;
  }
  return str;
}

/* removes an alias from the list */

void unalias(sh,str)
shell *sh;
char *str;
{
  char *already,*previous,*next,*scan;
  extract_alias(str);
  already=find_macro((char *)scratch+sizeof(char **),sh->alias_list);
  if (!already) shell_print(sh,"No such alias.");
  else {
    next = *(char **)already;
    scan=sh->alias_list;
    if (scan==already) sh->alias_list=next;
    else {
      do {
        previous=scan;
        scan = *(char **)previous;
      } while(scan!=already);
      *(char **)previous = next;
    }
    free(already);
  }
}

/* adds an alias to the list */

void alias_com(sh,str)
shell *sh;
char *str;
{
  char *rol,*already,*cpy;
  int len;
  if (!*str) {
    shell_print(sh,"-- Aliases --");
    show_macros(sh,sh->alias_list);
    return;
  }
  rol=extract_alias(str);
  if (!rol) {
    shell_print(sh,"-- Aliases --");
    show_macros(sh,sh->alias_list);
    return;
  }
  already=find_macro((char *)scratch+sizeof(char **),sh->alias_list);
  if (already) 
    if (!*rol) {
      display_macro(sh,already);
      return;
    }
    else unalias(sh,str);
  if ((!already) && (!*rol)) return;
  cpy=(char *)scratch+sizeof(char **);
  len=strlen(cpy)+1;
  cpy+=len;
  strcpy(cpy,rol);
  len+=strlen(cpy)+sizeof(char **)+1;
  *((char **)scratch)=sh->alias_list;
  sh->alias_list=(char *)malloc(len);
  memcpy(sh->alias_list,scratch,len);
}

/* 
   these are the trigger functions for server input 
   basically they are derived from the alias routines above
*/

/* this extracts the trigger from the input string */

char *extract_trigger(str)
char *str;
{
  char *cpy;
  cpy=(char *)scratch+sizeof(char **);
  while(isspace(*str)) str++;
  if (!*str) return 0;
  if (*str=='\"') { /* " */
    str++;
    while((*str!='\"') && (*str)) *cpy++ = *str++; /* " */
    *cpy=0;
    if (*str) {
      str++;
      while(isspace(*str)) str++;
    }
  }  
  else {
    *cpy++=wildcard;
    while((*str!=' ') && (*str)) *cpy++ = *str++;
    *cpy++=wildcard;
    *cpy++=0;
    while(isspace(*str)) str++;
  }
  return str;
}

/* removes a trigger from the list */

void untrigger(sh,str)
shell *sh;
char *str;
{
  char *already,*previous,*next,*scan;
  extract_trigger(str);
  already=find_macro((char *)scratch+sizeof(char **),sh->trigger_list);
  if (!already) shell_print(sh,"No such trigger.");
  else {
    next = *(char **)already;
    scan=sh->trigger_list;
    if (scan==already) sh->trigger_list=next;
    else {
      do {
        previous=scan;
        scan = *(char **)previous;
      } while(scan!=already);
      *(char **)previous = next;
    }
    free(already);
  }
}

/* adds a trigger to the list */

void trigger_com(sh,str)
shell *sh;
char *str;
{
  char *rol,*already,*cpy;
  int len;
  if (!*str) {
    shell_print(sh,"-- Triggers --");
    show_macros(sh,sh->trigger_list);
    return;
  }
  rol=extract_trigger(str);
  if (!rol) {
    shell_print(sh,"-- Triggers --");
    show_macros(sh,sh->trigger_list);
    return;
  }
  already=find_macro((char *)scratch+sizeof(char **),sh->trigger_list);
  if (already) 
    if (!*rol) {
      display_macro(sh,already);
      return;
    }
    else untrigger(sh,str);
  if ((!already) && (!*rol)) return;
  cpy=(char *)scratch+sizeof(char **);
  len=strlen(cpy)+1;
  cpy+=len;
  strcpy(cpy,rol);
  len+=strlen(cpy)+sizeof(char **)+1;
  *((char **)scratch)=sh->trigger_list;
  sh->trigger_list=(char *)malloc(len);
  memcpy(sh->trigger_list,scratch,len);
}


/* all lines (apart from when using the 'send' command) that get sent 
   to the server will go through this routine 
   Its incredibly horribly recursive */


void user_processing(sh,count)
shell *sh;
int count;
{
  char *alias,*where,*tmp,*cpy;
  char *buff_cpy;
  int argc=-1;
  unsigned int flags;

  buff_cpy=malloc(key_buff_len);

  flags=sh->flags&(COMMAND|NO_OUTPUT);
  alias=sh->alias_list;

  strncpy(buff_cpy,sh->key_buff,key_buff_len);
  tmp=buff_cpy;
  while(count<bailout && *tmp)
    if (*tmp=='\n') {
      *tmp=0;
      strncpy(sh->key_buff,buff_cpy,key_buff_len);
      user_processing(sh,count+1);
      tmp++;
      strncpy(sh->key_buff,tmp,key_buff_len);
      user_processing(sh,count+1);
      sh->flags &= ~(COMMAND|NO_OUTPUT);
      sh->flags |= flags;
      free(buff_cpy);
      return;
    }
    else tmp++;
  
  if (!(sh->flags&NO_ALIASES) && !(sh->flags&NO_RECURSION) && 
      (count<bailout)) {
    while((argc<0) && alias) {
      argc=match(sh->key_buff,alias+sizeof(char **));
      if (argc>=0) {
	where=expand_macro(sh,alias,argc);
	strncpy(sh->key_buff,where,key_buff_len);
	user_processing(sh,count+1);
	sh->flags &= ~(COMMAND|NO_OUTPUT);
	sh->flags |= flags;
	free(buff_cpy);
	return;
      }
      else alias = *(char **)alias;
    }
  }
  if (sh->flags&COMMAND) {
    command_processing(sh,sh->key_buff);
    /* quick fix for if sh was deleted by this command. Wibble */
    if (sh != current_shell) {
      free(buff_cpy);
      return;
    }
  }
  else send_out(sh,sh->key_buff);
  sh->flags &= ~(COMMAND|NO_OUTPUT);
  sh->flags |= flags;
  free(buff_cpy);
}

/* all lines recieved from the server will go through these routines */

void do_triggers(sh)
shell *sh;
{
  char *trigger,*where;
  char *key_buff_cpy;
  int argc = -1,count = 0;
  unsigned int flags;

  if (!*sh->screen_buff) return;

  if (sh->flags&NO_TRIGGERS) return;

  key_buff_cpy=malloc(key_buff_len);

  trigger=sh->trigger_list;
  while(trigger) {
    argc=match(sh->screen_buff,trigger+sizeof(char **));
    if (argc>=0) {
      if (!count) {
	strncpy(key_buff_cpy,sh->key_buff,key_buff_len);
	flags=sh->flags&(COMMAND|NO_UPDATE);
      }
      sh->flags &= ~COMMAND;
      sh->flags |= NO_UPDATE;
      where=expand_macro(sh,trigger,argc);
      strncpy(sh->key_buff,where,key_buff_len);
      user_processing(sh,0);
      count++;
    }
    trigger = *(char **)trigger;
  }
  if (count) {
    sh->flags &= ~(COMMAND|NO_UPDATE);
    sh->flags |= flags;
    strncpy(sh->key_buff,key_buff_cpy,key_buff_len);
  }
  sh->flags&=~NO_RECURSION;
  free(key_buff_cpy);
}

void input_processing(sh,str,len)
shell *sh;
char *str;
int len;
{
  unsigned int flags;
  sh->flags |= PRINT_INPUT;
  for(;len>0;len--) 
    if (((unsigned char) *str)==255) {
      if (sh->sb_pointer<screen_buff_len) {str++;len--;}
      else len++;
      *(sh->screen_buff+sh->sb_pointer)=0;
      sh->sb_pointer=0;
      do_triggers(sh,sh->screen_buff);
      if (sh->flags&GA_PROMPT) {
	if (!(sh->flags&PROMPT_IGNORE))
	  strncpy(sh->prompt,sh->screen_buff,PROMPT_LEN-1);
      }
      else
	if (sh->flags&PRINT_INPUT) {
	  strcat(sh->screen_buff,"\n");
	  if (sh->flags&HALF_LINE) {
	    sh->flags&=~HALF_LINE;
	    flags=sh->flags;
	    sh->flags|=NO_UPDATE;
	    print(sh,sh->screen_buff);
	    sh->flags=flags;
	    if (!(sh->flags&NO_UPDATE)) draw_input(sh);
	  }
	  else print(sh,sh->screen_buff);
	}
      sh->flags |= PRINT_INPUT;
    }
    else if ((*str!='\n') && (sh->sb_pointer<screen_buff_len)) {
      *(sh->screen_buff+sh->sb_pointer) = *str++;
      sh->sb_pointer++;
    }
    else {
      if (sh->sb_pointer<screen_buff_len) str++;
      else len++;
      *(sh->screen_buff+sh->sb_pointer)=0;
      sh->sb_pointer=0;
      do_triggers(sh,sh->screen_buff);
      if (sh->flags&PRINT_INPUT) {
        strcat(sh->screen_buff,"\n");
	if (sh->flags&HALF_LINE) {
	  sh->flags&=~HALF_LINE;
	  flags=sh->flags;
	  sh->flags|=NO_UPDATE;
	  print(sh,sh->screen_buff);
  	  sh->flags=flags;
	  if (!(sh->flags&NO_UPDATE)) draw_input(sh);
	}
	else print(sh,sh->screen_buff);
      }
      sh->flags |= PRINT_INPUT;
    }
  if (sh->sb_pointer) {
    *(sh->screen_buff+sh->sb_pointer)=0;
    sh->sb_pointer=0;
    do_triggers(sh,sh->screen_buff);
    if (sh->flags&PRINT_INPUT) {
      if (!strlen(sh->key_buff)) {
	sh->flags|=HALF_LINE;
	print(sh,sh->screen_buff);
      }
      else {
	sh->flags&=~HALF_LINE;
	strcat(sh->screen_buff,"\n");
	print(sh,sh->screen_buff);
      }
    }
    sh->flags |= PRINT_INPUT;
  }
  sh->flags &= ~NO_OUTPUT;
}

/* this is the match command */

void match_com(sh,str)
shell *sh;
char *str;
{
  char *where,*rol,*end,*start,*trigger,*cpy;
  char c,*buff_cpy;
  int argc,scan,len,begin=0,finish=0,count=0;

  buff_cpy=malloc(key_buff_len);
  
  sh->flags &= ~COMMAND;

  strcpy(buff_cpy,sh->key_buff);
  
  begin=atoi(str);
  if (begin) {
    while(*str && !isspace(*str)) str++;
    while(isspace(*str)) str++;
    finish=atoi(str);
    if (finish) {
      while(*str && !isspace(*str)) str++;
      while(isspace(*str)) str++;
    }
  }

  if (!finish) finish=10000;  /* ok ok its kludgy I know */

  rol=extract_trigger(str);
  if (!rol) {
    shell_print(sh,"Can't extract trigger part of argument");
    free(buff_cpy);
    return;
  }
  cpy=(char *)scratch+sizeof(char **);
  len=strlen(cpy)+1;
  cpy+=len;
  strcpy(cpy,rol);
  len+=strlen(cpy)+sizeof(char **)+1;
  trigger=(char *)malloc(len);
  memcpy(trigger,scratch,len);

  scan=(sh->sh_pointer-1) % screen_history_len;
  if (scan<0) scan+=screen_history_len;
  end=(char *)scratch+scratch_size;
  start=end;
  *(--start)=0;
  while(scan!=sh->sh_pointer) {
    c = *(sh->screen_history+scan);
    if ((!c) || (c=='\n')) {
      if (*start) {
        argc=match(start,trigger+sizeof(char **));
        if (argc>=0) {
	  count++;
	  if (count>=begin && count<=finish) {
	    where=expand_macro(sh,trigger,argc);
	    strncpy(sh->key_buff,where,key_buff_len);
	    user_processing(sh,0);
	  }
        }
        start=end;
        *(--start)=0;
      }
    }
    else *(--start)=c;
    scan=(scan-1) % screen_history_len;
    if (scan<0) scan+=screen_history_len;
  }
  strncpy(sh->key_buff,buff_cpy,key_buff_len);
  free(buff_cpy);
  free(trigger);
}


void recall(sh,str)
shell *sh;
char *str;
{
  int line=0,scan,i;
  char c,*cpy;
  line=atoi(str);
  if (!line) line=1;
  line++;

  scan=sh->sh_pointer-1;

  while(line && (scan!=sh->sh_pointer)) {
    c=*(sh->screen_history+scan);
    if ((!c) || (c=='\n')) line--;
    scan=(scan-1) % screen_history_len;
    if (scan<0) scan+=screen_history_len;
  }
  if (line) {
    shell_print(sh, "No such line");
    return;
  }
  scan=(scan+2) % screen_history_len;
  c=*(sh->screen_history+scan);
  cpy=sh->key_buff;
  for(i=0;(c && (c!='\n') && (i<key_buff_len));i++) {
    *cpy++=c;
    scan=(scan+1)%screen_history_len;
    c=*(sh->screen_history+scan);
  }
  *cpy=0;
  sh->curspos=strlen(sh->key_buff);
  sh->kb_pointer=cpy;
  sh->flags |= NO_CLEAR;
}

