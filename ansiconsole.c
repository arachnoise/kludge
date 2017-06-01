//ansiparse.c
//
//it parses ansi escape sequences

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>

#include "ansiconsole.h"

//console variables

DWORD fdwOldInputMode;
DWORD fdwOldOutputMode;
WORD wOldColorAttrs;

BOOL bEchoOn;
BOOL bEolAutoWrap;
COORD dwSavedCursor;

void emptyStack(stackItem** theStack)
{
	char* str;
	
	while(*theStack != NULL)
	{
		char *str = pop(theStack);
		
		if(str != NULL)
			free(str);
	}
}

//push( )
//
//takes a stack and pushes a record holding a c-string onto it
//push( ) fails catastrophically if malloc returns NULL values.
//
//this is okay as malloc rarely fails and if then only when 
//system memory is nearly full
//
int push(stackItem** theStack, char* item)
{
	if(*theStack == NULL)
	{
		*theStack = (stackItem*)malloc(sizeof(stackItem));
		
		if(*theStack == NULL)
			exitWithErr("Error: could not allocate memory for the stack\n");	
			
		(*theStack)->string = item;
		(*theStack)->next = NULL;
	}
	else
	{
		stackItem *current = (stackItem*)malloc(sizeof(stackItem));
		
		if(current == NULL)
			exitWithErr( "Error: could not allocate memory for the stack current\n");

		
		current->string = item;
		current->next = *theStack;
		*theStack = current;
	}
	
	return 1;
}

//pop( )
//pops a string item from the stack and returns it to the caller
char* pop(stackItem** theStack)
{
	char* retStr;
	stackItem* current;
	
	if(*theStack == NULL)
		return NULL;
	
	retStr = (*theStack)->string;
	current = *theStack;
	*theStack = (*theStack)->next;
	free(current);
	
	return retStr;
}

//parseInput -	parses user input and changes 
//				console mode according to ANSI
//				escape sequence codes
//
//An ANSI command is valid if:
//	1. String starts with an ANSI escape sequence "ESC["
//	2. Intermediate argument tokens terminate with ';' and contain numbers
//	3. Last argument terminates with a command
//	4. Command Types:
//		A: graphics mode defined and 
//		   command modifier is graphics mode set or reset
//	 	B: graphics mode not defined and
//		   command modifier is valid
//
//For all other strings parseInput writes to output (if bEcho is on)
//
int parseInput(HANDLE hOutput, char input, stackItem** theStack)
{
	static int savedIndex;
	static char savedNumber[MAX_ARG_LENGTH];
	
	static char  bEscSet = 0;
	static char  bEscSeqStart = 0;
	static char isGraphicsMod = 0;
	static char isEscSequence = 0;
	
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	
	COORD coord = { 0 , 0 };
	
	int pushIsGood;

	DWORD cRead;
	DWORD cWritten;
	DWORD cToWrite;
		
	char *c;
		
	if(isEscSequence)
	{
		if(bEscSeqStart)
		{
			if(input == DISPLAY_MODIFY)
			{
				isGraphicsMod = 1;
				bEscSeqStart = 0;
				return 0;
			}
			else
			{
				isGraphicsMod = 0;
				bEscSeqStart = 0;
			}
		}
		
		if(input >= '0' && input <= '9')
		{
			if(savedIndex < MAX_ARG_LENGTH)
			{
				savedNumber[savedIndex] = input;
				savedIndex++;
			}
			else
			{
				savedIndex = 0;
				isEscSequence = 0;
			}
		}
		else if(input == ';')
		{
			if(savedIndex > 0)
			{
				int i;				
				c = malloc(sizeof(char)*(savedIndex+1));
				
				if(c == NULL)
					exitWithErr("Error: could not allocate memory for substr\n");
					
				for(i = 0; i < savedIndex; i++)
					c[i] = savedNumber[i];
				
				c[i] = '\0';
				savedIndex = 0;
				pushIsGood = push(theStack, c);
			}
			else
				isEscSequence = 0;
		}
		else if(isANSICommand(&input, isGraphicsMod))
		{
			if(savedIndex > 0)
			{
				int i;
				
				c = malloc(sizeof(char)*(savedIndex+1));
				
				for(i = 0; i < savedIndex; i++)
					c[i] = savedNumber[i];
				
				c[i] = '\0';
				savedIndex = 0;
				pushIsGood = push(theStack, c);
			}
			
			c = malloc(sizeof(char)*2);
			
			if(c == NULL)
				exitWithErr("Error: could not allocate memory for substr\n");
			
			c[0] = input;
			c[1] = '\0';
			
			pushIsGood = push(theStack, c);
			evaluateANSI(hOutput, theStack);
			isEscSequence = 0;
		}	//if - else block
		else
			isEscSequence = 0;
	} //end if isEscSequence
	else if(input == ESCAPE)
	{
		bEscSet = 1;
	}
	else if(input == '[' && bEscSet)
	{
		bEscSet = 0;
		isEscSequence = 1;
		bEscSeqStart = 1;
	}
	else
	{
		//For all else, echo input		
		if(bEchoOn)
		{
			if (! GetConsoleScreenBufferInfo(hOutput, &csbiInfo)) 
					exitWithErr("GetConsoleScreenBufferInfo");
					
			if(input == '\b')
			{						
				coord.Y = csbiInfo.dwCursorPosition.Y;
				coord.X = csbiInfo.dwCursorPosition.X - 1;
				
				ValidateAndUpdate(hOutput, &coord, bEolAutoWrap, csbiInfo);
				WriteConsole(hOutput, " ", 1, &cWritten, NULL);
				SetConsoleCursorPosition(hOutput, coord);
			}
			else if(input == '\t')
			{
				//write to the next tab stop
				cToWrite = (TAB_SIZE - (csbiInfo.dwCursorPosition.X & 
							(TAB_SIZE-1)));
				coord.Y = csbiInfo.dwCursorPosition.Y;
				coord.X = csbiInfo.dwCursorPosition.X + cToWrite;
				
				ValidateAndUpdate(hOutput, &coord, bEolAutoWrap, csbiInfo);
				FillConsoleOutputCharacter(hOutput, ' ', cToWrite,
							csbiInfo.dwCursorPosition, &cWritten);
				FillConsoleOutputAttribute(hOutput, csbiInfo.wAttributes, 
							cToWrite, csbiInfo.dwCursorPosition, &cWritten);
				SetConsoleCursorPosition(hOutput, coord);
			}
			else if(input == '\n' || input == '\r')
			{	
				cToWrite = 1 + csbiInfo.srWindow.Right - 
								csbiInfo.dwCursorPosition.X;
				coord.Y = csbiInfo.dwCursorPosition.Y + 1;
				coord.X = 0;
				
				if(csbiInfo.dwCursorPosition.X < csbiInfo.srWindow.Right)
				{
					FillConsoleOutputCharacter(hOutput, ' ', cToWrite,
								csbiInfo.dwCursorPosition, &cWritten);
				}

				FillConsoleOutputAttribute(hOutput, csbiInfo.wAttributes, 
								cToWrite, csbiInfo.dwCursorPosition, &cWritten);
				ValidateAndUpdate(hOutput, &coord, bEolAutoWrap, csbiInfo);
				SetConsoleCursorPosition(hOutput, coord);
			}
			//bell char goes here
			else if(input == BELL)
			{
				Beep(800, 100);
			}
			else if(input == DEL)		//deletes up to end of line
			{
				/*CHAR_INFO buf[256];
				int currentIndex;
				int maxIndex;
				COORD dwBufSize;
				SMALL_RECT srCopyRegion;
				
				maxIndex = 1 + csbiInfo.srWindow.Right - 
							csbiInfo.dwCursorPosition.X;
							
				buf[maxIndex].Char.AsciiChar = ' ';
				buf[maxIndex].Attributes = csbiInfo.wAttributes;
				
				coord.Y = 0;
				coord.X = 0;
				dwBufSize.Y = 1;
				dwBufSize.X = 255;
				
				srCopyRegion.Top = csbiInfo.dwCursorPosition.Y;
				srCopyRegion.Bottom = csbiInfo.dwCursorPosition.Y;
				srCopyRegion.Left = csbiInfo.dwCursorPosition.X;
				srCopyRegion.Right = csbiInfo.srWindow.Right;
				
				ReadConsoleOutput(hOutput, buf, dwBufSize, coord, &srCopyRegion);
				
				for(currentIndex = 0; currentIndex < maxIndex; currentIndex++)
				{
					buf[currentIndex].Char.AsciiChar = 
							buf[currentIndex+1].Char.AsciiChar;
					buf[currentIndex].Attributes = 
							buf[currentIndex+1].Attributes;
				}
				
				WriteConsoleOutput(hOutput, buf, dwBufSize, coord, &srCopyRegion);
				*/
				
				coord.Y = csbiInfo.dwCursorPosition.Y;
				coord.X = csbiInfo.dwCursorPosition.X;
				
				WriteConsole(hOutput, " ", 1, &cWritten, NULL);
				SetConsoleCursorPosition(hOutput, coord);
			}
			else if(input != 0)
			{		
				coord.Y = csbiInfo.dwCursorPosition.Y;
				coord.X = csbiInfo.dwCursorPosition.X + 1;
				
				WriteConsole(hOutput, &input, 1, 
												&cWritten, NULL);
				ValidateAndUpdate(hOutput, &coord, bEolAutoWrap, csbiInfo);
				SetConsoleCursorPosition(hOutput, coord);
			}
		}
	}
	
	return 0;
}

//The execution routine printAction 
//
//printAction looks at the command type and then determines 
//how many arguments to retrieve
//
//It does this by pulling the topmost arguments from the stack.
//This is kind of lazy and produces strange results if you're 
//expecting the commands you entered first to have priority.
//
//Commands are organized by type. 
//See the ANSI escape sequence definitions for reference
//
void evaluateANSI(HANDLE hOutput, stackItem** theStack)
{
	char *token = pop(theStack);
	
	int arg1;
	int arg2;
	
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	COORD coord;
	DWORD cToWrite;
	DWORD cWritten = 0;
	
	WORD wColorAttrs;
	
	if (! GetConsoleScreenBufferInfo(hOutput, &csbiInfo)) 
			exitWithErr("GetConsoleScreenBufferInfo");
	
	switch(*token)
	{	
		case CURSOR_MOVE:
			//printf("%s is set cursor position\n", token);
			
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg2 = atoi(token);
				free(token);
				token = pop(theStack);
				
				if(token != NULL && isNumber(token))
				{
					arg1 = atoi(token);
					free(token);
					
					//printf("Moving to yx coord (%d,%d)\n", arg1, arg2);
					
					coord.Y = csbiInfo.srWindow.Top + arg1;
					coord.X = csbiInfo.srWindow.Left + arg2;
					
					if(coord.Y >= csbiInfo.srWindow.Bottom)
						coord.Y = csbiInfo.srWindow.Bottom - 1;
						
					if(coord.X >= csbiInfo.srWindow.Right)
						coord.X = csbiInfo.srWindow.Right - 1;
						
					SetConsoleCursorPosition(hOutput, coord);
				}
			}
			
			break;
			
		case CURSOR_HOME:
			//printf("%s is cursor position with default\n", token);
			
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg2 = atoi(token);
				free(token);
				token = pop(theStack);
				
				if(token != NULL && isNumber(token))
				{
					arg1 = atoi(token);
					free(token);
					
					//printf("Moving to yx coord (%d,%d)\n", arg1, arg2);
					coord.Y = csbiInfo.srWindow.Top + arg1;
					coord.X = csbiInfo.srWindow.Left + arg2;
					
					if(coord.Y >= csbiInfo.srWindow.Bottom)
						coord.Y = csbiInfo.srWindow.Bottom - 1;
						
					if(coord.X >= csbiInfo.srWindow.Right)
						coord.X = csbiInfo.srWindow.Right - 1;
				}
			}
			else
			{
				//printf("Resetting to yx coord (0,0)\n");
				coord.Y = csbiInfo.srWindow.Top;
				coord.X = csbiInfo.srWindow.Left;
			}
	
			SetConsoleCursorPosition(hOutput, coord);
			
			break;
		
		//one arg commands
		
		case CURSOR_UP:
			//printf("%s is move cursor up\n", token);
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg1 = atoi(token);
				free(token);
				token = pop(theStack);
				
				//printf("Moving up %d lines\n", arg1);
				
				coord.Y = csbiInfo.dwCursorPosition.Y - arg1;
				coord.X = csbiInfo.dwCursorPosition.X;
				
				if(coord.Y < csbiInfo.srWindow.Top)
					coord.Y = csbiInfo.srWindow.Top;
				
				SetConsoleCursorPosition(hOutput, coord);
			}
			
			break;
			
		case CURSOR_DOWN:
			//printf("%s is move cursor down\n", token);
			
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg1 = atoi(token);
				free(token);
				token = pop(theStack);
				
				//printf("Moving down %d lines\n", arg1);
				
				coord.Y = csbiInfo.dwCursorPosition.Y + arg1;
				coord.X = csbiInfo.dwCursorPosition.X;
				
				if(coord.Y > csbiInfo.srWindow.Bottom)
					coord.Y = csbiInfo.srWindow.Bottom;
					
				SetConsoleCursorPosition(hOutput, coord);
			}
			
			break;
			
		case CURSOR_FORWARD:
			//printf("%s is move cursor forward\n", token);
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg1 = atoi(token);
				free(token);
				token = pop(theStack);
				
				//printf("Moving forward %d lines\n", arg1);
				
				coord.Y = csbiInfo.dwCursorPosition.Y;
				coord.X = csbiInfo.dwCursorPosition.X + arg1;
				
				if(coord.X >= csbiInfo.srWindow.Right)
					coord.X = csbiInfo.srWindow.Right;
				
				SetConsoleCursorPosition(hOutput, coord);
			}
			
			break;
			
		case CURSOR_BACKWARD:
			//printf("%s is move cursor backward\n", token);
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg1 = atoi(token);
				free(token);
				token = pop(theStack);
				
				//printf("Moving back %d lines\n", arg1);
				
				coord.Y = csbiInfo.dwCursorPosition.Y;
				coord.X = csbiInfo.dwCursorPosition.X - arg1;
				
				if(coord.X < csbiInfo.srWindow.Left)
					coord.X = csbiInfo.srWindow.Left;
				
				SetConsoleCursorPosition(hOutput, coord);
			}
			
			break;
			
		//no args
		
		case CURSOR_SAVE:
			//printf("%s is cursor save\n", token);
			dwSavedCursor.Y = csbiInfo.dwCursorPosition.Y;
			dwSavedCursor.X = csbiInfo.dwCursorPosition.X;
			break;
		case CURSOR_RESTORE	:
			//printf("%s is cursor restore\n", token);
			SetConsoleCursorPosition(hOutput, dwSavedCursor);
			break;

		case ERASE_DISPLAY:
			free(token);
			token = pop(theStack);
			
			if(token != NULL && (token[1] == '\0') && *token == '2')
			{
				coord.X = csbiInfo.srWindow.Left;
				coord.Y = csbiInfo.srWindow.Top;
				cToWrite = (1 + (csbiInfo.srWindow.Bottom) 
							- (csbiInfo.srWindow.Top)) *
						  (1 + (csbiInfo.srWindow.Right) - 
						  (csbiInfo.srWindow.Left));
				
				FillConsoleOutputCharacter(hOutput, ' ', cToWrite, 
							coord, &cWritten);
				FillConsoleOutputAttribute(hOutput, csbiInfo.wAttributes,
							cToWrite, coord, &cWritten);
				SetConsoleCursorPosition(hOutput, coord);
			}
			break;
		case ERASE_LINE:
			//printf("%s is erase line\n", token);
			{
				coord.X = csbiInfo.dwCursorPosition.X;
				coord.Y = csbiInfo.dwCursorPosition.Y;
				cToWrite = 1 + csbiInfo.srWindow.Right - 
							csbiInfo.dwCursorPosition.X;
				
				FillConsoleOutputCharacter(hOutput, ' ', cToWrite, 
							coord, &cWritten);
				FillConsoleOutputAttribute(hOutput, csbiInfo.wAttributes,
							cToWrite, coord, &cWritten);
			}
			break;
		
		//variable args
		
		case CONSOLE_TEXT_MODE:
			//printf("%s is set console text attributes\n", token);

			wColorAttrs = csbiInfo.wAttributes;
			
			//mask out color flags but preserve intensity (bold) flags
			//see wincon.h for more info on color flags
			
			WORD bgMask = ~0x70;
			WORD fgMask = ~0x07;
			
			while(token != NULL)
			{
				if(isNumber(token))
				{
					int intval = atoi(token);
					switch(intval)
					{
						case ATTR_OFF:
							//printf("%d is set text attributes off\n", intval);
							wColorAttrs &= ~(FOREGROUND_INTENSITY |
											 BACKGROUND_INTENSITY) ;			 
							bEchoOn = 1;
							break;
						case ATTR_BOLD:
							//printf("%d is set text to bold\n", intval);
							wColorAttrs |= (FOREGROUND_INTENSITY |
											BACKGROUND_INTENSITY);
							break;
						case ATTR_UNDERSCORE:
							//printf("%d is set text to underscore\n", intval);
							break;
						case ATTR_BLINK:
							//printf("%d is set text to blink\n", intval);
							break;
						case ATTR_REVERSE:
							//printf("%d is reverse foreground and background colors\n", intval);
							wColorAttrs = (wColorAttrs & ~0xFF) |
										  ((wColorAttrs & 0xFF) >> 4) |
										  ((wColorAttrs & 0xFF) << 4);
							break;
						case ATTR_CONCEALED:
							//printf("%d is set text to hidden\n", intval);
							bEchoOn = 0;
					
							break;
						case COLOR_FG_BLACK	:
							//printf("%d is set text foreground to black\n", intval);
							wColorAttrs = (wColorAttrs & fgMask);
							break;
						case COLOR_FG_RED:
							//printf("%d is set text foreground to red\n", intval);
							wColorAttrs = (wColorAttrs & fgMask) |
										   FOREGROUND_RED;
							break;
						case COLOR_FG_GREEN:
							//printf("%d is set text foreground to green\n", intval);
							wColorAttrs = (wColorAttrs & fgMask) |
											FOREGROUND_GREEN;
							break;
						case COLOR_FG_YELLOW:
							//printf("%d is set text foreground to yellow\n", intval);
							wColorAttrs = (wColorAttrs & fgMask) |
											FOREGROUND_RED |
											FOREGROUND_GREEN;
							break;
						case COLOR_FG_BLUE:
							//printf("%d is set text foreground to blue\n", intval);
							wColorAttrs = (wColorAttrs & fgMask) |
											FOREGROUND_BLUE;
							break;
						case COLOR_FG_MAGENTA:
							//printf("%d is set text foreground to magenta\n", intval);
							wColorAttrs = (wColorAttrs & fgMask) |
											FOREGROUND_RED |
										   FOREGROUND_BLUE; 
							break;
						case COLOR_FG_CYAN:
							//printf("%d is set text foreground to cyan\n", intval);
							wColorAttrs = (wColorAttrs & fgMask)|
											FOREGROUND_GREEN |
											FOREGROUND_BLUE;
							break;
						case COLOR_FG_WHITE:
							//printf("%d is set text foreground to white\n", intval);
							wColorAttrs |= FOREGROUND_RED |
											FOREGROUND_GREEN |
											FOREGROUND_BLUE;
							break;
						case COLOR_BG_BLACK	:
							//printf("%d is set text background to black\n", intval);
							wColorAttrs = (wColorAttrs & bgMask);
							break;
						case COLOR_BG_RED:
							//printf("%d is set text background to red\n", intval);
							wColorAttrs = (wColorAttrs & bgMask) |
										   BACKGROUND_RED;
							break;
						case COLOR_BG_GREEN:
							//printf("%d is set text background to green\n", intval);
							wColorAttrs = (wColorAttrs & bgMask) |
											BACKGROUND_GREEN;
							break;
						case COLOR_BG_YELLOW:
							//printf("%d is set text background to yellow\n", intval);
							wColorAttrs = (wColorAttrs & bgMask) |
											BACKGROUND_RED |
											BACKGROUND_GREEN;
							break;
						case COLOR_BG_BLUE:
							//printf("%d is set text background to blue\n", intval);
							wColorAttrs = (wColorAttrs & bgMask) |
											BACKGROUND_BLUE;
							break;
						case COLOR_BG_MAGENTA:
							//printf("%d is set text background to magenta\n", intval);
							wColorAttrs = (wColorAttrs & bgMask) |
											BACKGROUND_RED |
										   BACKGROUND_BLUE;
							break;
						case COLOR_BG_CYAN:
							//printf("%d is set text background to cyan\n", intval);
							wColorAttrs = (wColorAttrs & bgMask)|
											BACKGROUND_GREEN |
											BACKGROUND_BLUE;
							break;
						case COLOR_BG_WHITE:
							//printf("%d is set text background to white\n", intval);
							wColorAttrs |= BACKGROUND_RED |
											BACKGROUND_GREEN |
											BACKGROUND_BLUE;
							break;
						default:
							//printf("%d is invalid input\n", intval);
							break;
					} // switch
				}
				
				free(token);
				token = pop(theStack);
			} //while loop
			
			//set text
			if (! SetConsoleTextAttribute(hOutput, wColorAttrs)) 
				exitWithErr("SetConsoleMode");
				
			//if(!FillConsoleOutputAttribute(hOutput,wColorAttrs, 80, coord, &cWritten))
			//	exitWithErr("FillConsoleOutputAttribute");
				
			break;
			
		//one arg commands
		
		case CONSOLE_GRAPHICS_MODE:
			//printf("%s is set console graphics mode\n", token);
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg1 = atoi(token);
				free(token);
				token = pop(theStack);
				
				if(arg1 == GFX_LINE_WRAP)
				{
					//printf("Enabling line wrap mode\n", arg1);
					bEolAutoWrap = 1;				
				}
			}
			break;
			
		case CONSOLE_GRAPHICS_RESET:
			//printf("%s is reset console graphics mode\n", token);
			free(token);
			token = pop(theStack);
			
			if(token != NULL && isNumber(token))
			{
				arg1 = atoi(token);
				free(token);
				token = pop(theStack);
				
				if(arg1 == GFX_LINE_WRAP)
				{
					//printf("Disabling line wrap mode\n", arg1);
					bEolAutoWrap = 0;					
				}
			}
			
			break;
			
		default:
			//printf("%s is display erase\n", token);
				
			break;
	}
	
	//clean up unused parameters
	emptyStack(theStack);
}

int isANSICommand(char* str, char bIsGraphicMod)
{
	if(bIsGraphicMod)
	{
		switch (*str) 
		{
			case CONSOLE_GRAPHICS_MODE:
			case CONSOLE_GRAPHICS_RESET:
				return 1;
			default:
				break;
		}
	}
	else switch (*str) 
	{
		case CURSOR_MOVE:
		case CURSOR_HOME:
		case CURSOR_UP:
		case CURSOR_DOWN:
		case CURSOR_FORWARD:
		case CURSOR_BACKWARD:
		case CURSOR_SAVE:
		case CURSOR_RESTORE	:
		case ERASE_DISPLAY:
		case ERASE_LINE:
		case CONSOLE_TEXT_MODE:
			return 1;
		default:
			break;
	}
	
	return 0;
}

int isNumber(char *str)
{
	if(*str == '\0')
		return 0;
		
	while(*str != '\0')
	{
		if(*str < '0' || *str > '9')
			return 0;
		
		str++;
	}
	
	return 1;
}

//ValidateAndUpdate(HANDLE, PCOORD, BOOL)
//
//Ensures that the screen cursor gets updated correctly
//in raw mode
//
//Case 1: Left side - move up one line and put cursor on right side
//		Subcases:
//		a: Top of window reached. Move window up one line
//  	b: Top of buffer reached. Move cursor to home (0,0)
//Case 2: Right side - move down one line and put cursor on left side
//		Subcases:
//		a: Bottom of window reached. Move window down one line
//		b: End of Buffer reached. Scroll buffer up one line.
//Case 3: Returns - move down one line and put cursor on left side
//		Subcases:
//		a: Bottom of window reached. Move window down one line
//		b: End of Buffer reached. Scroll buffer up one line.
//
//Precondition: csbiInfo recently updated
//				hOutput is a valid screen handle and read-enabled
//Postcondition:dwNewCursor updated
//
void ValidateAndUpdate(
	HANDLE hOutput, 
	PCOORD dwNewCursor, 
	BOOL bLineWrap,
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo)
{
	//if (!GetConsoleScreenBufferInfo(hOutput, &csbiInfo))
	//	exitWithErr("GetConsoleScreenBufferInfo"); 
	
	if(dwNewCursor->X < csbiInfo.srWindow.Left)
	{
		if(bLineWrap)
		{
			dwNewCursor->Y--;
			dwNewCursor->X = csbiInfo.srWindow.Right;
			
			if(dwNewCursor->Y < csbiInfo.srWindow.Top)
			{
				if(dwNewCursor->Y < 0)
				{
					dwNewCursor->Y = 0;
					dwNewCursor->X = 0;
				}
				else
				{
					csbiInfo.srWindow.Top--;
					csbiInfo.srWindow.Bottom--;
				
					if (! SetConsoleWindowInfo(hOutput, 1, 
						&csbiInfo.srWindow)) 
						exitWithErr("SetConsoleWindow");
				}
			}
		}
		else
			dwNewCursor->X = csbiInfo.srWindow.Left;
	}
	else if(dwNewCursor->X > csbiInfo.srWindow.Right)
	{
		if(bLineWrap)
		{
			dwNewCursor->Y++;
			dwNewCursor->X = 0;
		}
		else
			dwNewCursor->X = csbiInfo.srWindow.Right;
	}
	
	if(dwNewCursor->Y > csbiInfo.srWindow.Bottom)
	{
		if(dwNewCursor->Y < csbiInfo.dwSize.Y)
		{
			csbiInfo.srWindow.Top = dwNewCursor->Y - 
				(csbiInfo.srWindow.Bottom - csbiInfo.srWindow.Top);
			csbiInfo.srWindow.Bottom = dwNewCursor->Y;
		
			if (! SetConsoleWindowInfo(hOutput, 1, 
					&csbiInfo.srWindow)) 
				exitWithErr("SetConsoleWindow");
		}
		else
		{
			ScrollScreenBuffer(hOutput, csbiInfo);
			dwNewCursor->Y = csbiInfo.dwSize.Y - 1;
		}
	
	}
}

VOID ScrollScreenBuffer(HANDLE htToPrint, CONSOLE_SCREEN_BUFFER_INFO csbiInfo)
{
	BOOL fSuccess;
	SMALL_RECT srctScrollRect, srctClipRect; 
	CHAR_INFO chiFill; 
	COORD coordDest; 
	  
	//if (!GetConsoleScreenBufferInfo(htToPrint, &csbiInfo))
	//	exitWithErr("GetConsoleScreenBufferInfo"); 
	 
	srctScrollRect.Top = 0; 
	srctScrollRect.Bottom = csbiInfo.dwSize.Y - 1; 
	srctScrollRect.Left = 0; 
	srctScrollRect.Right = csbiInfo.dwSize.X - 1; 
	 
	coordDest.X = 0; 
	coordDest.Y = -1; 
	
	srctClipRect = srctScrollRect; 
	 
	chiFill.Attributes = csbiInfo.wAttributes; 
	chiFill.Char.AsciiChar = ' '; 
	 
	fSuccess = ScrollConsoleScreenBuffer( 
		htToPrint,
		&srctScrollRect,
		&srctClipRect,
		coordDest,
		&chiFill);
		
	if(!fSuccess)
		exitWithErr("ScrollConsoleScreenBuffer");
}

int InitConsole(PHANDLE hIn, PHANDLE hOut)
{
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	DWORD fdwInputMode;
	DWORD fdwOutputMode;
	HANDLE hStdout;
	HANDLE hStdin;
	
	bEchoOn = 1;
	bEolAutoWrap = 1;
	
	dwSavedCursor.X = 0;
	dwSavedCursor.Y = 0;
	
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	hStdin = GetStdHandle(STD_INPUT_HANDLE);
	
	*hOut = CreateConsoleScreenBuffer(
		(GENERIC_READ | GENERIC_WRITE), 
		(FILE_SHARE_READ | FILE_SHARE_WRITE),
		NULL,
		CONSOLE_TEXTMODE_BUFFER, 
		NULL);
	*hIn = hStdin;
	
	if(hIn == NULL || hOut == NULL)
		return 0;
	if(*hOut == INVALID_HANDLE_VALUE)
		return 0;
		
	if (hStdin == INVALID_HANDLE_VALUE 
		|| hStdout == INVALID_HANDLE_VALUE)
		exitWithErr("GetStdHandle ");
	if (!GetConsoleMode(hStdin, &fdwOldInputMode)) 
			exitWithErr("GetConsoleMode ");
	if(!GetConsoleMode(hStdout, &fdwOldOutputMode))
			exitWithErr("GetConsoleMode ");	
	if (! GetConsoleScreenBufferInfo(hStdout, &csbiInfo)) 
			exitWithErr("GetConsoleScreenBufferInfo ");
	
	wOldColorAttrs = csbiInfo.wAttributes;
	
	csbiInfo.dwCursorPosition.X = 0;
	csbiInfo.dwCursorPosition.Y = 0;
	csbiInfo.srWindow.Top = csbiInfo.srWindow.Top - csbiInfo.srWindow.Bottom;
	csbiInfo.srWindow.Bottom = 0 ;
	csbiInfo.srWindow.Right = csbiInfo.srWindow.Right - csbiInfo.srWindow.Left;
	csbiInfo.srWindow.Left = 0;

	fdwInputMode = fdwOldInputMode & ~(ENABLE_LINE_INPUT | 
					ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT |
					ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);		
	fdwOutputMode = fdwOldOutputMode & ~(ENABLE_PROCESSED_OUTPUT |
									ENABLE_WRAP_AT_EOL_OUTPUT);
	
	if (!SetConsoleMode(hStdin, fdwInputMode)) 
		exitWithErr("SetConsoleMode ");
	if(!SetConsoleMode(*hOut, fdwOutputMode))
		exitWithErr("SetConsoleMode ");
	if(!SetConsoleScreenBufferSize(*hOut, csbiInfo.dwSize))
		exitWithErr("SetConsoleScreenBufferSize ");
	if(!SetConsoleWindowInfo(*hOut, 1, &csbiInfo.srWindow))
		exitWithErr("SetConsoleWindowInfo ");
	if(!SetConsoleCursorPosition(*hOut, csbiInfo.dwCursorPosition))
		exitWithErr("SetConsoleCursorPosition ");
	if(!SetConsoleActiveScreenBuffer(*hOut))
		exitWithErr("SetConsoleActiveScreenBuffer ");
		
	return 1;
}

int ResetConsole( )
{
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	
	if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) 
		return 0;
	if (!SetConsoleTextAttribute(hStdout, wOldColorAttrs)) 
		return 0;
	if(!SetConsoleMode(hStdin, fdwOldInputMode))
		return 0;
	if(!SetConsoleMode(hStdout, fdwOldOutputMode))
		return 0;
		
	return 1;
}

int KeyEventProc(KEY_EVENT_RECORD kerEvent, 
	stackItem **theStack, 
	HANDLE hOutput)
{
	int c = -1;
	
	switch(kerEvent.wVirtualKeyCode)
	{		
		case VK_CAPITAL:
		case VK_CONTROL:
		case VK_SHIFT:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_END:
		case VK_HOME:
		case VK_LEFT:
		case VK_RIGHT:
		case VK_UP:
		case VK_DOWN:
			break;
		
		case VK_DELETE:
			if(kerEvent.bKeyDown)
			{
				c = DEL;
				parseInput(hOutput, c, theStack);
			}
			break;
			
		default:
			c = kerEvent.uChar.AsciiChar;
			if(c == END_OF_TEXT)
			{
				emptyStack(theStack);
				SetConsoleTextAttribute(hOutput, wOldColorAttrs);
				return -2;
			}
			else if(kerEvent.bKeyDown)
				parseInput(hOutput, (char)c, theStack);
				
			break;
	}
	
	return c;
}

void exitWithErr(char *str)
{
	HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE); 
	
	LPVOID lpMsgBuf;
	DWORD cWritten;
	DWORD errorCode;
	
	//ResetConsole( );
	
	if (hStderr == INVALID_HANDLE_VALUE)
		exit(1);
	
	errorCode = GetLastError( );
	
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);
	
	WriteConsole(hStderr, str, lstrlen(str), &cWritten, NULL);
	WriteConsole(hStderr, " - ", 3, &cWritten, NULL);
    WriteConsole(hStderr, lpMsgBuf, lstrlen(lpMsgBuf), &cWritten, NULL);
	exit(1);
}