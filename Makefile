# Kludge makefile

ifdef ComSpec
    RM = del /Q
        else ifeq ($(shell uname), Linux)
	    RM = rm -f
endif

CC = gcc
OPT_FLAGS = -Wall
DEBUG_FLAG =
OBJS = kludge.o ansiconsole.o
HEADERS = kludge.h ansiconsole.h

kludge: $(OBJS)
	$(CC) $(OPT_FLAGS) $(DEBUG_FLAG) -o $@ $(OBJS)

kludge_v1.2.o: kludge.c $(HEADERS)
	$(CC) $(OPT_FLAGS) $(DEBUG_FLAG) -c kludge.c
	
ansiconsole2.o: ansiconsole.c ansiconsole.h
	$(CC) $(OPT_FLAGS) $(DEBUG_FLAG) -c ansiconsole.c
	
.PHONY : clean
clean:
	$(RM) ${OBJS}
