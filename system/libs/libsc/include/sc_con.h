//sc_con.h
//System call library - console functions
//Bryan E. Topp <betopp@betopp.com> 2021
#ifndef _SC_CON_H
#define _SC_CON_H

#include <stdint.h>
#include <sys/types.h>

//Console initialization info
typedef struct _sc_con_init_s
{
	//Console flags (unused for now)
	int    flags;
	
	//Framebuffer config
	int    fb_width;
	int    fb_height;
	size_t fb_stride;
	
} _sc_con_init_t;

//Initializes console usage for the calling process.
//Flags is currently unused. Returns 0 on success or a negative error number.
int _sc_con_init(const _sc_con_init_t *buf_ptr, ssize_t buf_len);

//Presents the user's framebuffer to the console.
//The geometry of the buffer is configured when calling _sc_con_init.
//Returns 0 on success or a negative error number.
int _sc_con_flip(const void *fb_ptr, int flags);

//Mouse buttons that the kernel reports from its console.
typedef enum _sc_con_mbutton_e
{
	_SC_CON_MBUTTON_LEFT   = 1,
	_SC_CON_MBUTTON_RIGHT  = 2,
	_SC_CON_MBUTTON_MIDDLE = 4,
} _sc_con_mbutton_t;

//Scancodes that the kernel reports from its console.
typedef enum _sc_con_scancode_e
{
	_SC_CON_SCANCODE_UNKNOWN = 0,
	_SC_CON_SCANCODE_A = 4,
	_SC_CON_SCANCODE_B = 5,
	_SC_CON_SCANCODE_C = 6,
	_SC_CON_SCANCODE_D = 7,
	_SC_CON_SCANCODE_E = 8,
	_SC_CON_SCANCODE_F = 9,
	_SC_CON_SCANCODE_G = 10,
	_SC_CON_SCANCODE_H = 11,
	_SC_CON_SCANCODE_I = 12,
	_SC_CON_SCANCODE_J = 13,
	_SC_CON_SCANCODE_K = 14,
	_SC_CON_SCANCODE_L = 15,
	_SC_CON_SCANCODE_M = 16,
	_SC_CON_SCANCODE_N = 17,
	_SC_CON_SCANCODE_O = 18,
	_SC_CON_SCANCODE_P = 19,
	_SC_CON_SCANCODE_Q = 20,
	_SC_CON_SCANCODE_R = 21,
	_SC_CON_SCANCODE_S = 22,
	_SC_CON_SCANCODE_T = 23,
	_SC_CON_SCANCODE_U = 24,
	_SC_CON_SCANCODE_V = 25,
	_SC_CON_SCANCODE_W = 26,
	_SC_CON_SCANCODE_X = 27,
	_SC_CON_SCANCODE_Y = 28,
	_SC_CON_SCANCODE_Z = 29,
	_SC_CON_SCANCODE_1 = 30,
	_SC_CON_SCANCODE_2 = 31,
	_SC_CON_SCANCODE_3 = 32,
	_SC_CON_SCANCODE_4 = 33,
	_SC_CON_SCANCODE_5 = 34,
	_SC_CON_SCANCODE_6 = 35,
	_SC_CON_SCANCODE_7 = 36,
	_SC_CON_SCANCODE_8 = 37,
	_SC_CON_SCANCODE_9 = 38,
	_SC_CON_SCANCODE_0 = 39,
	_SC_CON_SCANCODE_RETURN = 40,
	_SC_CON_SCANCODE_ESCAPE = 41,
	_SC_CON_SCANCODE_BACKSPACE = 42,
	_SC_CON_SCANCODE_TAB = 43,
	_SC_CON_SCANCODE_SPACE = 44,
	_SC_CON_SCANCODE_MINUS = 45,
	_SC_CON_SCANCODE_EQUALS = 46,
	_SC_CON_SCANCODE_LEFTBRACKET = 47,
	_SC_CON_SCANCODE_RIGHTBRACKET = 48,
	_SC_CON_SCANCODE_BACKSLASH = 49,
	_SC_CON_SCANCODE_NONUSHASH = 50,
	_SC_CON_SCANCODE_SEMICOLON = 51,
	_SC_CON_SCANCODE_APOSTROPHE = 52,
	_SC_CON_SCANCODE_GRAVE = 53,
	_SC_CON_SCANCODE_COMMA = 54,
	_SC_CON_SCANCODE_PERIOD = 55,
	_SC_CON_SCANCODE_SLASH = 56,
	_SC_CON_SCANCODE_CAPSLOCK = 57,
	_SC_CON_SCANCODE_F1 = 58,
	_SC_CON_SCANCODE_F2 = 59,
	_SC_CON_SCANCODE_F3 = 60,
	_SC_CON_SCANCODE_F4 = 61,
	_SC_CON_SCANCODE_F5 = 62,
	_SC_CON_SCANCODE_F6 = 63,
	_SC_CON_SCANCODE_F7 = 64,
	_SC_CON_SCANCODE_F8 = 65,
	_SC_CON_SCANCODE_F9 = 66,
	_SC_CON_SCANCODE_F10 = 67,
	_SC_CON_SCANCODE_F11 = 68,
	_SC_CON_SCANCODE_F12 = 69,
	_SC_CON_SCANCODE_PRINTSCREEN = 70,
	_SC_CON_SCANCODE_SCROLLLOCK = 71,
	_SC_CON_SCANCODE_PAUSE = 72,
	_SC_CON_SCANCODE_INSERT = 73,
	_SC_CON_SCANCODE_HOME = 74,
	_SC_CON_SCANCODE_PAGEUP = 75,
	_SC_CON_SCANCODE_DELETE = 76,
	_SC_CON_SCANCODE_END = 77,
	_SC_CON_SCANCODE_PAGEDOWN = 78,
	_SC_CON_SCANCODE_RIGHT = 79,
	_SC_CON_SCANCODE_LEFT = 80,
	_SC_CON_SCANCODE_DOWN = 81,
	_SC_CON_SCANCODE_UP = 82,
	_SC_CON_SCANCODE_NUMLOCKCLEAR = 83,
	_SC_CON_SCANCODE_KP_DIVIDE = 84,
	_SC_CON_SCANCODE_KP_MULTIPLY = 85,
	_SC_CON_SCANCODE_KP_MINUS = 86,
	_SC_CON_SCANCODE_KP_PLUS = 87,
	_SC_CON_SCANCODE_KP_ENTER = 88,
	_SC_CON_SCANCODE_KP_1 = 89,
	_SC_CON_SCANCODE_KP_2 = 90,
	_SC_CON_SCANCODE_KP_3 = 91,
	_SC_CON_SCANCODE_KP_4 = 92,
	_SC_CON_SCANCODE_KP_5 = 93,
	_SC_CON_SCANCODE_KP_6 = 94,
	_SC_CON_SCANCODE_KP_7 = 95,
	_SC_CON_SCANCODE_KP_8 = 96,
	_SC_CON_SCANCODE_KP_9 = 97,
	_SC_CON_SCANCODE_KP_0 = 98,
	_SC_CON_SCANCODE_KP_PERIOD = 99,
	_SC_CON_SCANCODE_NONUSBACKSLASH = 100,
	_SC_CON_SCANCODE_APPLICATION = 101,
	_SC_CON_SCANCODE_POWER = 102,
	_SC_CON_SCANCODE_KP_EQUALS = 103,
	_SC_CON_SCANCODE_F13 = 104,
	_SC_CON_SCANCODE_F14 = 105,
	_SC_CON_SCANCODE_F15 = 106,
	_SC_CON_SCANCODE_F16 = 107,
	_SC_CON_SCANCODE_F17 = 108,
	_SC_CON_SCANCODE_F18 = 109,
	_SC_CON_SCANCODE_F19 = 110,
	_SC_CON_SCANCODE_F20 = 111,
	_SC_CON_SCANCODE_F21 = 112,
	_SC_CON_SCANCODE_F22 = 113,
	_SC_CON_SCANCODE_F23 = 114,
	_SC_CON_SCANCODE_F24 = 115,
	_SC_CON_SCANCODE_EXECUTE = 116,
	_SC_CON_SCANCODE_HELP = 117,
	_SC_CON_SCANCODE_MENU = 118,
	_SC_CON_SCANCODE_SELECT = 119,
	_SC_CON_SCANCODE_STOP = 120,
	_SC_CON_SCANCODE_AGAIN = 121,
	_SC_CON_SCANCODE_UNDO = 122,
	_SC_CON_SCANCODE_CUT = 123,
	_SC_CON_SCANCODE_COPY = 124,
	_SC_CON_SCANCODE_PASTE = 125,
	_SC_CON_SCANCODE_FIND = 126,
	_SC_CON_SCANCODE_MUTE = 127,
	_SC_CON_SCANCODE_VOLUMEUP = 128,
	_SC_CON_SCANCODE_VOLUMEDOWN = 129,
	_SC_CON_SCANCODE_KP_COMMA = 133,
	_SC_CON_SCANCODE_KP_EQUALSAS400 = 134,
	_SC_CON_SCANCODE_INTERNATIONAL1 = 135,
	_SC_CON_SCANCODE_INTERNATIONAL2 = 136,
	_SC_CON_SCANCODE_INTERNATIONAL3 = 137,
	_SC_CON_SCANCODE_INTERNATIONAL4 = 138,
	_SC_CON_SCANCODE_INTERNATIONAL5 = 139,
	_SC_CON_SCANCODE_INTERNATIONAL6 = 140,
	_SC_CON_SCANCODE_INTERNATIONAL7 = 141,
	_SC_CON_SCANCODE_INTERNATIONAL8 = 142,
	_SC_CON_SCANCODE_INTERNATIONAL9 = 143,
	_SC_CON_SCANCODE_LANG1 = 144,
	_SC_CON_SCANCODE_LANG2 = 145,
	_SC_CON_SCANCODE_LANG3 = 146,
	_SC_CON_SCANCODE_LANG4 = 147,
	_SC_CON_SCANCODE_LANG5 = 148,
	_SC_CON_SCANCODE_LANG6 = 149,
	_SC_CON_SCANCODE_LANG7 = 150,
	_SC_CON_SCANCODE_LANG8 = 151,
	_SC_CON_SCANCODE_LANG9 = 152,
	_SC_CON_SCANCODE_ALTERASE = 153,
	_SC_CON_SCANCODE_SYSREQ = 154,
	_SC_CON_SCANCODE_CANCEL = 155,
	_SC_CON_SCANCODE_CLEAR = 156,
	_SC_CON_SCANCODE_PRIOR = 157,
	_SC_CON_SCANCODE_RETURN2 = 158,
	_SC_CON_SCANCODE_SEPARATOR = 159,
	_SC_CON_SCANCODE_OUT = 160,
	_SC_CON_SCANCODE_OPER = 161,
	_SC_CON_SCANCODE_CLEARAGAIN = 162,
	_SC_CON_SCANCODE_CRSEL = 163,
	_SC_CON_SCANCODE_EXSEL = 164,
	_SC_CON_SCANCODE_KP_00 = 176,
	_SC_CON_SCANCODE_KP_000 = 177,
	_SC_CON_SCANCODE_THOUSANDSSEPARATOR = 178,
	_SC_CON_SCANCODE_DECIMALSEPARATOR = 179,
	_SC_CON_SCANCODE_CURRENCYUNIT = 180,
	_SC_CON_SCANCODE_CURRENCYSUBUNIT = 181,
	_SC_CON_SCANCODE_KP_LEFTPAREN = 182,
	_SC_CON_SCANCODE_KP_RIGHTPAREN = 183,
	_SC_CON_SCANCODE_KP_LEFTBRACE = 184,
	_SC_CON_SCANCODE_KP_RIGHTBRACE = 185,
	_SC_CON_SCANCODE_KP_TAB = 186,
	_SC_CON_SCANCODE_KP_BACKSPACE = 187,
	_SC_CON_SCANCODE_KP_A = 188,
	_SC_CON_SCANCODE_KP_B = 189,
	_SC_CON_SCANCODE_KP_C = 190,
	_SC_CON_SCANCODE_KP_D = 191,
	_SC_CON_SCANCODE_KP_E = 192,
	_SC_CON_SCANCODE_KP_F = 193,
	_SC_CON_SCANCODE_KP_XOR = 194,
	_SC_CON_SCANCODE_KP_POWER = 195,
	_SC_CON_SCANCODE_KP_PERCENT = 196,
	_SC_CON_SCANCODE_KP_LESS = 197,
	_SC_CON_SCANCODE_KP_GREATER = 198,
	_SC_CON_SCANCODE_KP_AMPERSAND = 199,
	_SC_CON_SCANCODE_KP_DBLAMPERSAND = 200,
	_SC_CON_SCANCODE_KP_VERTICALBAR = 201,
	_SC_CON_SCANCODE_KP_DBLVERTICALBAR = 202,
	_SC_CON_SCANCODE_KP_COLON = 203,
	_SC_CON_SCANCODE_KP_HASH = 204,
	_SC_CON_SCANCODE_KP_SPACE = 205,
	_SC_CON_SCANCODE_KP_AT = 206,
	_SC_CON_SCANCODE_KP_EXCLAM = 207,
	_SC_CON_SCANCODE_KP_MEMSTORE = 208,
	_SC_CON_SCANCODE_KP_MEMRECALL = 209,
	_SC_CON_SCANCODE_KP_MEMCLEAR = 210,
	_SC_CON_SCANCODE_KP_MEMADD = 211,
	_SC_CON_SCANCODE_KP_MEMSUBTRACT = 212,
	_SC_CON_SCANCODE_KP_MEMMULTIPLY = 213,
	_SC_CON_SCANCODE_KP_MEMDIVIDE = 214,
	_SC_CON_SCANCODE_KP_PLUSMINUS = 215,
	_SC_CON_SCANCODE_KP_CLEAR = 216,
	_SC_CON_SCANCODE_KP_CLEARENTRY = 217,
	_SC_CON_SCANCODE_KP_BINARY = 218,
	_SC_CON_SCANCODE_KP_OCTAL = 219,
	_SC_CON_SCANCODE_KP_DECIMAL = 220,
	_SC_CON_SCANCODE_KP_HEXADECIMAL = 221,
	_SC_CON_SCANCODE_LCTRL = 224,
	_SC_CON_SCANCODE_LSHIFT = 225,
	_SC_CON_SCANCODE_LALT = 226,
	_SC_CON_SCANCODE_LGUI = 227,
	_SC_CON_SCANCODE_RCTRL = 228,
	_SC_CON_SCANCODE_RSHIFT = 229,
	_SC_CON_SCANCODE_RALT = 230,
	_SC_CON_SCANCODE_RGUI = 231,
	_SC_CON_SCANCODE_MODE = 257,
	_SC_CON_SCANCODE_AUDIONEXT = 258,
	_SC_CON_SCANCODE_AUDIOPREV = 259,
	_SC_CON_SCANCODE_AUDIOSTOP = 260,
	_SC_CON_SCANCODE_AUDIOPLAY = 261,
	_SC_CON_SCANCODE_AUDIOMUTE = 262,
	_SC_CON_SCANCODE_MEDIASELECT = 263,
	_SC_CON_SCANCODE_WWW = 264,
	_SC_CON_SCANCODE_MAIL = 265,
	_SC_CON_SCANCODE_CALCULATOR = 266,
	_SC_CON_SCANCODE_COMPUTER = 267,
	_SC_CON_SCANCODE_AC_SEARCH = 268,
	_SC_CON_SCANCODE_AC_HOME = 269,
	_SC_CON_SCANCODE_AC_BACK = 270,
	_SC_CON_SCANCODE_AC_FORWARD = 271,
	_SC_CON_SCANCODE_AC_STOP = 272,
	_SC_CON_SCANCODE_AC_REFRESH = 273,
	_SC_CON_SCANCODE_AC_BOOKMARKS = 274,
	_SC_CON_SCANCODE_BRIGHTNESSDOWN = 275,
	_SC_CON_SCANCODE_BRIGHTNESSUP = 276,
	_SC_CON_SCANCODE_DISPLAYSWITCH = 277,
	_SC_CON_SCANCODE_KBDILLUMTOGGLE = 278,
	_SC_CON_SCANCODE_KBDILLUMDOWN = 279,
	_SC_CON_SCANCODE_KBDILLUMUP = 280,
	_SC_CON_SCANCODE_EJECT = 281,
	_SC_CON_SCANCODE_SLEEP = 282,
	_SC_CON_SCANCODE_APP1 = 283,
	_SC_CON_SCANCODE_APP2 = 284,
	
	_SC_CON_SCANCODE_MAX = 512,
} _sc_con_scancode_t;

//Type of input event that the console reports
typedef enum _sc_con_input_type_e
{
	_SC_CON_INPUT_TYPE_NONE  = 0,
	_SC_CON_INPUT_TYPE_KBD   = 1,
	_SC_CON_INPUT_TYPE_MOUSE = 2,
	
} _sc_con_input_type_t;

//Data about an input event from the console
typedef union _sc_con_input_u
{
	struct {
		_sc_con_scancode_t scancode : 16;
		uint32_t state              : 8;
		uint32_t unused             : 4;
		uint32_t type               : 4;
	} kbd;
	
	struct {
		int dx                    : 10;
		int dy                    : 10;
		_sc_con_mbutton_t buttons : 8;
		uint32_t type             : 4;
	} mouse;
	
	struct {
		uint32_t nottype : 28;
		uint32_t type    : 4;
	} type;
	
} _sc_con_input_t;

//Looks for console input. Outputs as many events as are waiting and would fit in the buffer.
//Returns the size, in bytes, of the data returned. buf_len and the return value are in bytes.
ssize_t _sc_con_input(_sc_con_input_t *buf_ptr, ssize_t each_bytes, ssize_t buf_bytes);

//Hands-off the console if this process has it currently.
int _sc_con_pass(pid_t next);

#endif //_SC_CON_H
