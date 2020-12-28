CC = gcc
CFLAGS = -std=gnu99 -Wall
LDLIBS = -lpthread -lm
all: numf

numf: numf.o indexing.o input.o other_functions.o
	${CC} ${CFLAGS} -o numf numf.o indexing.o input.o other_functions.o ${LDLIBS}

numf.o: numf.c input.h indexing.h 
	${CC} ${CFLAGS} -o numf.o -c numf.c ${LDLIBS}

indexing.o: indexing.c indexing.h 
	${CC} ${CFLAGS} -o indexing.o -c indexing.c ${LDLIBS}

input.o: input.c input.h 
	${CC} ${CFLAGS} -o input.o -c input.c ${LDLIBS}

other_functions.o: other_functions.c other_functions.h
	${CC} ${CFLAGS} -o other_functions.o -c other_functions.c ${LDLIBS}

.PHONY: all clean
clean:
	-rm ${PROG}