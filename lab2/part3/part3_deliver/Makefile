all: deliver

deliver: deliver.o
	gcc deliver.o -o deliver

deliver.o: deliver.c
	gcc -c deliver.c

clean:
	rm deliver.o deliver