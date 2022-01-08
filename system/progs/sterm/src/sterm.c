//sterm.c
//Stumpy terminal
//Bryan E. Topp <betopp@betopp.com> 2021

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sc.h>
#include <sc_con.h>

#include "confont.h"

//Todo - determine this dynamically
#define FB_WIDTH 640
#define FB_HEIGHT 480

//Backbuffer as allocated
uint32_t *fb_ptr;
int fb_width;
int fb_height;
size_t fb_stride;

//Text buffer holding console output, to be scrolled along
char *txt_buf; //Storage for text on screen / scrollback
char **txt_ptrs; //Points to array of pointers (one per line) pointing to characters (one per column)
int txt_cols; //Width of buffers in characters
int txt_rows; //Height of buffers in lines

//Size of screen in text buffer
int win_cols; //Width of screen in characters
int win_rows; //Height of screen in lines

//Origin of screen in text buffer
int scroll_row;
int scroll_col;

//Cursor position in text buffer (NOT RELATIVE TO SCREEN)
int curs_row;
int curs_col;

//Escape code that causes us to pass the console to another process
const char conswitch_code[] = { 0x1B, '[', 'C', 'C', 'S', 0 };
int conswitch_match = 0;
int32_t conswitch_pid = 0;

//File descriptor for back of pseudoterminal - that we hold onto
int pty_fd;

//File descriptor for front of pseudoterminal - that we give to the shell
int tty_fd;

//Keyboard modifier keys pressed
typedef enum kbd_mods_e
{
	KBD_MOD_LSHIFT = 0,
	KBD_MOD_RSHIFT = 1,
	KBD_MOD_LALT = 2,
	KBD_MOD_RALT = 3,
	KBD_MOD_LCTRL = 4,
	KBD_MOD_RCTRL = 5,
	
	KBD_MOD_SHIFT = 6,
	KBD_MOD_ALT = 7,
	KBD_MOD_CTRL = 8,
	
	KBD_MOD_MAX
} kbd_mods_t;
bool kbd_mods[KBD_MOD_MAX];

//Scancode-to-text mapping. Index is the standard HAL scancode enum.
//If an entry is \0, then no text character is generated for making that scancode.
//Each scancode has 4 translations - regular, shift, capslock, shift + capslock
const char keymap_usa[512][4] = 
{
	//Code                          Norm  Shft  Caps  Shft+Caps
	[_SC_CON_SCANCODE_A]           = {'a',  'A',  'A',  'a' },
	[_SC_CON_SCANCODE_B]           = {'b',  'B',  'B',  'b' },
	[_SC_CON_SCANCODE_C]           = {'c',  'C',  'C',  'c' },
	[_SC_CON_SCANCODE_D]           = {'d',  'D',  'D',  'd' },
	[_SC_CON_SCANCODE_E]           = {'e',  'E',  'E',  'e' },
	[_SC_CON_SCANCODE_F]           = {'f',  'F',  'F',  'f' },
	[_SC_CON_SCANCODE_G]           = {'g',  'G',  'G',  'g' },
	[_SC_CON_SCANCODE_H]           = {'h',  'H',  'H',  'h' },
	[_SC_CON_SCANCODE_I]           = {'i',  'I',  'I',  'i' },
	[_SC_CON_SCANCODE_J]           = {'j',  'J',  'J',  'j' },
	[_SC_CON_SCANCODE_K]           = {'k',  'K',  'K',  'k' },
	[_SC_CON_SCANCODE_L]           = {'l',  'L',  'L',  'l' },
	[_SC_CON_SCANCODE_M]           = {'m',  'M',  'M',  'm' },
	[_SC_CON_SCANCODE_N]           = {'n',  'N',  'N',  'n' },
	[_SC_CON_SCANCODE_O]           = {'o',  'O',  'O',  'o' },
	[_SC_CON_SCANCODE_P]           = {'p',  'P',  'P',  'p' },
	[_SC_CON_SCANCODE_Q]           = {'q',  'Q',  'Q',  'q' },
	[_SC_CON_SCANCODE_R]           = {'r',  'R',  'R',  'r' },
	[_SC_CON_SCANCODE_S]           = {'s',  'S',  'S',  's' },
	[_SC_CON_SCANCODE_T]           = {'t',  'T',  'T',  't' },
	[_SC_CON_SCANCODE_U]           = {'u',  'U',  'U',  'u' },
	[_SC_CON_SCANCODE_V]           = {'v',  'V',  'V',  'v' },
	[_SC_CON_SCANCODE_W]           = {'w',  'W',  'W',  'w' },
	[_SC_CON_SCANCODE_X]           = {'x',  'X',  'X',  'x' },
	[_SC_CON_SCANCODE_Y]           = {'y',  'Y',  'Y',  'y' },
	[_SC_CON_SCANCODE_Z]           = {'z',  'Z',  'Z',  'z' },
	[_SC_CON_SCANCODE_0]           = {'0',  ')',  '0',  ')' },
	[_SC_CON_SCANCODE_1]           = {'1',  '!',  '1',  '!' },
	[_SC_CON_SCANCODE_2]           = {'2',  '@',  '2',  '@' },
	[_SC_CON_SCANCODE_3]           = {'3',  '#',  '3',  '#' },
	[_SC_CON_SCANCODE_4]           = {'4',  '$',  '4',  '$' },
	[_SC_CON_SCANCODE_5]           = {'5',  '%',  '5',  '%' },
	[_SC_CON_SCANCODE_6]           = {'6',  '^',  '6',  '^' },
	[_SC_CON_SCANCODE_7]           = {'7',  '&',  '7',  '&' },
	[_SC_CON_SCANCODE_8]           = {'8',  '*',  '8',  '*' },
	[_SC_CON_SCANCODE_9]           = {'9',  '(',  '9',  '(' },
	[_SC_CON_SCANCODE_GRAVE]       = {'`',  '~',  '`',  '~' },
	[_SC_CON_SCANCODE_MINUS]       = {'-',  '_',  '-',  '_' },
	[_SC_CON_SCANCODE_EQUALS]      = {'=',  '+',  '=',  '+' },
	[_SC_CON_SCANCODE_BACKSLASH]   = {'\\', '|',  '\\', '|' },
	[_SC_CON_SCANCODE_SPACE]       = {' ',  ' ',  ' ',  ' ' },
	[_SC_CON_SCANCODE_TAB]         = {'\t', '\t', '\t', '\t'},
	[_SC_CON_SCANCODE_RETURN]      = {'\n', '\n', '\n', '\n'},
	[_SC_CON_SCANCODE_LEFTBRACKET] = {'[',  '{',  '[',  '{' },
	[_SC_CON_SCANCODE_RIGHTBRACKET]= {']',  '}',  ']',  '}' },
	
	[_SC_CON_SCANCODE_KP_DIVIDE]   = {'/',  '/',  '/',  '/' },
	[_SC_CON_SCANCODE_KP_MULTIPLY] = {'*',  '*',  '*',  '*' },
	[_SC_CON_SCANCODE_KP_MINUS]    = {'-',  '-',  '-',  '-' },
	[_SC_CON_SCANCODE_KP_PLUS]     = {'+',  '+',  '+',  '+' },
	[_SC_CON_SCANCODE_KP_ENTER]    = {'\n', '\n', '\n', '\n'},
	[_SC_CON_SCANCODE_KP_PERIOD]   = {'.',  '.',  '.',  '.' },
	[_SC_CON_SCANCODE_KP_0]        = {'0',  '0',  '0',  '0' },
	[_SC_CON_SCANCODE_KP_1]        = {'1',  '1',  '1',  '1' },
	[_SC_CON_SCANCODE_KP_2]        = {'2',  '2',  '2',  '2' },
	[_SC_CON_SCANCODE_KP_3]        = {'3',  '3',  '3',  '3' },
	[_SC_CON_SCANCODE_KP_4]        = {'4',  '4',  '4',  '4' },
	[_SC_CON_SCANCODE_KP_5]        = {'5',  '5',  '5',  '5' },
	[_SC_CON_SCANCODE_KP_6]        = {'6',  '6',  '6',  '6' },
	[_SC_CON_SCANCODE_KP_7]        = {'7',  '7',  '7',  '7' },
	[_SC_CON_SCANCODE_KP_8]        = {'8',  '8',  '8',  '8' },
	[_SC_CON_SCANCODE_KP_9]        = {'9',  '9',  '9',  '9' },
	
	[_SC_CON_SCANCODE_BACKSPACE]   = {8,    8,    8,    8   },
	[_SC_CON_SCANCODE_SLASH]       = {'/',  '?',  '/',  '?' },
	[_SC_CON_SCANCODE_PERIOD]      = {'.',  '>',  '.',  '>' },
	[_SC_CON_SCANCODE_COMMA]       = {',',  '<',  ',',  '<' },
	[_SC_CON_SCANCODE_SEMICOLON]   = {';',  ':',  ';',  ':' },
	[_SC_CON_SCANCODE_APOSTROPHE]  = {'\'', '"',  '\'', '"' },
};


//Draws a glyph at the given screen pixel.
static void glyph(int x, int y, char gl)
{
	for(int chy = 0; chy < confont_chy; chy++)
	{
		uint32_t *dst = (uint32_t*)(((char*)fb_ptr) + (x * sizeof(uint32_t)) + ( (y + chy) * fb_stride));
		for(int chx = 0; chx < confont_chx; chx++)
		{
			dst[chx] = (confont_bits[(uint8_t)gl][chy] & (1<<chx)) ? 0xFFFFFFFF : 0;
		}
	}
}

//Updates the origin of the screen, redrawing all changed cells.
static void setscroll(int new_scr_row, int new_scr_col)
{
	for(int sr = 0; sr < win_rows; sr++)
	{
		char *old_row = txt_ptrs[ (scroll_row + sr) % txt_rows ];
		char *new_row = txt_ptrs[ (new_scr_row        + sr) % txt_rows ];
		
		for(int sc = 0; sc < win_cols; sc++)
		{
			char old_char = old_row[ (scroll_col + sc) % txt_cols ];
			char new_char = new_row[ (new_scr_col        + sc) % txt_cols ];
			
			if(old_char != new_char)
				glyph(sc * confont_chx, sr * confont_chy, new_char);
		}
	}
	
	scroll_row = new_scr_row;
	scroll_col = new_scr_col;
}

//Advances the cursor to the next line.
static void newline(void)
{
	//Cursor moves one line forward
	curs_row = (curs_row + 1) % txt_rows;
	
	//Clear the new line
	memset(txt_ptrs[curs_row], ' ', txt_cols);
	
	//If the new line is just off the bottom of the screen, scroll down 1 line.
	if(curs_row == ((scroll_row + win_rows) % txt_rows))
		setscroll((scroll_row + 1) % txt_rows, scroll_col);
	
	//Return to beginning of next line
	curs_col = 0;
}

//Outputs a character to the console.
static void coutc(int ch)
{
	//Handle magic output to hand-off the console
	if(conswitch_match >= 0)
	{
		if(conswitch_code[conswitch_match] == ch)
		{
			conswitch_match++;
			if(conswitch_code[conswitch_match] == 0)
				conswitch_match = -1;
		}
	}
	else
	{
		if(ch < '0' || ch > '9')
		{
			conswitch_match = 0;
		}
		else
		{
			conswitch_pid *= 10;
			conswitch_pid += (ch - '0');
			conswitch_match--;
			if(conswitch_match < -12)
			{
				_sc_con_pass(conswitch_pid);
				conswitch_pid = 0;
				conswitch_match = 0;
			}
		}
	}
	
	//Unpaint old cursor before changing things
	glyph( 
		((txt_cols + curs_col - scroll_col) % win_cols) * confont_chx, 
		((txt_rows + curs_row - scroll_row) % win_rows) * confont_chy, 
		txt_ptrs[curs_row][curs_col]
	);

	
	if(ch == '\n')
	{
		newline();
		return;
	}
	
	if(ch == '\r')
	{
		curs_col = 0;
		return;
	}
	
	if(ch == '\t')
	{
		for(int ss = 0; ss < 4; ss++)
		{
			coutc(' ');
		}
		return;
	}
	
	if(ch == 8)
	{
		if(curs_col > 0)
			curs_col--;
		return;
	}
	
	if(ch == 7)
	{
		//Bell
		return;
	}
	
	//Store into the buffer and paint this one character.
	txt_ptrs[curs_row][curs_col] = ch;
	glyph( 
		((txt_cols + curs_col - scroll_col) % win_cols) * confont_chx, 
		((txt_rows + curs_row - scroll_row) % win_rows) * confont_chy, 
		ch
	);
	
	//Advance the cursor and scroll if necessary
	curs_col++;
	if(curs_col >= txt_cols)
		newline();
}

//Handles a keyboard input.
void kbd(_sc_con_scancode_t scancode, bool press)
{
	//Check if a modifier key is being changed
	static const _sc_con_scancode_t mod_scan[KBD_MOD_MAX] = 
	{
		[KBD_MOD_LSHIFT] = _SC_CON_SCANCODE_LSHIFT,
		[KBD_MOD_RSHIFT] = _SC_CON_SCANCODE_RSHIFT,
		[KBD_MOD_LCTRL] = _SC_CON_SCANCODE_LCTRL,
		[KBD_MOD_RCTRL] = _SC_CON_SCANCODE_RCTRL,
		[KBD_MOD_LALT] = _SC_CON_SCANCODE_LALT,
		[KBD_MOD_RALT] = _SC_CON_SCANCODE_RALT
	};
	for(int mm = 0; mm < KBD_MOD_MAX; mm++)
	{
		if(mod_scan[mm] != 0 && scancode == mod_scan[mm])
			kbd_mods[mm] = press;
	}
	
	//Recompute generic modifiers
	kbd_mods[KBD_MOD_ALT] = kbd_mods[KBD_MOD_LALT] || kbd_mods[KBD_MOD_RALT];
	kbd_mods[KBD_MOD_SHIFT] = kbd_mods[KBD_MOD_LSHIFT] || kbd_mods[KBD_MOD_RSHIFT];
	kbd_mods[KBD_MOD_CTRL] = kbd_mods[KBD_MOD_LCTRL] || kbd_mods[KBD_MOD_RCTRL];
	
	//React to normal keypresses
	if(press)
	{
		if(kbd_mods[KBD_MOD_CTRL])
		{
			if(scancode >= _SC_CON_SCANCODE_A && scancode <= _SC_CON_SCANCODE_Z)
			{
				uint8_t ctrlchar = 1 + scancode - _SC_CON_SCANCODE_A;
				
				//Todo - need a proper place to handle this stuff.
				//Is the kernel pty system really the best place?
				if(ctrlchar == 3)
				{
					//Ctrl-C
					killpg(tcgetpgrp(pty_fd), SIGTERM);
					return;
				}
				
				write(pty_fd, &ctrlchar, 1);
			}
		}
		else
		{
			if(scancode == _SC_CON_SCANCODE_UP)
			{
				write(pty_fd, (uint8_t[]){0x1b, 0x5b, 0x41}, 3);
			}
			else if(scancode == _SC_CON_SCANCODE_DOWN)
			{
				write(pty_fd, (uint8_t[]){0x1b, 0x5b, 0x42}, 3);
			}
			else if(scancode == _SC_CON_SCANCODE_LEFT)
			{
				write(pty_fd, (uint8_t[]){0x1b, 0x5b, 0x44}, 3);
			}
			else if(scancode == _SC_CON_SCANCODE_RIGHT)
			{
				write(pty_fd, (uint8_t[]){0x1b, 0x5b, 0x43}, 3);
			}
			else
			{
				int keymap_set = 0;
				if(kbd_mods[KBD_MOD_SHIFT])
					keymap_set += 1;
				
				uint8_t keyval = keymap_usa[scancode][keymap_set];
				if(keyval != 0)
					write(pty_fd, &keyval, 1);
			}
		}
	}
}


int main(int argc, const char **argv, const char **envp)
{
	//Ignore these for now
	(void)argc;
	(void)argv;
	(void)envp;
	
	//Open the back of the pseudoterminal for our side of communications
	char *pty_name_buf = "/dev/pty";
	pty_fd = open("/dev/pty", O_RDWR);
	if(pty_fd < 0)
	{
		perror("open /dev/pty");
		abort();
	}
	int pty_name_result = _sc_ioctl(pty_fd, _SC_IOCTL_SETNAME, pty_name_buf, strlen(pty_name_buf) + 1);
	if(pty_name_result < 0)
	{
		errno = -pty_name_result;
		perror("ioctl pty name");
		abort();
	}
	
	//Make sure we can open the front of the pseudoterminal, to hand it over to the shell
	tty_fd = open("/dev/tty", O_RDWR);
	if(tty_fd < 0)
	{
		perror("open /dev/tty");
		abort();
	}

	//Figure out framebuffer dimensions and allocate enough memory to back it	
	fb_width = FB_WIDTH;
	fb_height = FB_HEIGHT;
	fb_stride = FB_WIDTH*4;
	fb_ptr = malloc(fb_height * fb_stride);
	if(fb_ptr == NULL)
	{
		perror("malloc fb");
		abort();
	}
	memset(fb_ptr, 0, fb_height * fb_stride);
	
	//Tell kernel we'll be using our console output
	const _sc_con_init_t con_parms = 
	{
		.flags = 0,
		.fb_width = fb_width,
		.fb_height = fb_height,
		.fb_stride = fb_stride,
	};

	int con_err = _sc_con_init(&con_parms, sizeof(con_parms));
	if(con_err < 0)
	{
		errno = -con_err;
		perror("_sc_con_init");
		abort();
	}
	
	//Allocate text buffers based on screen and glyph size
	win_cols = fb_width / confont_chx;
	win_rows = fb_height / confont_chy;
	txt_cols = win_cols;
	txt_rows = win_rows * 4;
	
	txt_buf = malloc(txt_cols * txt_rows);
	if(txt_buf == NULL)
	{
		perror("malloc txt buf");
		abort();
	}
	memset(txt_buf, ' ', txt_rows * txt_cols);
	
	txt_ptrs = malloc(sizeof(char*) * txt_rows);
	if(txt_ptrs == NULL)
	{
		perror("malloc txt ptrs");
		abort();
	}
	for(int rr = 0; rr < txt_rows; rr++)
	{
		txt_ptrs[rr] = txt_buf + (rr * txt_cols);
	}	
	
	//Init scrolling
	setscroll(0, 0);
	
	//Show intro message
	dprintf(tty_fd, "sterm: " BUILDVERSION " by " BUILDUSER " at " BUILDDATE "\n");
	dprintf(tty_fd, "sterm: pid=%d ppid=%d\n", getpid(), getppid());
	dprintf(tty_fd, "sterm: spawning /bin/oksh-6.9\n");
	
	//Spawn the shell (spawn the getty?)
	pid_t forkpid = fork();
	if(forkpid < 0)
	{
		//Failed to fork
		perror("fork");
		abort();
	}
	if(forkpid == 0)
	{
		//Child process
		close(pty_fd);
		dup2(tty_fd, STDIN_FILENO);
		dup2(tty_fd, STDOUT_FILENO);
		dup2(tty_fd, STDERR_FILENO);
		execve("/bin/oksh-6.9", (char*[]){"oksh-6.9", NULL}, (char**)envp);
		perror("execve");
		abort(); //execl shouldn't return
	}
	
	//Parent process
	close(tty_fd);
	
	//Pump those pipes
	while(1)
	{
		//See if the console delivered us any new keyboard/mouse input
		while(1)
		{
			_sc_con_input_t from_con_buf[16];
			ssize_t from_con_len = _sc_con_input(from_con_buf, sizeof(from_con_buf[0]), sizeof(from_con_buf));
			if(from_con_len <= 0)
				break;
			
			for(uint32_t ii = 0; ii < from_con_len / sizeof(_sc_con_input_t); ii++)
			{
				switch(from_con_buf[ii].type.type)
				{
					case _SC_CON_INPUT_TYPE_KBD:
						kbd(from_con_buf[ii].kbd.scancode, from_con_buf[ii].kbd.state);
						break;
					case _SC_CON_INPUT_TYPE_MOUSE:
						break;
					default:
						break;
				}
			}
		}
		
		//See if the shell output characters for the screen
		while(1)
		{
			uint8_t from_shell_buf[64];
			ssize_t from_shell_len = _sc_read(pty_fd, from_shell_buf, sizeof(from_shell_buf));
			if(from_shell_len <= 0)
				break;
			
			for(ssize_t cc = 0; cc < from_shell_len; cc++)
			{
				coutc(from_shell_buf[cc]);
			}
			
			//Paint cursor
			glyph( 
				((txt_cols + curs_col - scroll_col) % win_cols) * confont_chx, 
				((txt_rows + curs_row - scroll_row) % win_rows) * confont_chy, 
				'_'
			);
		}

		//Update the framebuffer as drawn
		int flip_result = _sc_con_flip(fb_ptr, 0);
		if(flip_result < 0)
		{
			errno = -flip_result;
			perror("ioctl flip");
			abort();
		}
		
		//See if our shell died
		int wait_status = 0;
		pid_t wait_pid = waitpid(-1, &wait_status, WNOHANG);
		if( (wait_pid == forkpid) && WIFEXITED(wait_status) )
		{
			//Shell died. Quit.
			exit(WEXITSTATUS(wait_status));
			abort();
		}
		
		//Wait if nothing happened
		_sc_pause();
	}
}