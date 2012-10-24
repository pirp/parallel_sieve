seq:
	gcc -O3 -o sieve sequential.c -lm
par:
	mpicc -O3 -o par_sieve parallelsieve.c -lbsponmpi -lm #-g
all: seq par

