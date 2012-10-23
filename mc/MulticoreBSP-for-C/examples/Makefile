include ../include.mk

INCLUDE_DIR = ../include/
LFLAGS = ../lib/libmcbsp${VERSION}.a -lm ${LINKTO}

CFLAGS := ${CFLAGS} -I${INCLUDE_DIR}
CPPFLAGS := ${CPPFLAGS} -I${INCLUDE_DIR}

all: ../lib/libmcbsp${VERSION}.a multistart parallel_loop hello_world

../lib/libmcbsp${VERSION}.a:
	(cd ..; make)

multistart: multistart.o
	${CC} ${CFLAGS} -o $@ $^ ${LFLAGS} `../deplibs.sh`

parallel_loop: parallel_loop.o
	${CC} ${CFLAGS} -o $@ $^ ${LFLAGS} `../deplibs.sh`

hello_world: hello_world.opp
	${CPP} ${CPPFLAGS} -o $@ $^ ${LFLAGS} `../deplibs.sh`

clean:
	rm -f multistart.o parallel_loop.o hello_world.opp multistart parallel_loop hello_world

