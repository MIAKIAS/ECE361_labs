/*  ECE361 Lab2 server program
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
#include"../packet.h"

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
    int bytesReceived;

RECEIVE_FIRST:    //keep accepting message from client
    bytesReceived = recvfrom(mySocket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    if (bytesReceived < 0){
        if (sendto(mySocket, "NACK", strlen("NACK"), 0, (struct sockaddr*)&from, fromLen) <= 0)
                syserror("sendto_NACK");
        syserror("recvfrom");
    } 

    struct packet *fragment = s_to_p(buf);

    FILE *f;
    
    int total_fragments;
    char *path;

    //check whether it is the first packet
    if(fragment->frag_no != 1){
        clear_packet(fragment);
        goto RECEIVE_FIRST;
    }
    else{
        if (sendto(mySocket, "ACK#1", strlen("ACK#1"), 0, (struct sockaddr*)&from, fromLen) <= 0)
            syserror("sendto_ACK");
        
        path = malloc(sizeof(char) * (strlen(fragment->filename) + 1));
        strcpy(path, fragment->filename);
        
        f = fopen(path, "w");        
        
        printf("A new file is created.\n");

        total_fragments = fragment->total_frag;
        printf("total_fragments: %d\n", total_fragments);
        fwrite(fragment->filedata, sizeof(char), fragment->size, f);
        printf("Received from deliver, writing to file, send ACK#1 to server\n"); 

        if(total_fragments == 1){
            printf("Done\n");
        }
    }

    clear_packet(fragment);

    int i = 0;

    for(i = 2; i <= total_fragments; ++i){
RECEIVE_NEXT:
        if(recvfrom(mySocket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen) <= 0){
            syserror("recvfrom");
            
            if (sendto(mySocket, "NACK", strlen("NACK"), 0, (struct sockaddr*)&from, fromLen) <= 0)
                syserror("sendto_NACK");

            break;
        }
       
        fragment = s_to_p(buf);

        //check packet sequence is the next one
        if(fragment->frag_no != i){
            clear_packet(fragment);
            goto RECEIVE_NEXT;
        }

        fwrite(fragment->filedata, sizeof(char), fragment->size, f);
        
        //get ack#
        char ack[100] = "ACK#";
        char ackNum[10] = {0};
        sprintf(ackNum, "%d", fragment->frag_no);
        strcat(ack, ackNum);
        printf("%s\n", ack);

        clear_packet(fragment);

        if (sendto(mySocket, ack, strlen(ack), 0, (struct sockaddr*)&from, fromLen) <= 0){
            syserror("sendto_ACK");
            break;
        }

        printf("Received from deliver, writing to file, send %s to server\n", ack);    
    }

    fclose(f);
    free(path);

    if(i == total_fragments + 1){
        printf("Done\n");
    }
        
    close(mySocket);
    freeaddrinfo(res);
    return 0;
}