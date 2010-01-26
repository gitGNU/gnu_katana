all : voidptr

CFLAGS=-std=c99 -Wall -gdwarf-2 -g3 -fvar-tracking --save-temps -Xlinker --emit-relocs

voidptr : main.c
	gcc -o voidptr $(CFLAGS) main.c

clean :
	rm -f *.o voidptr