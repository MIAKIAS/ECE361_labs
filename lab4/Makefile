all: server client

server: server.o
	gcc server.o -o server

server.o: server.c 
	gcc -c server.c

client: client.o
	gcc client.o -o client

client.o: client.c
	gcc -c client.c

clean:
	rm *.o server client 