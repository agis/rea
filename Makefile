all: rea.c
	gcc -Wall http_parser.h http_parser.c rea.c -o rea

clean:
	rm rea
