include include.mk

library: ${OBJECTS} mcbsp.opp
	mkdir include || true
	mkdir lib || true
	cp mcbsp.h include/
	cp mcbsp.hpp include/
	ar rcs lib/libmcbsp${VERSION}.a ${OBJECTS} mcbsp.opp

compat:
	rm -f ${OBJECTS}
	make CFLAGS="${CFLAGS} -DMCBSP_COMPATIBILITY_MODE" ${OBJECTS} tests
	mkdir include || true
	mkdir lib || true
	cp mcbsp.h include/
	ar rcs lib/compat-libmcbsp${VERSION}.a ${OBJECTS}
	rm -f ${OBJECTS}
	rm -f tests/internal.o tests/util.o tests/bsp.o tests/abort.o tests/test_abort.out tests/spmd.o

clean:
	rm -f ${OBJECTS} mcbsp.opp
	rm -f test_internal    test_util    test_bsp    test_abort    tests/test_abort.out    test_spmd
	rm -rf include
	rm -rf lib

tests: tests/internal.o tests/util.o mcinternal.o mcutil.o tests/bsp.o mcbsp.o tests/abort.o tests/spmd.o
	${CC} ${CFLAGS} -o test_internal tests/internal.o mcinternal.o mcutil.o mcbsp.o `./deplibs.sh`
	${CC} ${CFLAGS} -o test_util     tests/util.o     mcinternal.o mcutil.o mcbsp.o `./deplibs.sh`
	${CC} ${CFLAGS} -o test_bsp      tests/bsp.o      mcinternal.o mcutil.o mcbsp.o `./deplibs.sh`
	${CC} ${CFLAGS} -o test_abort	 tests/abort.o	  mcinternal.o mcutil.o mcbsp.o `./deplibs.sh`
	${CC} ${CFLAGS} -o test_spmd     tests/spmd.o     mcinternal.o mcutil.o mcbsp.o `./deplibs.sh`
	rm -f tests/internal.o tests/util.o tests/bsp.o tests/abort.o tests/test_abort.out tests/spmd.o
	./test_internal
	./test_util > /dev/null; if [ "$$?" -ne "0" ]; then echo "SUCCESS"; else echo "FAILURE"; fi
	./test_bsp
	./test_abort 2> tests/test_abort.out; diff tests/test_abort.out tests/test_abort.chk; if [ $$? -eq 0 ]; then echo "SUCCESS"; else echo "FAILURE"; fi
	./test_spmd
