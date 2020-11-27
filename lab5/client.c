/*  ECE361 Lab5 client program
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
int client_join_session(int mySocket, char* buf);
int client_create_session(int mySocket, char* buf);
int client_leave_session(int mySocket);
int client_list(int mySocket);
int client_text(int mySocket, char* buf);
int client_exit(int mySocket);

bool isInSession = false;
bool isLogIn = false;
char curr_client_id[1024] = {0};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//define a thread to receive messages
pthread_t receive_th;

int main(){

    int mySocket = -1;
    

    while (true){
        //keep receiving commands
        char command[255] = {0};

        fgets(command, 1024, stdin);
        command[strlen(command) - 1] = 0; //fgets counts '\n'

        if (strncmp(command, "/login", strlen("/login")) == 0){

            if (isLogIn){
                printf("You Have Already Logged In Another Account, Please Log Out first...\n");
                continue;
            }
            mySocket = client_login(command);
            if (mySocket != -1){
                isLogIn = true;
                printf("Successfully Logged In...\n");
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
                pthread_mutex_unlock(&lock);
                continue;
            }
            pthread_mutex_unlock(&lock);
            client_logout(mySocket);

            pthread_mutex_lock(&lock);
            isInSession = false;
            isLogIn = false;
            memset(curr_client_id, 0, sizeof(curr_client_id));
            printf("Successfully Logged Out\n");
            pthread_mutex_unlock(&lock);

        } else if (strncmp(command, "/joinsession", strlen("/joinsession")) == 0){
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_join_session(mySocket, command);

        } else if (strncmp(command, "/leavesession", strlen("/leavesession")) == 0){
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
                continue;
            } else if (isInSession = false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
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
                pthread_mutex_unlock(&lock);
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_create_session(mySocket, command);
            
        } else if (strncmp(command, "/list", strlen("/list")) == 0){

            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
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
                pthread_mutex_unlock(&lock);
                continue;
            } else if (isInSession = false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
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
        //printf("Source: %s\n", msg_struct.source);


        if (type == JN_ACK){
            strcpy(msg_struct.source, "SERVER");
            isInSession = true;
            printf("Successfully Joined Session: %s\n", msg_struct.data);
        } else if (type == JN_NAK){
            strcpy(msg_struct.source, "SERVER");
            isInSession = false;
            printf("Failed To Join Session: %s\n", msg_struct.data);
        } else if (type == NS_ACK){
            strcpy(msg_struct.source, "SERVER");
            isInSession = true;
            printf("Successfully Created and Joined Session: %s\n", msg_struct.data);
        } else if (type == QU_ACK){
            strcpy(msg_struct.source, "SERVER");
            printf("%s\n", msg_struct.data);
        } else{
            printf("%s: %s\n", msg_struct.source, msg_struct.data);
        } 
        pthread_mutex_unlock(&lock);
    }
}

int client_login(char* buf){
    char serverAddr[255] = {0};
    char port[255] = {0};

    char msg[255] = {0};
    if (message_to_command(buf, msg, "N/A") == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    //using space to distinguish ip and port
    int spaceCount = 0;
    char *thirdSpace, *fourthpace;
    char* i;
    for (i = buf; *i != '\0'; i++){
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
    if (*i == '\0'){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    strncpy(serverAddr, thirdSpace + 1, fourthpace - thirdSpace - 1);
    strcpy(port, fourthpace + 1);
    //printf("Server IP: %s\nServer Port: %s\n", serverAddr, port);

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

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    //wait for server's confirmation
    char confirm[255] = {0};
    if (recv(mySocket, confirm, 255, 0) <= 0){
        syserror("recv");
    }

    //printf("Message Received: %s\n", confirm);
    struct message msg_recv;
    int type = command_to_message(confirm, &msg_recv);
    
    if (type == LO_ACK){
        struct message temp;
        command_to_message(msg, &temp);
        char *comma = strchr(temp.data, ',');
        strncpy(curr_client_id, temp.data, comma - (char*)temp.data);

        return mySocket;

    } else{

        printf("Log In Failed: %s\n", msg_recv.data);
        return -1;

    }
    /*===========================================================*/
}

int client_logout(int mySocket){
    char msg[255] = {0};
    if (message_to_command("/logout", msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }
    return 0;
}

int client_join_session(int mySocket, char* buf){
    char msg[255] = {0};
    if (message_to_command(buf, msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    //while (!isJnAckRecv){}

    return 0;
}

int client_create_session(int mySocket, char* buf){
    char msg[255] = {0};
    if (message_to_command(buf, msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }
    
    return 0;
}

int client_leave_session(int mySocket){
    char msg[255] = {0};
    if (message_to_command("/leavesession", msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    return 0;
}

int client_list(int mySocket){
    char msg[255] = {0};
    if (message_to_command("/list", msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
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

int client_text(int mySocket, char* buf){
    char msg[255] = {0};
    if (message_to_command(buf, msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    return 0;
}