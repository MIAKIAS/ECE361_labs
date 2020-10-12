/*  ECE361 Lab1 server program
    Produced by Weizhou Wang and Adam Wu
    Some of the code are borrowed from "Beej's Guide to Network Programming"
    Input Format: server <UDP listen port>*/
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<netdb.h>
#include<errno.h>

void usage(){
	fprintf(stderr, "Incorrect inputs\n");
	exit(1);
}

void syserror(char* name){
    fprintf(stderr, "%s\n", name);
    exit(1);
}

int main(int argc, char** argv){
    //first check input format
    if (argc != 2) usage(); 

    char* port = argv[1];
    //int port = atoi(argv[1]);

    //create the address information
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC; // dont's care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // UDP sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    if (getaddrinfo(NULL, port, &hints, &res) != 0) 
        syserror("getaddrinfo");


    int mySocket;
    struct addrinfo *p;

    //traverse the list to find a valid one
    for (p = res; p != NULL; p = p->ai_next){
        //create the socket
        mySocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (mySocket == -1){
            continue;
        } 

        //bind the socket with the port on local machine
        if (bind(mySocket, p->ai_addr, p->ai_addrlen) == -1){
            continue;
        }

        break;
    }

    if (p == NULL) syserror("socket or bind have errors");

    //create a buffer to receive message
    char buf[1024] = {0};

    //create a sockaddr to get IP address and port of the originating machine
    //we only focus on IPv4
    struct sockaddr_in from;
    int fromLen = sizeof(struct sockaddr_storage);

    //keep accepting message from client
    int bytesReceived = recvfrom(mySocket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    if (bytesReceived < 0){
        syserror("recvfrom");
    } 

    //send back message based on the received message
    printf("Message received: \"%s\"\n", buf);
    if (strcmp("ftp", buf) == 0){
        if (sendto(mySocket, "yes", strlen("yes") + 1, 0, (struct sockaddr*)&from, fromLen) == -1)
            syserror("sendto_yes");
        printf("Message replied: \"yes\"\n");
    } else{
        if (sendto(mySocket, "no", strlen("no") + 1, 0, (struct sockaddr*)&from, fromLen) == -1)
            syserror("sendto_no");
        printf("Message replied: \"no\"\n");
    }

    close(mySocket);
    freeaddrinfo(res);
    return 0;
}