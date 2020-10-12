/*  ECE361 Lab1 client program
    Produced by Weizhou Wang and Adam Wu
    Some of the code are borrowed from "Beej's Guide to Network Programming"
    Input Format: deliver <server address> <server port number>*/
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<netdb.h>
#include<fcntl.h>
#include<stdbool.h>
#include<dirent.h>
#include<time.h>

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
    if (argc != 3) usage(); 

    char* serverAddr = argv[1];
    char* port = argv[2];
    //int port = atoi(argv[2]);

    // //create the address information
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC; // dont's care IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // UDP sockets

    if (getaddrinfo(serverAddr, port, &hints, &res) != 0) 
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

        break;
    }

    if (mySocket == -1) syserror("socket");

    /*do not need to bind, since we are not a server*/

    //prompt user to input the message
    printf("Please input a message...\n");

    char first[1024] = {0};
    char second[1024] = {0};

    scanf("%s%s", first, second);


    //check whether the file is valid
    bool exist = false;
    exist = (access(second, F_OK) == 0);

    //if file does not exist, exit
    if (!exist){
        printf("no such file...\n");
        //free the list
        freeaddrinfo(res);
        //break the connection
        close(mySocket);
        return 0;
    } 

    //get the begin time
    clock_t begin = clock();

    //send message to server
    if (sendto(mySocket, first, strlen(first) + 1, 0, res->ai_addr, sizeof(struct sockaddr_storage)) <= 0)
        syserror("sendto_ftp");

    //keep accepting message from client
    char buf[1024] = {0};
    struct sockaddr_in from;
    int fromLen = sizeof(struct sockaddr_storage);

    int bytesReceived = recvfrom(mySocket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    if (bytesReceived < 0)
        syserror("recvfrom");
    
    //get the end time
    clock_t end = clock();

    //calculate the round-trip time
    clock_t totalTime = end - begin;
    printf("The round-trip time is %d msec.\n", totalTime);

    if (strcmp(buf, "yes") == 0){
        printf("A file transfer can start.\n");
    } else{
        printf("Rejected by server...\n");
        //free the list
        freeaddrinfo(res);
        //break the connection
        close(mySocket);
        return 0;
    }

    //free the list
    freeaddrinfo(res);
    //break the connection
    close(mySocket);
    return 0;
}