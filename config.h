/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User's Shell          Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/

/* 
   This is the file config.h

   This file contains general definitions that decide how the 
   program will run.
*/

/* what the standard prompt looks like */

#define STANDARD_PROMPT ":"

/* the file that is executed when TUsh starts up */

#define STARTUP_FILE ".tushrc"

/* the file where the site list is kept */

#define SITE_FILE ".tush-sites"

/* standard file for scripts */

#define SCRIPT_FILE "tush-script"

/* format for the time */

#define STANDARD_TIME_STRING "%I.%M:%S %p - %a, %d %B"



/* 
   Some standard sizes (most of these can be altered while running 
   the program anyway) 
*/


/* This is the size of a general working area */

#define SCRATCH_SIZE 5000

/* the length of the keyboard input buffer */

#define STANDARD_KEY_BUFF_LEN 512

/* the length of the socket/screen buffer */

#define STANDARD_SCREEN_BUFF_LEN 1024

/* length of the keyboard history */

#define STANDARD_KEY_HISTORY_LEN 5000

/* length of the screen/socket history */

#define STANDARD_SCREEN_HISTORY_LEN 10000

/* the maximum no of characters in the prompt */

#define PROMPT_LEN 50

/* the maximum number of times that a line will get passed through the
   alias processor. (to prevent infinite loops) */

#define STANDARD_BAILOUT 20

/* any words longer than this max, will not get wrapped in word wrap mode */

#define MAX_WORD_SIZE 15


/* this controls the small block size for malloc requests
   only change it if you know what it does */

#define MAX_SMALL_BLOCK 200


/* the prompt you get when escaping to do a command */

#define COMMAND_PROMPT "xtush > "

/* Prefix for TUsh messages */

#define SHELL_PRINT "xtush: "

/* length of buffer to store graphics commands */
/* needs to be this big for max bsx scene size*/

#define MAXGRAPHICSLEN 5000 

/* length of buffer to store normal text type stuff */

#define MAXLEN 2000

/* Thing printed when you start a new shell */

#define SHELL_HEADER "xtushv1.5 - TUshv1.74 adapted for graphics\n1995 - Jonathan Sloman\n\n"

/***********************************************************************

   Alter things below this line only if you know what you are doing.

************************************************************************/

#include <stdio.h>

/* the shell structure definition */

struct shs {
  struct shs *next;
  int curspos;
  char *key_history;
  int kh_pointer;
  int kh_search;
  char *screen_history;
  int sh_pointer;
  char *key_buff;
  char *kb_pointer;
  char *screen_buff;
  int sb_pointer;
  char *alias_list;
  char *trigger_list;
  unsigned int flags;
  FILE *script_stream;
  int sock_desc;
  char prompt[PROMPT_LEN];
  int col;
  int max_word;
  int child_pid;
  int child_fd[2];
  int child_mode;
#ifdef GRAPHICS
  Pixmap frame;
  int grmode;
#endif
/* Stuff for telnet state thingies. */
  int lastwasnl;
  int lastwaslf;
  int telnetcontrol;
  char inputline[MAXLEN];  /* completely over the top, but oh well */
  int len;
#ifdef GRAPHICS
  int graphics;
  int bsxgraphics;
  char inputgraphics[MAXGRAPHICSLEN];
  int graphicslen;
#endif
};

typedef struct shs shell;

/* various flags for each shell */

#define CONNECTED 1
#define COMMAND 2
#define NO_INPUT 4
#define NO_UPDATE 8
#define NO_CLEAR 16
#define ECHO_CHARS 32
#define PRINT_INPUT 64
#define LOWER_CASE 128
#define CAPITALISE 256
#define UPPER_CASE 512
#define CAPS_DONE 1024
#define HISTORY_OFF 2048
#define SECRET 4096
#define WORD_WRAP 8192
#define DIAGNOSTICS 16384
#define NO_OUTPUT 32768
#define NO_RECURSION 65536
#define HALF_LINE (1<<17)
#define DO_BLANK (1<<18)
#define I_CHANGE (1<<19)
#define CTRL_C (1<<20)
#define NO_ALIASES (1<<21)
#define NO_TRIGGERS (1<<22)
#define AUTODIE (1<<23)
#define NO_SCREEN_OUTPUT (1<<24)
#define GA_PROMPT (1<<25)
#define PROMPT_IGNORE (1<<26)
#define MONITOR (1<<27)
#define SEE_BELL (1<<28)
#define EOR_ON (1<<29)
#define MONITOR_GONEOFF (1<<30)

#ifdef GRAPHICS
/* possible graphics modes. */
#define GRNONE 0
#define GRBSX 1
#define GRREAD 2
#define GRWRITE 3
#endif

/* file structure definition */

typedef struct {
  void *where;
  int length;
} file;

