/*  ECE361 Lab2 client program
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
#include "../packet.h"

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
    
    //find the last '/' in the path if exist
    int i = strlen(filePath);
    while (i >= 0 && filePath[i] != '/'){
        i--;
    }
    char* fileName = filePath + i + 1; //now fileName points to only the name

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
        fragments[i].filename = fileName;
        if (i == number_frag - 1){
            fragments[i].size = numberChar % 1000;
        } else{
            fragments[i].size = 1000;
        }
    }

    //send fragments with stop-and-wait
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
            free(message);
            fclose(fd);
            free(fragments);
            printf("RECEIVING ERROR! File Transmission Closed...\n");
            return;
        }
  
        //check the reply ACK message
        //struct packet* ACK = s_to_p(buf);

        if (strcmp(buf, "ACK") != 0){
            printf("Acknowledge for fragment #%d message did not match!\n", i + 1);
            printf("ERROR! File Transmission Closed...\n");
            free(message);
            fclose(fd);
            free(fragments);
            //free(ACK);
            return;
        }
        printf("Received ACK of fragment #%d\n", i + 1);
        free(message);
        //free(ACK);
    }
    printf("File Transmission Finished...\n");
    fclose(fd);
    free(fragments);
}