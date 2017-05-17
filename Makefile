all: bench2 bench rdtscp sy

bench2:
	gcc -O3 -march=native -o bench2 bench2.c -lrt

bench:
	gcc -O3 -march=native -o bench bench.c -lrt

sy:
	gcc -O3 -march=native -o sy sy.c -lrt

rdtscp:
	gcc -O3 -march=native -o rdtscp rdtscp.c -lrt

