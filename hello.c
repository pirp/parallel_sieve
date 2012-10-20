#include "BSPedupack1.01/bspedupack.h"
int P;

void bsphello(){

	bsp_begin(P);
	int p,s;
	p = bsp_nprocs();
	s=bsp_pid();
	printf("Hello from processor %d!\n",s);
	bsp_end();
}
int main(int argc, char **argv){
	P = atoi(argv[1]);
	bsp_init(bsphello, argc, argv);

	bsphello();

	exit(0);
}