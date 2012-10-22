#include <stdio.h>
#include <stdlib.h>

int main(){
	int i,j,k;
	for(i=0;i<3;i++){
		for(j=0;j<3;j++){
			for(k=j;k<3;k++){
				if(j+2==k) goto next;
				printf("%d,%d,%d\n",i,j,k);
			}
		}
		next:{}
	}

}