#include "BSPedupack1.01/bspedupack.c"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "seqsieve.c"

int P;
int N;
int goldbach = 0;
int twins=0;


void parSieve(){
	bsp_begin(P);
	int p,s;
	p=bsp_nprocs();
	s=bsp_pid();

	bsp_push_reg(&N,SZINT);
  	bsp_sync();
  	bsp_get(0,&N,0,&N,SZINT);  
  	bsp_sync();
  	bsp_pop_reg(&N);
	
	int m=N/2;
	int q=sqrt(N);

	int i,j;

	double t0=bsp_time();


	// sieves sequentially and creates a list with only the primes up to sqrt(N)

	int* list=seq(q);
	int count=0;
	for(i=0;i<q/2;i++) if(list[i]!=0) count++; 

	int* initial_primes = vecalloci(count);
	
	i=0;
	for(j=0;j<q/2;j++) {
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

	while(i<b){
		local[i]=0;
		i++;
	}
	

	//actual parallel sieve: whenever a multiple is found it is flagged by setting it negative

	

	for(i=0;i<count;i++){
		for(j=0;j<b;j++) if(abs(local[j])%initial_primes[i] == 0) break;
		while(j<=b){
			if(local[j]>0) local[j] = -local[j];
			j+=initial_primes[i];
		}
	}

	// counting the local primes and informing all other processors

	int count2=0;

	for(i=0;i<b;i++) if(local[i]>0) count2++;
	if (s==0) count2+=count;

	int* globalCount = vecalloci(p);
	bsp_push_reg(globalCount,p*SZINT);
	bsp_sync();
	
	for(i=0;i<p;i++) bsp_put(i,&count2,globalCount,s*SZINT,SZINT);
	bsp_sync();
	bsp_pop_reg(globalCount);

	double t1=bsp_time();

	// total count of primes

	int finalCount=0;
	for(i=0;i<p;i++) finalCount+=globalCount[i];

	printf("%3d: Found %d primes in %.6lf s\n",s,finalCount,t1-t0);

	// Check for twin primes

	int nlocaltwins = 0;

	/*
	* processor 0 checks also internally in the list of primes up to sqrt(N) and in
	* the boundary between this list and the local list just sieved
	*/ 

	if(s==0){
		for(i=1;i<count-1;i++) nlocaltwins+=twin(initial_primes[i],initial_primes[i+1]);
		if(twin(initial_primes[count-1],local[0])) nlocaltwins++;
	}

	// check internally in the local list just sieved

	for(i=0;i<b-1;i++){
		if(local[i]<0) continue;
		if(local[i+1]>0) nlocaltwins++;
	}

	// looks wether its last number was prime and let the next processor know
	int local_last_is_prime = ((local[b-1]>0) ? 1 :0);
	int previous_last_is_prime=0;
	bsp_push_reg(&previous_last_is_prime,SZINT);
	bsp_sync();
	if(s<p-1) bsp_put(s+1,&local_last_is_prime,&previous_last_is_prime,0,SZINT);
	bsp_sync();
	bsp_pop_reg(&previous_last_is_prime);
	
	// check for twin primes split to processors s-1 and s]
	if(previous_last_is_prime && (local[0]>0)) nlocaltwins++;


	// communication to let all processor know the local number of twin primes and sum it

	int* globalTwinCount = vecalloci(p);
	bsp_push_reg(globalTwinCount,p*SZINT);
	bsp_sync();
	for(i=0;i<p;i++) bsp_put(i,&nlocaltwins,globalTwinCount,s*SZINT,SZINT);
	bsp_sync();
	bsp_pop_reg(globalTwinCount);

	double t2=bsp_time();

	int finalTwinCount=0;
	for(i=0;i<p;i++) finalTwinCount+=globalTwinCount[i];
	if(twins){
		printf("%3d: Found %d pairs of twin primes in %.6lf s\n",s,finalTwinCount,t2-t1);
	}

	// Goldbach conjecture checker: since it is rather costly it is done only whenever the appropriate flag is set
	if(goldbach){

		// compacts the list of primes sieved in parallel

		int* localPrimes = vecalloci(count2);

		i=0;

		if(s==0){
			for(j=0;j<count;j++) {
				localPrimes[i]=initial_primes[j];
				i++;
			}
		}

		for(j=0;j<b;j++) {
			if(local[j]>0){
				localPrimes[i]=local[j];
				i++;
			}
		}


		int* allPrimes = vecalloci(finalCount);
		bsp_push_reg(allPrimes,finalCount*SZINT);
		bsp_sync();
		int offset =0;
		for(i=0;i<s;i++) offset+=globalCount[i];
		for(i=0;i<p;i++) bsp_put(i,localPrimes,allPrimes,offset*SZINT,count2*SZINT);
		bsp_sync();
		bsp_pop_reg(allPrimes);	

		// computes the largest even to be checked and split the evens evenly to all the processors

		int largestEven = ( N%2 ? N-1 : N );
		int blockEven = ceil(1.0*(largestEven-4)/(2*p));

		int* listEven = vecalloci(blockEven);

		i=0;
		j=4+2*s*blockEven;

		while( i<blockEven && j<=largestEven ){ 
			listEven[i]=j;
			i++;
			j+=2;
		}
		while(i<blockEven){
			listEven[i]=0;
			i++;
		}
		int k;

		// looks for the Goldbach partition of every local even

		for(i=0;i<blockEven;i++){
			for(j=0;j<finalCount;j++){
				for(k=j;k<finalCount;k++)
					if(allPrimes[j]+allPrimes[k]==listEven[i]){
						listEven[i]=0;
						goto next;
					}
			}
			next:{};
		}

		// checks whether some number did not have a Goldbach partition

		int numberNonGoldbach=0;
		for(i=0;i<blockEven;i++) if(listEven[i]!= 0) numberNonGoldbach++;
		double t3 = bsp_time();
		printf("%3d: There are %d numbers that are not sum of 2 primes in %.6lf s\n",s,numberNonGoldbach,t3-t2);

		vecfreei(listEven);
		vecfreei(localPrimes);
		vecfreei(allPrimes);
	}
	vecfreei(local);
	vecfreei(globalCount);
	vecfreei(globalTwinCount);
	bsp_end();
}

int twin(int i, int j){
	return ((j-2==i) ? 1 : 0);
}

int main(int argc, char **argv){
	bsp_init(parSieve, argc, argv);

	if(argc>1){
		N = atoi(argv[2]);
		if (argc>2){
			P = atoi(argv[1]);
			if(argc>3){
				goldbach = atoi(argv[3]);
				twins=1;
			}			
		} else {
			P=1;
		}
		
	} else {
		N=100;
		P=1;
	}

	parSieve();

	exit(0);
}
