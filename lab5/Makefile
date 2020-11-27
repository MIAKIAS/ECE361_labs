all: server client

server: server.o
	gcc server.o -o server -lpthread

server.o: server.c 
	gcc -c server.c -lpthread

client: client.o
	gcc client.o -o client -lpthread

client.o: client.c
	gcc -c client.c -lpthread

clean:
	rm *.o server client