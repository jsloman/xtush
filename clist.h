/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User Shell            Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/

/* 
        this is the file clist.c

        here is a list of all the TUsh commands
*/

void quit_out(),start_new_shell(),kill_shell(),next_shell(),
  previous_shell(),refresh_shell(),change_prompt(),send_com(),
  echo_com(),pipe_com(),shell_escape(),execute(),null_com(),
  set_kb(),set_kh(),set_sb(),set_sh(),toggle_echo(),view_time(),
  set_time_str(),status(),set_scratch(),no_alias_pipe_com(),
  set_diag(),toggle_blanks(),help(),set_wildcard(),
  toggle_alias(),toggle_trigger(),toggle_autodie(),toggle_output(),
  toggle_prompt_mode(),set_title(),set_bell_mode(),monitor_shell();

#ifndef NO_FANCY_MALLOC
void check_malloc();
#endif

extern void alias_com(),unalias(),show_history(),do_history(),
  start_script(),stop_script(),untrigger(),trigger_com(),set_bailout(),
  match_com(),open_com(),close_com(),hilight(),clear_screen(),
  change_dir(),pwd(),set_wrap_size(),change_wrap(),recall(),
  create_child(),set_child_input(),set_child_output(),kill_child(),
  hilight_in_current(),print_in_current();
       

struct com {
  char *text;
  void *function;
};

struct com coms[]={
        "quit",quit_out,
        "refresh",refresh_shell,
        "repaint",refresh_shell,
        "new",start_new_shell,
        "kill",kill_shell,
        "next",next_shell,
        "previous",previous_shell,
        "history",show_history,
        "last ",do_history,
        "%",do_history,
        "echo",echo_com,
        "send",send_com,
        "#",null_com,
        "alias",alias_com,
        "unalias",unalias,
        "trigger",trigger_com,
        "untrigger",untrigger,
        "pipe",pipe_com,
        "|",pipe_com,
        "<",no_alias_pipe_com,
        "execute",execute,
        "shell",shell_escape,
        "!",shell_escape,
        "set kb",set_kb,
        "set kh",set_kh,
        "set sb",set_sb,
        "set sh",set_sh,
        "set echo",toggle_echo,
	"set alias",toggle_alias,
	"set trigger",toggle_trigger,
	"set output",toggle_output,
	"set prompt mode",toggle_prompt_mode,
        "set prompt",change_prompt,
	"set bell",set_bell_mode,
	"monitor",monitor_shell,
	"titlebar",set_title,
	"auto kill",toggle_autodie,
        "time",view_time,
        "set time",set_time_str,
        "set wildcard",set_wildcard,
        "stat",status,
        "set scratch",set_scratch,
        "script",start_script,
        "stop script",stop_script,
        "hilight",hilight,
	"chilight",hilight_in_current,
	"cecho",print_in_current,
        "cls",clear_screen,
        "set bailout",set_bailout,
        "match",match_com,
	"open",open_com,
	"close",close_com,
	"cd",change_dir,
	"pwd",pwd,
        "set wrap size",set_wrap_size,
	"set wrap",change_wrap,
	"recall",recall,
	"set opts",set_diag,
	"set opt",set_diag,
	"set blank",toggle_blanks,
	"help",help,
#ifndef NO_FANCY_MALLOC
	"malloc",check_malloc,
#endif
	0,0 };
