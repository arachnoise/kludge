//kludge.c version 1.2 - now has a console!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ansiconsole.h"
#include "kludge.h"

int main(int argc, char* argv[])
{
	//kludge components
	
	unsigned int ProgramBuf[ProgBufSize];
	unsigned int instructionPtr;
	unsigned int accumulator;
	unsigned int indexReg;
	unsigned int immOperand;
	
	//console variables
	
	DWORD cRead;
	DWORD cWritten;
	LPSTR lpszKludgeStart = "The KLUDGE machine has entered execution ";
	HANDLE hInput;
	HANDLE hOutput;
	INPUT_RECORD irInRecords[IR_BUF_SIZE];
	
	stackItem **theStack;
	
	//runtime variables
	
	char exitFlag;
	char debugFlag;
	int printableMem[MaxPrintMem];
	int memSize;
	int i;
	
	//don't debug unless asked to do so
	debugFlag = 0;
	
	for(i = 0; i < ProgBufSize; i++)
		ProgramBuf[i] = 0;
	
	//load one program from command line
	
	if(argc >= 2)
	{
		FILE  *runFile;
		FILE  *displayFile;
		
		int c;					//for copying data to display file
		
		time_t theTime;			//for time printout
		struct tm* theDate;
		char* dateStr;
		
		runFile = fopen(argv[1], "r");
		
		if(runFile == NULL)
		{
			fprintf(stderr, "Error, couldn't open %s for execution\n", argv[1]);
			exit(1);
		}
		
		ParseFile(runFile, printableMem, &memSize, ProgramBuf);
		printf("File translation successful. Creating MEMORY.DISPLAY file.\n\n");
		rewind(runFile);
		displayFile = fopen("MEMORY.DISPLAY", "w");
		
		if(displayFile == NULL)
		{
			fprintf(stderr, "Error, couldn't open Memory.Display\n");
			exit(1);
		}
		
		time(&theTime);
		theDate = localtime(&theTime);
		dateStr = asctime(theDate);
		fprintf(displayFile, "\n\n\t\t\t\t\t\tMEMORY.DISPLAY\n\n");
		fprintf(displayFile, "\t\tDate and time of run: %s\n\n", dateStr);
		
		while((c=fgetc(runFile)) != EOF)
			fputc(c, displayFile);
		
		fclose(displayFile);
		fclose(runFile);
	}	//endif
	else
	{
		printf("Usage: %s <filename>\n", argv[0]);
		exit(1);
	}
	
	if(argc > 2)		//"-d" only option so far
	{
		if(strcmp(argv[2], "-d"))
		{
			printf("%s is an invalid option. Valid options are:\n");
			printf("-d:	debug enable\n");
		}
		else
		{
			debugFlag = 1;
		}
	}
	
	printf("%s\n\n", lpszKludgeStart);
	
	theStack = (stackItem**)malloc(sizeof(stackItem*));
	
	if(theStack == NULL)
	{
		fprintf(stderr, "Allocation for pointer to stack failed\n");
		exit(1);
	}
	
	*theStack = NULL;
	
	if(!InitConsole(&hInput, &hOutput))
	{
		fprintf(stderr, "Console creation failed\n");
		exit(1);
	}
	
	WriteConsole(hOutput, lpszKludgeStart, 
		lstrlen(lpszKludgeStart),
		&cWritten, 
		NULL);

	instructionPtr = 0;
	accumulator = 0;
	indexReg = 0;
	exitFlag = 0;
	
	while((instructionPtr < ProgBufSize) && !exitFlag)
	{
		int* opPtr;
		
		unsigned int opCode;
		unsigned char instrByte;
		unsigned char addrByte;
		unsigned short opndWord;
	
		unsigned int temp;
		
		//load and decode 4 byte opcode
		//instruction code is 4th byte
		//address is HO word of third byte
		//Operand is 1st and 2nd byte
		
		opCode = ProgramBuf[instructionPtr];
		instrByte = (opCode & 0xFF000000) >> 24;
		addrByte = (opCode & 0x00FF0000) >> 20;
		opndWord = (short)opCode;	
		
		//compute addressing mode and fetch operand
		
		switch(addrByte)
		{
			case DispAddrMode:
				opPtr = ProgramBuf + opndWord;
				break;
			case ImmAddrMode:
				if(instrByte == KludgeMoveMem)		//Can't write to immediate operand!
					HaltWithErr(IllegalOperand, instructionPtr);
				
				if(opndWord & 0x8000) 				//opndWord isn't signed for addressing purposes
					immOperand = opndWord | 0xFFFF0000;		//but in immediate mode it needs to be
				else								//if it's a negative number (HO bit = 1) then sign extend
					immOperand = opndWord;
				
				opPtr = &immOperand;			
				break;
			case IndrAddrMode:
				temp = ProgramBuf[opndWord];		//treat temp as address
			
				if(temp > ProgBufSize)
					HaltWithErr(AddrOutOfRange, instructionPtr);
				
				opPtr = ProgramBuf+temp;
				break;
			case IndxAddrMode:
				temp = indexReg + opndWord;
			
				if(temp > ProgBufSize)
					HaltWithErr(AddrOutOfRange, instructionPtr);
						
				opPtr = ProgramBuf+temp;				
				break;
			
			default:
				HaltWithErr(IllegalAddrMode, instructionPtr);
				break;
		}
		
		//execute instruction
		if(debugFlag)
		{
			printf("\tBeginning at:\t\t\t%.8X\n", instructionPtr);
			printf("\t\tOpcode is:\t\t\t%.8X\n",	ProgramBuf[instructionPtr]);
			printf("\t\tAccumulator:\t\t%.8X\n", accumulator);
			printf("\t\tIndexReg:\t\t\t%.8X\n", indexReg);
			
			if(addrByte != ImmAddrMode &&
				instrByte != KludgeJump &&
				instrByte != KludgeJNotZero)
			{
				printf("\t\tLoading address:\t%.8X\n", temp);
				printf("\t\tLoading value:\t\t%.8X\n\n",	ProgramBuf[temp]);
			}
			else
				printf("\t\tLoading value:\t\t%.8X\n\n", immOperand);
				
		}
		
		instructionPtr++;
		
		switch(instrByte){
			case KludgeMoveAC:
				accumulator = *opPtr;
				break;
			case KludgeMoveMem:
				*opPtr = accumulator;
				break;
			case KludgeMoveIR:
				indexReg = *opPtr;
				break;
			case KludgeJump:
				instructionPtr = opndWord;
				break;
			case KludgeJNotZero:
				if(accumulator != 0)
					instructionPtr = opndWord;
				break;
			case KludgeAdd:
				accumulator += *opPtr;
				break;
			case KludgeShLeft:
				if(!(*opPtr & 0x80000000))
					accumulator <<= *opPtr;
				break;
			case KludgeHalt:
				exitFlag = 1;
				break;
			case KludgeIn:
			{	
				int i;
				int temp;
				int bKeyFound;
				
				bKeyFound = 0;
				
				while(!bKeyFound)
				{
					if(!ReadConsoleInput(hInput, irInRecords, 1, &cRead))
						exitWithErr("ReadConsoleInput");
						
					switch(irInRecords[0].EventType)
					{		
						case KEY_EVENT:
							if(irInRecords[0].Event.KeyEvent.bKeyDown)
							{
								temp = KeyEventProc(irInRecords[0].Event.KeyEvent,
									theStack, 
									hOutput);
								
								if(temp > 0)
								{
									bKeyFound = 1;
									accumulator = 0xFF & temp;
									//printf("Accumulator is %d\n", accumulator);
								}
								else if(temp == -2)
								{
									PrintMemory(ProgramBuf, printableMem, memSize);
									HaltWithErr(SignalTerminate, instructionPtr);
								}
							}
							break;
						case MOUSE_EVENT:
							break;
						case WINDOW_BUFFER_SIZE_EVENT:
							break;
						case FOCUS_EVENT:
							break;
						case MENU_EVENT:
							break;
						
						default:
							exitWithErr("Unknown event type detected!");
					}
				}
			}
				break;
			case KludgeOut:
			{
				char c = accumulator >> 24;		//get HO byte
				parseInput(hOutput, c, theStack);
			}
				break;
			
			default:
				HaltWithErr(IllegalInstr, instructionPtr);
		}
	} //end while loop
	
	if(!exitFlag)
		HaltWithErr(EndOfProgram, instructionPtr);
	
	printf("Your KLUDGE program terminated normally.\n\n");
	
	//Display list of memory locations
	
	PrintMemory(ProgramBuf, printableMem, memSize);
	
	return 0;
}

//read n byte ascii string as 4 byte hex value
//does not check for overflow
int a2h(char *str)
{
	int retval = 0;
		
	while(*str != 0)
	{
		retval <<= 4;
		
		if(*str >= '0' && *str <= '9')
			retval += *str - '0';
			
		else if(tolower(*str) >= 'a' && tolower(*str) <= 'f')	
			retval += tolower(*str) - 'a' + 10;
			
		else
			return 0;
		
		str++;
	}
	
	return retval;
}

//TODO: give more informative error messages
void HaltWithErr(int errNum, unsigned int address)
{	
	static char *errMessage[] =  {
		"Error at address $%X - Operand address out of range.\n",
		"Error at address $%X - End of program reached before HALT instruction.\n",
		"Error at address $%X - Illegal instruction.\n",
		"Error at address $%X - Illegal addressing mode.\n",
		"Error at address $%X - Can't store data at immediate operand.\n",
		"Error at line %d - Illegal value in kludge file address field.\n",
	    "Error at line %d - Declared memory print section is empty\n",
		"Error at line %d - Bad line in kludge file.\n",
		"Error at line %d - Termination Signal received.\n"};
					 
	fprintf(stderr, errMessage[errNum], address);
	
	exit(1);
}

void PrintMemory(int *progBuf, int *memBuf, int bufSize)
{
	int i;
	
	for(i = 0; i < bufSize; i++)
	{
		int memslot = progBuf[memBuf[i]];
		printf("Memory location %.4X in hex is %.8X and in decimal:  %d\n",
				memBuf[i], memslot, memslot);		
	}
}

//ParseFile
//
//This routine takes a kludge file, creates an executable core
//and stores it in the program codebank.
//Afterwards,it saves a list of printable memory locations
//into memloc variable
//
//Kludge runtime files are tab delimited
//	Code and Data Sections
//	Field 1		Field 2		Field 3
//	Address		Code		Comment
//
//	Memory printout section
//	99999
//	Mem1	Mem2	Mem3	etc...
void ParseFile(FILE* theFile, int memLoc[], int *memSize, 
				unsigned int ProgramBuf[])
{
	char theLine[LineSize];
	char memFlag;
	char *tok;
	char delim[] = " \t\n";
	unsigned int addrToRead;
	unsigned int instrToRead;
	int line;
	
	*memSize = 0;
	memFlag = 0;
	addrToRead = 0;
	instrToRead = 0;
	line = 1;
	
	while(fgets(theLine, LineSize, theFile) && !memFlag)
	{
		if(theLine[0] == '\n')
			continue;
			
		//1st token should be valid address
		tok = strtok(theLine, delim);
				
		if(tok == NULL)
			HaltWithErr(BadInputLine, line);
		
		addrToRead = a2h(tok);
		
		if(addrToRead == EndCodeSection)
		{
			memFlag = 1;
			line++;
			continue;
		}
		else if(addrToRead >= ProgBufSize)
		{
			HaltWithErr(IllegalAddrRead, line);
		}
			
		//check buffer to avoid overwriting data
		if(ProgramBuf[addrToRead] != 0)
			HaltWithErr(BadInputLine, line);

		//2nd is opcode
		tok = strtok(NULL, delim);
		
		if(tok == NULL)
		{
			HaltWithErr(BadInputLine, line);
		}
			
		instrToRead = a2h(tok);	 
		ProgramBuf[addrToRead] = instrToRead;
		
		//ignore comment field
		line++;
	}
	
	//special case: 99999 read
	//the line immediately following 
	//should be a list of memory locations
	
	if(memFlag)
	{
		int i = 0;
		
		tok = strtok(theLine, delim);
		
		if(tok == NULL)
			HaltWithErr(EmptyMemSection, line);
			
		while(tok != NULL && i < MaxPrintMem)
		{	
			memLoc[i] = a2h(tok);
			
			if(memLoc[i] >= ProgBufSize)
				HaltWithErr(IllegalAddrRead, line);
				
			tok = strtok(NULL, delim);
			i++;
			*memSize = *memSize + 1;
		}	
	}
}	//end ParseFile