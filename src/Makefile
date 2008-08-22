MODS = gtk+-2.0 libg3d
INCS = `pkg-config ${MODS} --cflags`
LIBS = `pkg-config ${MODS} --libs` -lm
OBJS = main.o gui.o v4l2.o gray.o ac3d.o config.o
BIN = 3dscan
#CC = gcc
CFLAGS = -Wall -ansi -pedantic ${INCS}

all: ${BIN}

.c.o:
	${CC} -c ${CFLAGS} -o $@ $<

${BIN}: ${OBJS}
	${CC} -o $@ ${OBJS} ${LIBS}

clean:
	rm -f ${OBJS} ${BIN}
