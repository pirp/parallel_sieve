#! /bin/bash
#cd /home/bissstud/Students11/d.taviani/parAlg/BSPedupack
cd ../../../BSPedupack1.01
mpcc -o bench_put bspbench.c bspedupack.c -lbsponmpi
./bench_put $1 
