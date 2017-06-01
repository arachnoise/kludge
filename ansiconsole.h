//ansiconsole.h

#ifndef ANSI_CONSOLE_H
#define ANSI_CONSOLE_H

#include <windows.h>  

#define MAX_ARG_LENGTH		3		//longest number that the parser will accept
#define BUF_SIZE			256	    //for buffered and unbuffered input
#define IR_BUF_SIZE 		128	 	//for input records

//character io definitions

#define TAB_SIZE			8

#define END_OF_TEXT			3
#define BELL				7
#define ESCAPE				27
#define DEL					127

//console definitions

#define ATTR_OFF			0
#define ATTR_BOLD 			1
#define ATTR_UNDERSCORE 	4		//not supported
#define ATTR_BLINK			5		//not supported
#define ATTR_REVERSE		7
#define ATTR_CONCEALED		8		//echo off

#define COLOR_FG_BLACK		30
#define COLOR_FG_RED		31
#define COLOR_FG_GREEN		32
#define COLOR_FG_YELLOW		33
#define COLOR_FG_BLUE		34
#define COLOR_FG_MAGENTA	35
#define COLOR_FG_CYAN		36
#define COLOR_FG_WHITE		37

#define COLOR_BG_BLACK		40
#define COLOR_BG_RED		41
#define COLOR_BG_GREEN		42
#define COLOR_BG_YELLOW		43
#define COLOR_BG_BLUE		44
#define COLOR_BG_MAGENTA	45
#define COLOR_BG_CYAN		46
#define COLOR_BG_WHITE		47

#define CURSOR_MOVE			'H'
#define CURSOR_HOME			'f'
#define CURSOR_UP			'A'
#define CURSOR_DOWN			'B'
#define CURSOR_FORWARD		'C'
#define CURSOR_BACKWARD		'D'
#define CURSOR_SAVE			's'
#define	CURSOR_RESTORE		'u'

#define ERASE_DISPLAY		'J'			//'2' must precede 'J' for this to work
#define ERASE_LINE			'K'

#define CONSOLE_TEXT_MODE	'm'

#define DISPLAY_MODIFY		'='
#define	CONSOLE_GRAPHICS_MODE  'h'
#define CONSOLE_GRAPHICS_RESET 'l'

#define GFX_LINE_WRAP	7

typedef struct stackItem
{
	char *string;
	struct stackItem *next;
} stackItem;

//stack functions

void emptyStack(stackItem**);
int push(stackItem**, char*);
char* pop(stackItem**);

//ansi processing and evaluation functions

void evaluateANSI(HANDLE, stackItem**);
int parseInput(HANDLE, char, stackItem**);
int isANSICommand(char*, char);
int isNumber(char *);

//screen buffer related

void ValidateAndUpdate(HANDLE, PCOORD, BOOL, CONSOLE_SCREEN_BUFFER_INFO);
void ScrollScreenBuffer(HANDLE, CONSOLE_SCREEN_BUFFER_INFO);

//processing and error functions

int InitConsole(PHANDLE, PHANDLE);
int ResetConsole( );
int KeyEventProc(KEY_EVENT_RECORD, stackItem**, HANDLE);
void exitWithErr(char *str);

#endif