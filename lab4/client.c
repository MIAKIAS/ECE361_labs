/*  ECE361 Lab4 client program
    Produced by Weizhou Wang and Adam Wu
    Some of the code are borrowed from "Beej's Guide to Network Programming"
    Input Format: client */
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
#include<stdbool.h>
#include "common.h"
#include "client.h"

void* keep_receiving(void* mySocket);
int client_login(char* buf);
int client_logout(int mySocket);
int client_join_session(int mySocket, char* msg);
int client_create_session(int mySocket, char* msg);
int client_leave_session(int mySocket);
int client_list(int mySocket);
int client_text(int mySocket, char* msg);
int client_exit(int mySocket);

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int main(){
    //define a thread to receive messages
    pthread_t receive_th;

    int mySocket = -1;
    bool isLogIn = false;
    bool isInSession = false;

    while (true){
        //keep receiving commands
        char command[255] = {0};

        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = 0; //fgets counts '\n'

        
        if (strncmp(command, "/login", strlen("/login")) == 0){

            mySocket = client_login(command);
            if (mySocket != -1){
                isLogIn = true;
                if (pthread_create(&receive_th, NULL, keep_receiving, &mySocket) != 0) {
                    syserror("thread create");
                }
            } else {
                printf("Log In Failed, Please Try Again...\n");
                continue;
            }

        } else if (strncmp(command, "/logout", strlen("/logout")) == 0){

            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }
            client_logout(mySocket);
            isLogIn = false;

        } else if (strncmp(command, "/joinsession", strlen("/joinsession")) == 0){

            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }
            if (client_join_session(mySocket, command) != -1){
                isInSession = true;
            } else{
                printf("Sessison Join Failed, Please Try Again...\n");
                continue;
            }


        } else if (strncmp(command, "/leavesession", strlen("/leavesession")) == 0){
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            } else if (isInSession = false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                continue;
            }
            client_leave_session(mySocket);
            isInSession = false;

        } else if (strncmp(command, "/createsession", strlen("/createsession")) == 0){
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }

            if (client_create_session(mySocket, command) != -1){
                isInSession = true;
            } else{
                printf("Sessison Creation Failed, Please Try Again...\n");
                continue;
            }
            
        } else if (strncmp(command, "/list", strlen("/list")) == 0){

            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }

            client_list(mySocket);
            
        } else if (strncmp(command, "/quit", strlen("/quit")) == 0){

            client_exit(mySocket);

            close(mySocket);
            pthread_mutex_destroy(&lock);

            printf("Bye~\n");

            return 0;

        } else{ //this is for the text command

            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            } else if (isInSession = false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                continue;
            }

            client_text(mySocket, command);

        } 
        
    }


    return 0;
}


void* keep_receiving(void* mySocket){
    int* temp = (int*)mySocket;
    while (true){
        pthread_mutex_lock(&lock);
        char msg[255] = {0};
        if (recv(*temp, msg, 254, 0) <= 0){
            syserror("recv");
        }
        printf("%s\n", msg);
        pthread_mutex_unlock(&lock);
    }
}

int client_login(char* buf){
    char serverAddr[255] = {0};
    char port[255] = {0};

    //using space to distinguish ip and port
    int spaceCount = 0;
    char *thirdSpace, *fourthpace;
    for (char* i = buf; *i != '\0'; i++){
        if (*i == ' '){
            spaceCount++;
            if (spaceCount == 3){
                thirdSpace = i;
            } else if (spaceCount == 4){
                fourthpace = i;
                break;
            }
        } 
    }

    strncpy(serverAddr, thirdSpace + 1, fourthpace - thirdSpace - 1);
    strcpy(port, fourthpace + 1);
    printf("Server IP: %s\nServer Port: %s\n", serverAddr, port);

    /*===================Connect to the server==================*/
    //create the address information
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC; // dont's care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP sockets
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

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
        if (connect(mySocket, p->ai_addr, p->ai_addrlen) == -1) {
            close(mySocket);
            syserror("connect");
            continue;
        }
        break;
    }

    if (mySocket == -1) syserror("socket");

    freeaddrinfo(res);

    if (send(mySocket, buf, strlen(buf) + 1, 0) <= 0){
        syserror("send");
    }

    //wait for server's confirmation
    char confirm[255] = {0};
    if (recv(mySocket, confirm, 255, 0) <= 0){
        syserror("recv");
    }

    printf("Message Received: %s\n", confirm);

    struct message recv_msg;
    recv_msg.type = -1;
    recv_msg.size = -1;
    strcpy(recv_msg.source, "");
    strcpy(recv_msg.data, "");
    if (strcmp(confirm, "LO_ACK") == 0){
        return mySocket;
    } else{
        return -1;
    }
    /*===========================================================*/
}

int client_logout(int mySocket){
    pthread_mutex_lock(&lock);
    if (send(mySocket, "/logout", strlen("logout") + 1, 0) <= 0){
        syserror("send");
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

int client_join_session(int mySocket, char* msg){
    pthread_mutex_lock(&lock);
    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }
    
    char confirm[255] = {0};
    if (recv(mySocket, confirm, 255, 0) <= 0){
        syserror("recv");
    }

    pthread_mutex_unlock(&lock);

    if (strcmp(confirm, "JN_ACK") == 0){
        return 1;
    } else{
        return -1;
    }
}

int client_create_session(int mySocket, char* msg){
    pthread_mutex_lock(&lock);
    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }
    
    char confirm[255] = {0};
    if (recv(mySocket, confirm, 255, 0) <= 0){
        syserror("recv");
    }

    pthread_mutex_unlock(&lock);

    if (strcmp(confirm, "NS_ACK") == 0){
        return 1;
    } else{
        return -1;
    }
}

int client_leave_session(int mySocket){
    pthread_mutex_lock(&lock);
    if (send(mySocket, "/leavesession", strlen("leavesession") + 1, 0) <= 0){
        syserror("send");
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

int client_list(int mySocket){
    pthread_mutex_lock(&lock);
    if (send(mySocket, "/list", strlen("/list") + 1, 0) <= 0){
        syserror("send");
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

int client_exit(int mySocket){
    pthread_mutex_lock(&lock);
    if (send(mySocket, "/quit", strlen("/quit") + 1, 0) <= 0){
        syserror("send");
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

int client_text(int mySocket, char* msg){
    pthread_mutex_lock(&lock);
    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }
    pthread_mutex_unlock(&lock);
    return 0;
}
