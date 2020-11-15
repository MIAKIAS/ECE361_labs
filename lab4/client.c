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

bool isLoAckRecv = false;
bool isJnAckRecv = false;
bool isQuAckRecv = false;
bool isNsAckRecv = false;
bool isInSession = false;
bool isLogIn = false;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int main(){
    //define a thread to receive messages
    pthread_t receive_th;

    int mySocket = -1;
    

    while (true){
        //keep receiving commands
        char command[255] = {0};

        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = 0; //fgets counts '\n'

        printf("Command Typed: %s\n", command);

        if (strncmp(command, "/login", strlen("/login")) == 0){

            if (isLogIn){
                printf("You Have Already Logged In Another Account, Please Log Out first...\n");
                continue;
            }
            mySocket = client_login(command);
            if (mySocket != -1){
                isLogIn = true;

                if (pthread_create(&receive_th, NULL, keep_receiving, &mySocket) != 0) {
                    syserror("thread create");
                }
            } else {
                continue;
            }

        } else if (strncmp(command, "/logout", strlen("/logout")) == 0){
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }
            pthread_mutex_unlock(&lock);
            client_logout(mySocket);

            pthread_mutex_lock(&lock);
            isLoAckRecv = false;
            isJnAckRecv = false;
            isQuAckRecv = false;
            isNsAckRecv = false;
            isInSession = false;
            isLogIn = false;
            printf("Successfully Logged Out\n");
            pthread_mutex_unlock(&lock);

        } else if (strncmp(command, "/joinsession", strlen("/joinsession")) == 0){
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_join_session(mySocket, command);

        } else if (strncmp(command, "/leavesession", strlen("/leavesession")) == 0){
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            } else if (isInSession = false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_leave_session(mySocket);

            pthread_mutex_lock(&lock);
            isInSession = false;
            printf("Successfully Left Session\n");
            pthread_mutex_unlock(&lock);

        } else if (strncmp(command, "/createsession", strlen("/createsession")) == 0){
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_create_session(mySocket, command);
            
        } else if (strncmp(command, "/list", strlen("/list")) == 0){

            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_list(mySocket);
            
        } else if (strncmp(command, "/quit", strlen("/quit")) == 0){

            if (isLogIn){
                client_logout(mySocket);
            }

            close(mySocket);
            pthread_mutex_destroy(&lock);

            printf("Bye~\n");

            return 0;

        } else{ //this is for the text command
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                continue;
            } else if (isInSession = false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_text(mySocket, command);

        } 
        
    }


    return 0;
}


void* keep_receiving(void* mySocket){
    int* temp = (int*)mySocket;
    while (true){
        
        if (isLogIn == false){
            return NULL;
        }

        char msg[255] = {0};
        if (recv(*temp, msg, 254, 0) <= 0){
            if (isLogIn == false) return NULL;
            syserror("recv");
        }

        pthread_mutex_lock(&lock);
        //printf("Message Received: %s\n", msg);

        struct message msg_struct;
        int type = command_to_message(msg, &msg_struct);

        isLoAckRecv = false;
        isJnAckRecv = false;
        isQuAckRecv = false;
        isNsAckRecv = false;

        if (type == JN_ACK){
            isJnAckRecv = true;
            isInSession = true;
            printf("Successfully Joined Session: %s\n", msg_struct.data);
        } else if (type == JN_NAK){
            isJnAckRecv = true;
            isInSession = false;
            char *reason = strchr(msg_struct.data, ' ');
            printf("Failed To Join Session: %s\n", reason + 1);
        } else if (type == NS_ACK){
            isNsAckRecv = true;
            isInSession = true;
            printf("Successfully Joined Session: %s\n", msg_struct.data);
        } else if (type == QU_ACK){
            isQuAckRecv = true;
            printf("%s\n", msg_struct.data);
        } else{
            printf("%s\n", msg_struct.data);
        } 
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
    if (strncmp(confirm, "LO_ACK", strlen("LO_ACK")) == 0){
        return mySocket;
    } else{
        int type = command_to_message(confirm, &recv_msg);
        printf("Log In Failed: %s\n", recv_msg.data);
        return -1;
    }
    /*===========================================================*/
}

int client_logout(int mySocket){
    if (send(mySocket, "/logout", strlen("logout") + 1, 0) <= 0){
        syserror("send");
    }
    return 0;
}

int client_join_session(int mySocket, char* msg){
    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    while (!isJnAckRecv){}

    return 0;
}

int client_create_session(int mySocket, char* msg){

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }
    
    return 0;
}

int client_leave_session(int mySocket){

    if (send(mySocket, "/leavesession", strlen("leavesession") + 1, 0) <= 0){
        syserror("send");
    }

    return 0;
}

int client_list(int mySocket){

    if (send(mySocket, "/list", strlen("/list") + 1, 0) <= 0){
        syserror("send");
    }

    return 0;
}

// int client_exit(int mySocket){

//     if (send(mySocket, "/quit", strlen("/quit") + 1, 0) <= 0){
//         syserror("send");
//     }

//     return 0;
// }

int client_text(int mySocket, char* msg){

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    return 0;
}
