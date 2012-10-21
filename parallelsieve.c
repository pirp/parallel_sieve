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
	int i,j;

	double t0=bsp_time();

	// sieves sequentially and creates a list with only the primes up to sqrt(N)
	int* list=seq(q);
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


	// creation of the local part of numbers to sieve in parallel

	double ratio = 1.0*(N-q)/(2*p);
	int b=ceil(ratio); 
	int* local=vecalloci(b);
	
	i=0;
	j=q+2*s*b;

	if(j%2==0) j++;
	
	while( i<b && j<N ){ 
		local[i]=j;
		i++;
		j+=2;
	}

	//actual sieving: whenever a multiple is found flag it by setting it negative

	

	for(i=0;i<count;i++){
		for(j=0;j<b;j++) if(abs(local[j])%initial_primes[i] == 0) break;
		while(j<=b){
			if(local[j]>0) local[j] = -local[j];
			j+=initial_primes[i];
		}
	}

	int count2=0;

	for(i=0;i<b;i++) if(local[i]>0) count2++;
	if (s==0) count2+=count;

	int* globalCount = vecalloci(p);
	bsp_push_reg(globalCount,p*SZINT);
	bsp_sync();
	
	for(i=0;i<p;i++) bsp_put(i,&count2,globalCount,s*SZINT,SZINT);
	bsp_sync();

	double t1=bsp_time();

	int finalCount=0;
	for(i=0;i<p;i++) finalCount+=globalCount[i];

	printf("%d: ha! I claim there are %d primes in %.6lf s\n",s,finalCount,t1-t0);

	//if(s==0) for(i=0;i<b;i++) printf("%d: l[%d]=%d\n",s,i,local[i]);



	bsp_end();
}

int main(int argc, char **argv){
	bsp_init(parSieve, argc, argv);

	N = atoi(argv[1]);
	P = atoi(argv[2]);
	

	parSieve();

	exit(0);
}