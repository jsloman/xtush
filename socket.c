/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User Shell            Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/


/* 
        This is the file socket.c
        Contains Telnet protocol I/O
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <arpa/telnet.h>
#ifndef LINUX
#ifndef NO_FILIO
#include <sys/filio.h>
#endif
#endif
#ifdef GRAPHICS
#include <X11/Xlib.h>
#endif

#include "config.h"

extern void print();
extern void *scratch;
extern shell *current_shell, *first_shell;

#ifdef GRAPHICS
extern int graphics_processing();
extern int bsx_processing();
extern void blankpixmap();
extern void redisplay();
#endif

static int innewline = 0;

/* the close command */

void close_com(sh,str)
shell *sh;
char *str;
{
  if (!(sh->flags&CONNECTED)) 
    shell_print(sh,"No connection made.");
  else {
    sh->flags &= ~(CONNECTED|EOR_ON);
    sh->flags |= COMMAND;
    close(sh->sock_desc);
    sh->sock_desc=0;
    shell_print(sh,"Connection Closed.");
#ifdef GRAPHICS
    blankpixmap(&(sh->frame));
    sh->grmode = GRNONE;
    redisplay();
#endif
    if (sh->flags&AUTODIE) kill_shell(sh,str);
  }
}

/* send a line to the connected server */

void send_out(sh,str)
shell *sh;
char *str;
{
  char *cpy;
  int len;
  if (!(sh->flags&CONNECTED)) return;  
  cpy=scratch;
  while(*str)  
    if (*str=='\n') {
      *cpy++='\r'; 
      *cpy++='\n';
      str++;
    }
    else
      *cpy++ = *str++;
  *cpy++='\r'; 
  *cpy++='\n';
  *cpy=0;
  len=strlen(scratch);
  if (!(sh->flags&SECRET) && 
      (sh->flags&ECHO_CHARS) || ((sh->flags&DO_BLANK) && 
				 (*(char *)scratch=='\n') && (len==2)))
    print(sh,scratch);
  if (write(sh->sock_desc,scratch,len) == -1) {
    shell_print(sh, "Error writing to socket ... closing.");
    close_com(sh,0);
  }
}

/* parses the input one character at a time. */
/* (yes this is horrible, but never mind, it used to be worse..) */

void parse_input(sh, c, flag) 
shell *sh;
unsigned char c;
int flag;
{
  char pling[3];
  int tmp;
#ifdef GRAPHICS
  if (sh->graphics > 0) {
    if (flag)
      return;
    switch (sh->graphicslen) {
    case 1:
      if (c == 'g')
	sh->inputgraphics[sh->graphicslen++] = c;
      else
	sh->graphics = 0;
      break;
    case 2:
      if (c == 'r')
	sh->inputgraphics[sh->graphicslen++] = c;
      else
	sh->graphics = 0;
      break;
    default:
      if (c == 0 || c == '\n' || c == '\r') {
	if (c == '\n')
	  sh->lastwasnl = 1;
	if (c == '\r')
	  sh->lastwaslf = 1;
	sh->inputgraphics[sh->graphicslen] = '\0';
	graphics_processing(sh,sh->inputgraphics + 3);
	sh->graphicslen = 0;
	sh->graphics = 0;
	return;
      }
      else
	if (sh->graphicslen >= MAXGRAPHICSLEN-1)
	  print(sh,"graphicslen exceeded.. oops.\n");
	else
	  sh->inputgraphics[sh->graphicslen++] = c;
    }
    if (sh->graphics == 0) {
      strncpy(sh->inputline + sh->len, sh->inputgraphics, sh->graphicslen);
      sh->len += sh->graphicslen;
    }
    else
      return;
  }
  if (sh->bsxgraphics > 0) {
    if (flag)
      return;
    if (c == '\0' || c == '\n' || c == '\r') {
      sh->inputgraphics[sh->graphicslen] = '\0';
      tmp = bsx_processing(sh, sh->inputgraphics+1);
      if (!tmp) {
	strncpy(sh->inputline+sh->len, sh->inputgraphics, sh->graphicslen);
	sh->len += sh->graphicslen;
      }
      else if (tmp < sh->graphicslen-1) {
	strncpy(sh->inputline+sh->len, sh->inputgraphics+1+tmp, sh->graphicslen-tmp-1);
	sh->len += sh->graphicslen-tmp-1;
      }
      if (tmp)
	sh->grmode = GRBSX;
      sh->graphicslen = 0;
      sh->bsxgraphics = 0;
    }
    else if (c == '@') {
      sh->inputgraphics[sh->graphicslen] = '\0';
      tmp = bsx_processing(sh, sh->inputgraphics+1);
      if (!tmp) {
	strncpy(sh->inputline+sh->len, sh->inputgraphics, sh->graphicslen);
	sh->len += sh->graphicslen;
      }
      else if (tmp < sh->graphicslen-1) {
	strncpy(sh->inputline+sh->len, sh->inputgraphics+1+tmp, sh->graphicslen-tmp-1);
	sh->len += sh->graphicslen-tmp-1;
      }
      sh->graphicslen = 1;
      sh->inputgraphics[0] = c;
    }
    else
      if (sh->graphicslen >= MAXGRAPHICSLEN-1)
	print(sh,"graphicslen exceeded.. oops.\n");
      else {
	sh->inputgraphics[sh->graphicslen++] = c;
	if (sh->graphicslen == 4)
	  if (!strncmp(sh->inputgraphics,"@RFS",4)) {
	    sh->inputgraphics[sh->graphicslen++] = '\0';
	    bsx_processing(sh, sh->inputgraphics+1);
	    sh->graphicslen = 0;
	  }
      }
    return;
  }
#endif
  if (sh->telnetcontrol > 0) {
    if (flag)
      return;
    switch(sh->telnetcontrol) {
    case 2: /* WILL */
      switch(c) {
      case TELOPT_ECHO:
	if (sh->flags&SECRET) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WILL Echo\n");
	    print(sh,"RESPONSE - IGNORE, already in secret mode\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WILL Echo\n");
	    print(sh,"RESPONSE - Enter secrecy mode\n");
	  }
	  write(sh->sock_desc,"\377\375\001",3);
	  sh->flags |= SECRET;
	}
	break;
      case TELOPT_SGA:
	if (sh->flags&DIAGNOSTICS) {
	  print(sh,"RECEIVED - WILL Single char mode\n");
	  print(sh,"RESPONSE - Send DONT single char mode\n");
	}
	write(sh->sock_desc,"\377\376\003",3);
	break;
      case TELOPT_EOR:
	if (sh->flags&EOR_ON) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WILL EOR\n");
	    print(sh,"RESPONSE - IGNORE, already in EOR mode\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WILL EOR\n");
	    print(sh,"RESPONSE - Send DO EOR\n");
	  }
	  write(sh->sock_desc,"\377\375\031",3);
	  sh->flags |= EOR_ON;
	}
	break;
      }
      break;
    case 3:  /* WONT */
      switch(c) {
      case TELOPT_ECHO:
	if (!(sh->flags&SECRET)) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WONT Echo\n");
	    print(sh,"RESPONSE - IGNORE, already doing echo\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WONT Echo\n");
	    print(sh,"RESPONSE - Stop secrecy mode\n");
	  }
	  write(sh->sock_desc,"\377\376\001",3);
	  sh->flags &= ~SECRET;
	}
	break;
      case TELOPT_SGA:
	if (sh->flags&DIAGNOSTICS) {
	  print(sh,"RECEIVED - WONT Single char mode\n");
	  print(sh,"RESPONSE - Ignore (TUsh is already in this mode)\n");
	}
	break;
      case TELOPT_EOR:
	if (!(sh->flags&EOR_ON)) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WONT EOR\n");
	    print(sh,"RESPONSE - IGNORE, not in EOR mode\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - WONT EOR\n");
	    print(sh,"RESPONSE - Send DONT EOR\n");
	  }
	  write(sh->sock_desc,"\377\376\031",3);
	  sh->flags &= ~EOR_ON;
	}
	break;
      }
      break;
    case 4:  /* DO */
      switch(c) {
      case TELOPT_ECHO:
	if (!(sh->flags&SECRET)) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DO Echo\n");
	    print(sh,"RESPONSE - IGNORE, already echoing\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DO Echo\n");
	    print(sh,"RESPONSE - Stop secrecy mode\n");
	  }
	  write(sh->sock_desc,"\377\373\001",3);
	  sh->flags &= ~SECRET;
	}
	break;
      case TELOPT_SGA:
	if (sh->flags&DIAGNOSTICS) {
	  print(sh,"RECEIVED - DO Single char mode\n");
	  print(sh,"RESPONSE - Send WONT Single char mode\n");
	}
	write(sh->sock_desc,"\377\374\003",3);
	break;
      case TELOPT_EOR:
	if (sh->flags&EOR_ON) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DO EOR\n");
	    print(sh,"RESPONSE - IGNORE, already in mode\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DO EOR\n");
	    print(sh,"RESPONSE - Send WILL EOR\n");
	  }
	  write(sh->sock_desc,"\377\373\031",3);
	  sh->flags |= EOR_ON;
	}
	break;
      default:
	if (sh->flags&DIAGNOSTICS) {
	  print(sh,"RECEIVED - DO Unsupported Option\n");
	  print(sh,"RESPONSE - Send WONT <option>\n");
	}
	pling[0]='\377';
	pling[1]='\374';
	pling[2]=c;
	write(sh->sock_desc,pling,3);
	break;
      }
      break;
    case 5:  /* DONT */
      switch(c) {
      case TELOPT_ECHO:
	if (sh->flags&SECRET) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DONT Echo\n");
	    print(sh,"RESPONSE - IGNORE, already in secrecy mode\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DONT Echo\n");
	    print(sh,"RESPONSE - Enter secrecy mode\n");
	  }
  /*	write(sh->sock_desc,"\377\374\001",3);
	sh->flags |= SECRET;
	I'm not sure about this. The only thing I know which sends this is
	Uglymug, and it expects it to mean that local echo _should_ be done,
	so, for now, in the interests of uglymug compatibility, it's
	commented out.  Whether Uglymug is wrong, I don't know, but ho hum.
	Jonathan
	*/
	}
	break;
      case TELOPT_SGA:
	if (sh->flags&DIAGNOSTICS) {
	  print(sh,"RECEIVED - DONT Single char mode\n");
	  print(sh,"RESPONSE - Ignore (TUsh is already in this mode)\n");
	}
	break;
      case TELOPT_EOR:
	if (!(sh->flags&EOR_ON)) {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DONT EOR\n");
	    print(sh,"RESPONSE - IGNORE, already in mode\n");
	  }
	}
	else {
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DONT EOR\n");
	    print(sh,"RESPONSE - Send WONT EOR\n");
	  }
	  write(sh->sock_desc,"\377\374\031",3);
	  sh->flags &= ~EOR_ON;
	}
	break;
      }
      break;
    }
    if (sh->telnetcontrol > 1) {
      sh->telnetcontrol = 0;
      return;
    }
    switch(c) {
    /* IP kill connection */
    case IP:                                        /* IP control */
      if (sh->flags&DIAGNOSTICS) {
	print(sh,"RECEIVED - Interrupt Process\n");
	print(sh,"RESPONSE - Close connection\n");
      }
      shell_print(sh,"Foreign host requested close.");
      close_com(sh,"");
      break;
      /* prompt processing stuff */
    case GA:                                     /* Go ahead (prompt) */
      if (!(sh->flags&EOR_ON)) {
	sh->inputline[sh->len++]=255;
	c = '\0';
      }
      break;
    case EOR:                                 /* EOR  (prompt)  */
      if (sh->flags&EOR_ON) {
	sh->inputline[sh->len++]=255;
	c = '\0';
      }
      break;
    case WILL:
      sh->telnetcontrol = 2;
      break;
    case WONT:
      sh->telnetcontrol = 3;
      break;
    case DO:
      sh->telnetcontrol = 4;
      break;
    case DONT:
      sh->telnetcontrol = 5;
      break;
    }
    if (sh->telnetcontrol == 1)
      sh->telnetcontrol = 0;
    if (c != '\0')
      return;
  }
  if (c != '\n' && c != '\r')
    sh->lastwasnl = sh->lastwaslf = 0;
  switch (c) {
  case IAC: 
    sh->telnetcontrol = 1;
    break;
  case '\n':
    if (sh->lastwaslf)
      break;
    sh->lastwasnl = 1;
    sh->inputline[sh->len++] = c;
    input_processing(sh, sh->inputline, sh->len);
    sh->len = 0;
    break;
  case '\r':
    if (sh->lastwasnl)
      break;
    sh->lastwaslf = 1;
    sh->inputline[sh->len++] = '\n';
    input_processing(sh, sh->inputline, sh->len);
    sh->len = 0;
    break;
  case '\0':
    if (sh->len > 0) {
      sh->inputline[sh->len++] = c;
      input_processing(sh, sh->inputline, sh->len);
    }
    sh->len = 0;
    break;
#ifdef GRAPHICS
  case '@':
    if (sh->grmode == GRNONE || sh->grmode == GRBSX) {
      sh->bsxgraphics = 1;
      sh->graphicslen = 0;
      sh->inputgraphics[sh->graphicslen++] = c;
    }
    else
      sh->inputline[sh->len++] = c;
    break;
  case 7:
    if (sh->grmode != GRBSX) {
      sh->graphics = 1;
      sh->graphicslen = 0;
      sh->inputgraphics[sh->graphicslen++] = c;
    }
    else
      sh->inputline[sh->len++]=c;
    break;
#endif
  default:
    if (sh->len >= MAXLEN-10) {
      if (sh->len >= MAXLEN)
	sh->len = MAXLEN-1;
      sh->inputline[sh->len++]='\0';
      input_processing(sh, sh->inputline, sh->len);
      sh->len = 0;
    }
    sh->inputline[sh->len++] = c;
    break;
  }
}

/* recieves a line from the server */

void read_in(sh)
shell *sh;
{
  long chars_ready = 0;
  int sd;
  unsigned int flags, flags2;
  int i;
  char *m, tmp[80];
  shell *scan;
#ifdef GRAPHICS
  static int more_graphics = 0;
  static char *more_graphics_str;
  char *bsxcom;
#endif

  flags=sh->flags&NO_UPDATE;
  if (sh==current_shell && !flags && !(sh->flags&HALF_LINE)) delete_input(sh);
  sh->flags |= NO_UPDATE;

  sd=sh->sock_desc;
#ifdef SYSV
  if (ioctl(sd,FIONREAD,&chars_ready) == -1) {
    if (errno == ENOLINK) {
      shell_print(sh,"Connection closed by foreign host");
      close_com(sh,0);
      update(sh);
      return;
    }
  }
#else
  if (ioctl(sd,FIONREAD,&chars_ready) == -1) handle_error("Ioctl failed");
#endif
  if (!chars_ready) {
    shell_print(sh,"Connection closed by foreign host");
    close_com(sh,0);
    update(sh);
    return;
  }
  m = malloc(chars_ready+1);
  if (read(sd, m, chars_ready) == -1) 
    handle_error("Reading from socket.");
  for (i = 0; i < chars_ready; i++)
    parse_input(sh, m[i], 0);
  parse_input(sh, '\0', 1);
  free(m);
  
  if (sh != current_shell) {
    if ((sh->flags&MONITOR) && !(sh->flags&MONITOR_GONEOFF)) {
      sh->flags |= MONITOR_GONEOFF;
      flags2 = current_shell->flags&WORD_WRAP;
      current_shell->flags &= ~WORD_WRAP;
      for(i = 1, scan = first_shell; scan; i++, scan = scan->next)
	if (scan == sh) break;
      sprintf(tmp,"%s Activity in shell %d.", SHELL_PRINT, i);
      hilight_in_current(sh, tmp);
      current_shell->flags |= flags2;
    }
  }

  sh->flags &= ~NO_UPDATE;
  sh->flags |= flags;
  if (sh==current_shell && !(sh->flags&NO_UPDATE) && !(sh->flags&HALF_LINE)) 
    draw_input(sh);
}

/* open up a connection to a server */

void open_socket(sh,host,port)
char *host;
int port;
shell *sh;
{
  struct in_addr inet_address;
  struct hostent *hp;
  struct sockaddr_in sock;
  int *address;

/* first convert 'host' into a recognisable address */

  if (isalpha(*host)) {
    hp=gethostbyname(host);
    if (!hp) {
      shell_print(sh,"Unknown hostname.");
      return;
    }
    memcpy((char *)&inet_address,hp->h_addr,sizeof(struct in_addr));
  }
  else inet_address.s_addr=inet_addr(host);
  if (inet_address.s_addr == -1) {
    shell_print(sh,"Malformed number.");
    return;
  }
  sprintf(scratch,"%s Connecting to server : %s ...\n",SHELL_PRINT,host);
  print(sh,scratch);

  /* now grab a socket */  
  
  sh->sock_desc=socket(AF_INET,SOCK_STREAM,0);
  if (sh->sock_desc == -1) {
    shell_print(sh,"Unable to open socket.");
    return;
  }
  sock.sin_family=AF_INET;
  sock.sin_port=htons(port);
  sock.sin_addr=inet_address;
    
  /* and try to connect */
  
  if (connect(sh->sock_desc,(struct sockaddr *)&sock,sizeof(sock)) == -1) {
    shell_print(sh,"Couldn't connect to site.");
    /*    shell_print(sh,sys_errlist[errno]);*/
    close(sh->sock_desc);
    return;
  }
  shell_print(sh,"Connected to server ...");
  sh->flags |= CONNECTED;
  sh->flags &= ~COMMAND;
}

/* the open command */

void open_com(sh,str)
shell *sh;
char *str;
{
  char *scan;
  int port=0;
  if (sh->flags&CONNECTED) {
    shell_print(sh,"Connection Already Established.");
    return;
  }
  if (!*str) {
    shell_print(sh,"Syntax - open <host> <port>");
    return;
  }
  scan=str;
  while(*scan && (*scan!=' ')) scan++;
  if (*scan) {
    *scan++=0;
    while(*scan && (*scan==' ')) scan++;
    port=atoi(scan);
  }
  if (!port) port=23;
  open_socket(sh,str,port);
}


