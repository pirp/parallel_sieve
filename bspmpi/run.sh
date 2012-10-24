#! /bin/sh

for p in 1 2 4
	do
		echo "$p"
		echo "\n \n $p processors \n" >> bspmpi-1e8.txt
		echo "\n \n $p processors \n" >> bspmpi-1e9.txt
		./mk.sh $p 100000000 >> bspmpi-1e8.txt
		./mk.sh $p 1000000000 >> bspmpi-1e9.txt

	done
