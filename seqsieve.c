#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int* seq(long int n){
	int m=n/2;
	int i,j;
	int *list = malloc(m*sizeof(int));
	int s=0;

	list[0]=2;

	for(i=1;i<m;i++) list[i]=2*i+1;



	for(i=1;i<=sqrt(n);i++){
		if(list[i]==0) continue;
		for(j=3*i+1;j<=m;j+=2*i+1) list[j]=0;
	}

	return list;

}