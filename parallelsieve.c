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
	int q=sqrt(N);
	p=bsp_nprocs();
	s=bsp_pid();

	int* list=seq(q);
	
	int i,j;
	int count=0;


	for(i=0;i<q;i++) if(list[i]!=0) count++; 

	int* initial_primes = vecalloci(count);
	
	i=0;
	for(j=0;j<q;j++) {
		if(list[j]!=0) 
		{
			initial_primes[i]=list[j];
			i++;

		}
	}

//	printf("%d The number of primes below %d is %d \n",s,(int)sqrt(N),count);
	double ratio = (double)(N-q)/(2*p);
	int b=ceil(ratio); // number of local element
	
	printf("local elements %d\n",b);


	int* local=vecalloci(b);
	
	i=0;
	j=q+2*s*b;

	if(j%2==0) j++;
	
	while( i<b && j<N ){ 
		local[i]=j;
		i++;
		j+=2;
	}

	if(s==3) for(i=0;i<b;i++) printf("%d: l[%d]=%d\n",s,i,local[i]);

	bsp_end();
}

int main(int argc, char **argv){
	bsp_init(parSieve, argc, argv);

	N = atoi(argv[1]);
	P = atoi(argv[2]);
	

	parSieve();

	exit(0);
}