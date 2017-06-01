#ifndef KLUDGE_H
#define KLUDGE_H

//possible kludge execution errors

#define	AddrOutOfRange	0
#define EndOfProgram 	1
#define IllegalInstr 	2
#define IllegalAddrMode 3
#define IllegalOperand	4
#define IllegalAddrRead 5
#define EmptyMemSection 6
#define BadInputLine	7
#define SignalTerminate 8

//kludge addressing modes

#define DispAddrMode	0
#define ImmAddrMode		2
#define IndrAddrMode	4
#define	IndxAddrMode	8

//kludge instruction codes

#define KludgeIllegal	0
#define KludgeMoveAC	1
#define KludgeMoveMem	2
#define KludgeMoveIR	3
#define KludgeJump		4
#define KludgeJNotZero	5
#define KludgeAdd		6
#define	KludgeShLeft	7
#define KludgeHalt		8
#define KludgeIn		9
#define KludgeOut		10

//Kludge miscellanies

#define ProgBufSize		65536
#define NumberOfInstr	9
#define LineSize		1024
#define MaxPrintMem		100
#define EndCodeSection	0x99999		//parseFile reads in ascii codes
									//as hex digits

int a2h(char* str);

void HaltWithErr(int errNum, unsigned int address);
void PrintMemory(int *progBuf, int *memBuf, int bufSize);
void ParseFile(FILE* theFile, int memLoc[], int *memSize, 
				unsigned int ProgramBuf[]);

#endif