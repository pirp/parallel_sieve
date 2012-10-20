#include "seqsieve.c"

 int main(int argc, char** argv) {
 	long int n;
 	
 	int i;
 	int s=0;
 	if(argc>1){
	 	n=atoi(argv[1]);
	} else { 
	 	n= 100000;
	}
	int m=n/2;
	int* listPrimes = seq(n);


	for(i=0;i<m;i++){
		if(listPrimes[i]!=0) s++; 
	}

	printf("Number of primes below %ld is %d \n",n,s);



}