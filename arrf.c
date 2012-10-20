#include "BSPedupack1.01/bspedupack.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


int* myFunc(int n) {
    int i;
 
    
    int *x = malloc(n * sizeof(int));
 
    
    if (x == 0) {
        return 0; 
    }
 
    
    for (i=0; i<n; i++) {
        x[i] = i;
    }
 
    
    return x;
}

int main(int argc, char **argv){

int* x=myFunc(atoi(argv[1]));
int i;
for (i=0;i<atoi(argv[1]);i++){
printf("x[%d]=%d \n",i,x[i]);
}
}