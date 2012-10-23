#! /bin/bash
make par
mpirun -np 4 par_sieve $2 $1
