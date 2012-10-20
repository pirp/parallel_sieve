#include "BSPedupack1.01/bspedupack.c"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "seqsieve.c"

int P;
int N;



void parSieve(){
	bsp_begin(P);
	int p,s;
	int m=N/2;
	int size_seq_list=sqrt(N);
	p=bsp_nprocs();
	s=bsp_pid();

	int* list=seq(size_seq_list);
	
	int i,j;
	int count=0;


	for(i=0;i<size_seq_list;i++) if(list[i]!=0) count++; 

	int* primes_until_sqrootn=vecalloci(count);
	
	i=0;
	for(j=0;j<size_seq_list;j++) {
		if(list[j]!=0) 
		{
			primes_until_sqrootn[i]=list[j];
			i++;

		}
	}

	printf("%d The number of primes below %d is %d \n",s,(int)sqrt(N),count);
	//for(i=0;i<count;i++) printf("%d: l[%d]=%d\n",s,i,primes_until_sqrootn[i]);
	//int nloc=(N-size_seq_list)/p;
	bsp_end();
}

int main(int argc, char **argv){
	bsp_init(parSieve, argc, argv);

	N = atoi(argv[1]);
	P = atoi(argv[2]);
	

	parSieve();

	exit(0);
}