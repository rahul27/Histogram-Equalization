hist.o: hist.c
	gcc -std=c99 -lm -ltiff hist.c -o hist.o
