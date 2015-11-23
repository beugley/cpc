
CC = /usr/bin/cc

EXE = cpc
SRCS = cpc.c
OBJS = ${SRCS:.c=.o}

CFLAGS = -I.
LDFLAGS =
LIBS =

all: ${EXE}

clean:
	rm -f ${EXE} ${OBJS}

${EXE}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LDFLAGS} ${LIBS}


.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) $(DFLAGS) -c $<

