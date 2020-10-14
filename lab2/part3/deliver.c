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
#include "packet.h"

//check whether the input is correct
void usage();
//print out error message and terminate the program
void syserror(char* name);
//send file using packet structure
void sendFileTo(char* filePath, int mySocket, struct sockaddr* dest_addr, socklen_t addrlen);

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

    //send the file
    sendFileTo(second, mySocket, res->ai_addr, sizeof(struct sockaddr_storage));
    
    //free the list
    freeaddrinfo(res);
    //break the connection
    close(mySocket);
    return 0;
}

void usage(){
	fprintf(stderr, "Incorrect inputs\n");
	exit(1);
}

void syserror(char* name){
    fprintf(stderr, "%s\n", name);
    exit(1);
}

void sendFileTo(char* filePath, int mySocket, struct sockaddr* dest_addr, socklen_t addrlen){
    struct packet* ACK;
    //open the file, read only
    FILE* fd = fopen(filePath, "r");
    if (fd == NULL){
        syserror("File open");
    }

    //measure the total number of characters in the file
    //first move the curser to the end of file
    fseek(fd, 0, SEEK_END);
    //then get the # of characters
    int numberChar = ftell(fd);

    //calculate the total fragments needed
    int number_frag = numberChar / 1000;
    if (numberChar % 1000 != 0) number_frag++;

    //set the curser back
    fseek(fd, 0, SEEK_SET);

    char receive_buffer[1000] = {0};
    struct packet* fragments = (struct packet*)malloc(sizeof(struct packet) * number_frag); //store fragments
    
    //create fragments
    for (int i = 0; i < number_frag; ++i){
        //struct packet* myPacket = (struct packet*)malloc(sizeof(struct packet));
        memset(fragments[i].filedata, 0, sizeof(fragments[i].filedata));
    
        //read data from file
        if (fread(fragments[i].filedata, sizeof(char), 1000, fd) != 1000 && i != number_frag - 1)
            syserror("file read");

        //update packet info
        fragments[i].total_frag = number_frag;
        fragments[i].frag_no = i + 1;
        fragments[i].filename = filePath;
        if (i == number_frag - 1){
            fragments[i].size = numberChar % 1000;
        } else{
            fragments[i].size = 1000;
        }
    }

    //send fragments with stop-and-wait
    int retransmit = 0;
    //set a timer for receiving 
    struct timeval timer;
    timer.tv_sec = 0;
    timer.tv_usec = 500;
    if (setsockopt(mySocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timer, sizeof(timer)) < 0)
        syserror("set timer");

    for (int i = 0; i < number_frag; ++i){
        //send the fragment
        int size = 0;
        char* message = p_to_s(&fragments[i], &size);
        if (sendto(mySocket, message, size, 0, dest_addr, sizeof(struct sockaddr_storage)) <= 0)
            syserror("sendto");

        //wait for ACK
        char buf[1024] = {0};
        struct sockaddr_in from;
        int fromLen = sizeof(struct sockaddr_storage);

        if (recvfrom(mySocket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen) <= 0){
            //if failed, may time out, retry
            printf("Fragment #%d time out! Retransmitting...\n", i + 1);
            i--;
            retransmit++;
            if (retransmit > 3){
                printf("Restransmit too many times.\n"
                "There may be a connection/server issue, please check and rerun the program...\n");
                free(message);
                free(fragments);
                fclose(fd);
                return;
            }
            free(message);
            continue;
        }
  
        //check the reply ACK message
        //struct packet* ACK = s_to_p(buf);

        if (strcmp(buf, "ACK") != 0){
            printf("Acknowledge for fragment #%d message did not match! Retransmitting...\n", i + 1);
            i--;
            retransmit++;
            if (retransmit > 3){
                printf("Restransmit too many times.\n" 
                "There may be a connection/server issue, please check and rerun the program...\n");
                free(message);
                //free(ACK);
                free(fragments);
                fclose(fd);
                return;
            }
            free(message);
            //free(ACK);
            continue;
        }
        printf("Received ACK of fragment #%d\n", i + 1);
        retransmit = 0;
        free(message);
        //free(ACK);
    }
    fclose(fd);
    free(fragments);
}