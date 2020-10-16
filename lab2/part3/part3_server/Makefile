all: server

server: server.o
	gcc server.o -o server

server.o: server.c
	gcc -c server.c

clean:
	rm server.o server