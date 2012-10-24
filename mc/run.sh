#! /bin/sh

for p in 1 2 4
	do
		echo "$p"
		echo "\n \n $p processors \n" >> 1e8.txt
		echo "\n \n $p processors \n" >> 1e9.txt
		./parallelsieve 100000000 $p >> 1e8.txt
		./parallelsieve 1000000000 $p >> 1e9.txt

	done
