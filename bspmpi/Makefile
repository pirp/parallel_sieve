seq:
	gcc -o sieve sequential.c -lm
par:
	mpcc -o par_sieve parallelsieve.c -lbsponmpi -lm #-g
all: seq par

