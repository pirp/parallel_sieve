CC= gcc
CFLAGS= -03 -DMCBSP_COMPATIBILITY_MODE
LFLAGS= compat-libmcbsp1.0.1.a -pthread -lm 
OBJIP= bspinprod.o bspedupack.o
OBJBEN= bspbench.o bspedupack.o
OBJBENGET= bspbench_get.o bspedupack.o
OBJLU= bsplu_test.o bsplu.o bspedupack.o
OBJFFT= bspfft_test.o bspfft.o bspedupack.o
OBJMV= bspmv_test.o bspmv.o bspedupack.o

all: ip bench lu fft matvec

ip: $(OBJIP)
	$(CC) $(CFLAGS) -o ip $(OBJIP) $(LFLAGS)

bench: $(OBJBEN)
	$(CC) $(CFLAGS) -o bench $(OBJBEN) $(LFLAGS)
	$(CC) $(CFLAGS) -o bench_get $(OBJBENGET) $(LFLAGS)
lu: $(OBJLU)
	$(CC) $(CFLAGS) -o lu $(OBJLU) $(LFLAGS)

fft: $(OBJFFT)
	$(CC) $(CFLAGS) -o fft $(OBJFFT) $(LFLAGS)

matvec: $(OBJMV)
	$(CC) $(CFLAGS) -o matvec $(OBJMV) $(LFLAGS)

bspinprod.o: bspinprod.c bspedupack.h
	$(CC) $(CFLAGS) -c bspinprod.c

bspbench_get.o: bspbench_get bspedupack.h
	$(CC) $(CFLAGS) -c bspbench_get.c
bspbench.o: bspbench.c bspedupack.h
	$(CC) $(CFLAGS) -c bspbench.c

bsplu_test.o: bsplu_test.c bspedupack.h
	$(CC) $(CFLAGS) -c bsplu_test.c

bsplu.o: bsplu.c bspedupack.h
	$(CC) $(CFLAGS) -c bsplu.c

bspfft_test.o: bspfft_test.c bspedupack.h
	$(CC) $(CFLAGS) -c bspfft_test.c

bspfft.o: bspfft.c bspedupack.h
	$(CC) $(CFLAGS) -c bspfft.c

bspmv_test.o: bspmv_test.c bspedupack.h
	$(CC) $(CFLAGS) -c bspmv_test.c

bspmv.o: bspmv.c bspedupack.h
	$(CC) $(CFLAGS) -c bspmv.c

bspedupack.o: bspedupack.c bspedupack.h
	$(CC) $(CFLAGS) -c bspedupack.c

clean:
	rm -f *.o ip bench lu fft matvec
