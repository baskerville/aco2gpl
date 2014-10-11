all: aco2gpl

aco2gpl: aco2gpl.c
	gcc -O2 -Wall -W -o aco2gpl aco2gpl.c

clean:
	rm -f aco2gpl
