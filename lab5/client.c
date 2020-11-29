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
int client_leave_session(int mySocket, char* buf);
int client_list(int mySocket);
int client_text(int mySocket, char* buf);
int client_exit(int mySocket);
int client_invite(int mySocket, char* buf);


struct list{
    struct session* head;
};
struct session{
    struct session* next;
    char session_name[MAX_NAME];
};

void list_insert(char* name);
bool list_delete(char* name);
bool list_find(char* name);
void list_print();

bool isInSession = false;
bool isLogIn = false;
bool isInvite = false;
char invite_session[255] = {0};
char curr_client_id[1024] = {0};
struct list* session_list = NULL; 

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
//define a thread to receive messages
pthread_t receive_th;

int main(){

    int mySocket = -1;

    session_list = malloc(sizeof(struct session));
    session_list->head = NULL;

    while (true){
        //keep receiving commands
        char command[255] = {0};

        fgets(command, 1024, stdin);

        command[strlen(command) - 1] = 0; //fgets counts '\n'/

        //printf("command: %s\n", command);
        if (isInvite){ //check whether client need to reply the invitation first
            if (strcmp(command, "Yes") == 0){ //directly join the session
                strcpy(command, "/joinsession ");
                strcat(command, invite_session);
                isInvite = false;
                memset(invite_session, 0, strlen(invite_session) + 1);
            } else if (strcmp(command, "No") == 0){
                printf("Refuse invitation...\n");
                isInvite = false;
                memset(invite_session, 0, strlen(invite_session) + 1);
                continue;
            } else{
                //re-capture the input
                printf("Invalid Input.\nPlease select whether you want to join (Yes/No)\n");
                continue;
            }
            //printf("command: %s\n", command);
        }

        //check different user input
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
            } else if (isInSession == false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
                continue;
            }
            pthread_mutex_unlock(&lock);

            if (client_leave_session(mySocket, command) == -1){
                continue;
            }

            pthread_mutex_lock(&lock);
            //isInSession = false;
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
            free(session_list);

            printf("Bye~\n");

            return 0;

        } else if (strncmp(command, "/invite", strlen("/invite")) == 0){

            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
                continue;
            } else if (isInSession == false){
                printf("You Have To Join a Session First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
                continue;
            }
            pthread_mutex_unlock(&lock);

            client_invite(mySocket, command);

        } else{ //this is for the text command
            pthread_mutex_lock(&lock);
            if (isLogIn == false){
                printf("You Have To Log In First, Please Try Again...\n");
                pthread_mutex_unlock(&lock);
                continue;
            } else if (isInSession == false){
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
        
        //if logged out, just return
        if (isLogIn == false){
            return NULL;
        }

        char msg[255] = {0};
        if (recv(*temp, msg, 254, 0) <= 0){
            if (isLogIn == false) return NULL;
            printf("Disconnected from server due to long time inactivity...\n");
            printf("Please log in again...\n");
            isInSession = false;
            isLogIn = false;
            memset(curr_client_id, 0, sizeof(curr_client_id));
            return NULL;
            //syserror("recv");
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
            list_insert(msg_struct.data);
        } else if (type == JN_NAK){
            strcpy(msg_struct.source, "SERVER");
            //isInSession = false;
            printf("Failed To Join Session: %s\n", msg_struct.data);
        } else if (type == NS_ACK){
            strcpy(msg_struct.source, "SERVER");
            isInSession = true;
            printf("Successfully Created and Joined Session: %s\n", msg_struct.data);
            list_insert(msg_struct.data);
        } else if (type == QU_ACK){
            strcpy(msg_struct.source, "SERVER");
            printf("%s\n", msg_struct.data);
        } else if (type == INVITE){
            printf("User: %s invites you to join the session: %s\n", msg_struct.source, msg_struct.session);
            printf("Do you want to join? (Yes/No)\n", msg_struct.session, msg_struct.source);
            strcpy(invite_session, msg_struct.session);
            isInvite = true; //set the flag, leave for the other thread to accpet string
        } else{
            printf("%s[%s]: %s\n", msg_struct.source, msg_struct.session, msg_struct.data);
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

int client_leave_session(int mySocket, char* buf){
    char msg[255] = {0};
    if (message_to_command(buf, msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        list_print();
        return -1;
    }

    //make sure the session is available
    if (list_delete(buf + strlen("/leavesession ")) == false){
        list_print();
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    printf("Successfully Left Session: %s\n", buf + strlen("/leavesession "));

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

int client_invite(int mySocket, char* buf){

    char msg[255] = {0};
    if (message_to_command(buf, msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    char* temp = strchr(buf + strlen("/invite "), ' ');
    char user[255] = {0};
    strncpy(user, buf + strlen("/invite "), temp - (buf + strlen("/invite ")));
    printf("Inviting user: %s to session: %s\n", user, temp + 1);
    return 0;
}


int client_text(int mySocket, char* buf){
    char session_name[1024] = {0};
    if (session_list->head != NULL && session_list->head->next == NULL){
        //if only one available session
        strcpy(session_name, session_list->head->session_name);
    } else{
        bool isInList = false;
        printf("Which session do you want to send to?\n");
        do{ //prompt the user to choose the destination session
            memset(session_name, 0, strlen(session_name));
            list_print();
            fgets(session_name, 1024, stdin);
            session_name[strlen(session_name) - 1] = 0;
            isInList = list_find(session_name);
            if (!isInList){
                printf("No matched session name. Please try again...\n");
            }
        } while(!isInList);
    }

    strcat(session_name, ":");
    strcat(session_name, buf);
    char msg[255] = {0};
    if (message_to_command(session_name, msg, curr_client_id) == false){
        printf("Wrong Input, Please try again...\n");
        return -1;
    }

    if (send(mySocket, msg, strlen(msg) + 1, 0) <= 0){
        syserror("send");
    }

    return 0;
}

//insert a session to the list
void list_insert(char* name){
    struct session* temp = session_list->head;
    if (temp == NULL){
        session_list->head = malloc(sizeof(struct session));
        session_list->head->next = NULL;
        strcpy(session_list->head->session_name, name);
        return;
    }
    while (temp->next != NULL){
        temp = temp->next;
    }
    temp->next = malloc(sizeof(struct session));
    temp->next->next = NULL;
    strcpy(temp->next->session_name, name);
    return;
}

//delete a session with specific name
bool list_delete(char* name){
    //if list is empty
    if (session_list->head == NULL){
        return false;
    }
    //if it is head
    if (strcmp(session_list->head->session_name, name) == 0){
        struct session* temp = session_list->head;
        session_list->head = session_list->head->next;
        free(temp);
        if (session_list->head == NULL){ //if no session in the list, set the flag to false
            isInSession = false;
        }
        return true;
    }
    //else
    struct session* temp = session_list->head;
    while (temp->next != NULL){
        if (strcmp(temp->next->session_name, name) == 0){
            break;
        }
        temp = temp->next;
    }
    if (temp->next == NULL){
        printf("No matched session...\n");
        return false;
    } 
    struct session* target = temp->next;
    temp->next = target->next;
    free(target);

    if (session_list->head == NULL){ //if no session in the list, set the flag to false
        isInSession = false;
    }

    return true;
}

//find whether there is an element with specific name in the list
bool list_find(char* name){
    struct session* temp = session_list->head;
    while (temp != NULL){
        if (strcmp(temp->session_name, name) == 0){
            return true;
        }
        temp = temp->next;
    }
    return false;
}

//print the whole list
void list_print(){
    struct session* temp = session_list->head;
    printf("\nAvailable sessions for you: \n");
    while (temp != NULL){
        printf("%s\n", temp->session_name);
        temp = temp->next;
    }
    printf("\n");
    return;
}