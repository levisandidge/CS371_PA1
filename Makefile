#! /usr/bin/make -f

C=gcc -Og -g

clean:
	-rm *.o PA1

PA1: pa1.o
	$(C) -o $@ $^

%.o : %.c
	$(C) -c -o $@ $<
