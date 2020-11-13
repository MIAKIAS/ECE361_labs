/*  ECE361 Lab4 server program
    Produced by Weizhou Wang and Adam Wu
    Some of the code are borrowed from "Beej's Guide to Network Programming"
    Input Format: server <TCP listen port>*/
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
#include <stdbool.h>
#include "common.h"
#include "client.h"

#define NUM_CONNECTIONS 8

struct session_info {
    char *session_id;
    int num_clients;
    //struct client clients[MAX_CLIENTS_NUMBER];
    struct client_id_list list;
    struct session_info *next;
};

struct session_queue {
    struct session_info *head;
};

//struct session_info sessions[MAX_SESSIONS];
struct session_queue session_q;


struct client clients[MAX_CLIENTS_NUMBER];
//struct client_list clients;
//clients.head = NULL;

//get session id 
//char *sessions[MAX_SESSIONS];

//struct client_list session_clients[MAX_SESSIONS];

int client_count = 0;
int session_count = 0;

void enque_session_queue(struct session_info *session){
    if(!session_q.head){
        session_q.head = session;
        return;
    }

    struct session_info *temp = session_q.head;

    while(temp->next != NULL){
        temp = temp->next;
    }
    
    temp->next = session;

    session_count ++;
}

void clear_session(struct session_info *session){
    if(session_q.head == session){
        session_q.head = session_q.head->next;
    }
    
    struct session_info *temp_session = session_q.head;

    while(temp_session->next != NULL){
        if(temp_session == session){
            break;
        }
        temp_session = temp_session->next;
    }
    temp_session->next = temp_session->next->next;

    //free current session

    free(session->session_id);
    session->next = NULL;

    struct client_id *temp_client_id = session->list.head;
    while(temp_client_id != NULL){
        struct client_id *temp_delete = temp_client_id;
        temp_client_id = temp_client_id->next;

        free(temp_delete->ID);
        free(temp_delete);
    }

    free(session);
    
}

struct session_info *find_session(char *session_id){
    struct session_info *temp = session_q.head;

    while(temp != NULL){
        if(strcmp(temp->session_id, session_id)){
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

void add_client_to_session(char *id, struct client_id_list *list){
    if(list->head == NULL){
        list->head = malloc(sizeof(struct client_id));
        list->head->next = NULL;
        strcpy(list->head->ID, id);
        return;
    }

    struct client_id *temp = list->head;
    while(temp->next != NULL){
        temp = temp->next;
    }

    temp = malloc(sizeof(struct client_id));
    temp->next = NULL;
    strcpy(temp->ID, id);
}

void delete_client_in_session(char *id, struct client_id_list *list){
    if(list->head == NULL){
        return;
    }

    if(strcmp(list->head->ID, id) == 0){
        struct client_id *temp_delete = list->head;
        list->head = list->head->next;
        free(temp_delete->ID);
        free(temp_delete);
        return;
    }

    struct client_id *temp = list->head;
    while(temp->next != NULL){
        if(strcmp(temp->next->ID, id) == 0){
            struct client_id *temp_delete = temp->next;
            temp->next = temp->next->next;
            free(temp_delete->ID);
            free(temp_delete);
            return;
        }
    }
}

int read_command();
void request_handler();


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
    hints.ai_socktype = SOCK_STREAM; // TCP sockets
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

    if(listen(mySocket, NUM_CONNECTIONS) == -1){
        syserror("listen");
    }

    struct sockaddr addr;
    socklen_t addrlen;

    session_q.head = NULL;

    int new_socket;
    while(1){
        new_socket = accept(mySocket, &addr, &addrlen);
        if(new_socket == -1){
            syserror("accept");
        }

        //check if new user socket already in user list
        //add_to_client_list(new_socket);

        request_handler();

        if(client_count <= 0){
            break;
        }
    }

    close(mySocket);
    freeaddrinfo(res);
    return 0;
}

void request_handler(){
    char buf[255];
    struct message msg;

    for(int i = 0; i < client_count; ++i){
        if(recv(clients[i].client_socket, buf, 255, 0) < 0){
            continue;
        }

        strcpy(msg.source, clients[i].ID);

        int type = command_to_message(buf, &msg);
        //should login in client.c to pass in the client socket
        if(type == LOGIN){
            ;
        }else if(type == EXIT){
            //logout();
        }else if(type == JOIN){
            int j;
            struct session_info *session_to_join = find_session(msg.data);

            if(session_to_join == NULL){
                //response JN_NAK
            }else{
                clients[i].in_session = true;
                clients[i].session_id = malloc(sizeof(char) * (strlen(msg.data) + 1));
                strcpy(clients[i].session_id, msg.data);

                //add to session_to_join->client_id_list
                add_client_to_session(clients[i].ID, &(session_to_join->list));
                session_to_join->num_clients ++;

                //response JN_ACK
            }
            

        }else if(type == LEAVE_SESS){
            char *curr_session_id = malloc(sizeof(char) * (strlen(clients[i].session_id) + 1));
            strcpy(curr_session_id, clients[i].session_id);
            
            free(clients[i].session_id);
            clients[i].in_session = false;

            struct session_info *temp_session = find_session(msg.data);
            
            //delete current client in the session
            delete_client_in_session(clients[i].ID, &(temp_session->list));
            temp_session->num_clients --;

            if(temp_session->num_clients <= 0){
                //remove session
                clear_session(temp_session);
                session_count --;
            }
        }else if(type == NEW_SESS){         
            struct session_info *new_session = malloc(sizeof(struct session_info));
            new_session->session_id = malloc(sizeof(char) * (strlen(msg.data) + 1));
            strcpy(new_session->session_id, msg.data);
            new_session->num_clients = 0;
            new_session->list.head = NULL;

            //enque to session queue
            enque_session_queue(new_session);
            //response NS_ACK

        }else if(type == MESSAGE){
            //multicase the message to all clients in the same session
        }else if(type == QUERY){

        }


    }
}

int read_command(){
    return 0;
}