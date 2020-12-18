PROG = numf
CC=gcc
CFLAGS= -std=gnu99 -Wall
LDLIBS = -lpthread -lm
all: ${PROG}

${PROG}: ${PROG}.c
	${CC} ${CFLAGS} -o ${PROG} ${PROG}.c ${LDLIBS}

.PHONY: all clean
clean:
	-rm ${PROG}