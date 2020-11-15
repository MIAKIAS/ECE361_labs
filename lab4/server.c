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

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; //no use

pthread_mutex_t client_count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t session_count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t session_queue_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t threads_client[MAX_THREADS];
int thread_index = 0;

struct session_info {
    char *session_id;
    int num_clients;
    //struct client clients[MAX_CLIENTS_NUMBER];
    struct client_index_list list;
    struct session_info *next;
};

struct session_queue {
    struct session_info *head;
};

//struct session_info sessions[MAX_SESSIONS];
struct session_queue session_q;


struct client clients[MAX_CLIENTS_NUMBER] = {
    {.ID = "test_id1", .password = "password1", .active = false, 
        .client_socket = -1, .in_session = false, .session_id = NULL},
    {.ID = "test_id2", .password = "password2", .active = false, 
        .client_socket = -1, .in_session = false, .session_id = NULL},
    {.ID = "test_id3", .password = "password3", .active = false, 
        .client_socket = -1, .in_session = false, .session_id = NULL}
};
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
        if(temp_session->next == session){
            break;
        }
        temp_session = temp_session->next;
    }
    temp_session->next = temp_session->next->next;

    //free current session

    free(session->session_id);
    session->next = NULL;

    struct client_index *temp_client_index = session->list.head;
    while(temp_client_index != NULL){
        struct client_index *temp_delete = temp_client_index;
        temp_client_index = temp_client_index->next;

        free(temp_delete);
    }

    free(session);
    
}

struct session_info *find_session(char *session_id){
    struct session_info *temp = session_q.head;

    while(temp != NULL){
        if(strcmp(temp->session_id, session_id) == 0){
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

void add_client_to_session(char *id, struct client_index_list *list){
    //find index in client list
    int index = -1;
    
    for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
        if(strcmp(clients[i].ID, id) == 0){
            index = i;
            break;
        }
    }
    
    if(index == -1){
        printf("Error: cannot find the client\n");
        return;
    }

    if(list->head == NULL){
        list->head = malloc(sizeof(struct client_index));
        list->head->next = NULL;
        list->head->index = index;
        return;
    }

    struct client_index *temp = list->head;
    while(temp->next != NULL){
        temp = temp->next;
    }

    temp->next = malloc(sizeof(struct client_index));
    temp->next->index = index;
    temp->next->next = NULL;
    // temp->index = index;
    // temp->next = NULL;
}

void delete_client_in_session(char *id, struct client_index_list *list){
    //find index in client list
    int index = -1;
    
    for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
        if(strcmp(clients[i].ID, id) == 0){
            index = i;
            break;
        }
    }
    
    if(index == -1){
        printf("Error: cannot find the client\n");
        return;
    }

    if(list->head == NULL){
        return;
    }

    if(list->head->index == index){
        struct client_index *temp_delete = list->head;
        list->head = list->head->next;
        free(temp_delete);
        return;
    }

    struct client_index *temp = list->head;
    while(temp->next != NULL){
        if(temp->next->index == index){
            struct client_index *temp_delete = temp->next;
            temp->next = temp->next->next;
            free(temp_delete);
            return;
        }
    }
}

//send list of users 
void list_users(int client_socket){
    if(send(client_socket, "QU_ACK: Online users:\n", strlen("QU_ACK: Online users:\n"), 0) < 0){
        syserror("send");
    }
    //printf("Online users:\n");

    for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
        if(clients[i].active){
            //printf("%s\n", clients[i].ID);
            char buf[255] = {0};
            strcpy(buf, clients[i].ID);
            strcat(buf, "\n");

            if(send(client_socket, buf, strlen(buf), 0) < 0){
                syserror("send");
            }
        }
    }
}

//send list of sessions
void list_sessions(int client_socket){
    if(send(client_socket, "Available sessions:\n", strlen("Available sessions:\n"), 0) < 0){
        syserror("send");
    }

    //printf("Available sessions:\n");

    struct session_info *curr_session = session_q.head;

    while(curr_session != NULL){
        //printf("%s\n", curr_session->session_id);
        char session_packet[255] = {0};
        strcpy(session_packet, curr_session->session_id);
        strcat(session_packet, "\n");

        if(send(client_socket, session_packet, strlen(session_packet), 0) < 0){
            syserror("send");
        }
        curr_session = curr_session->next;
    }
}


//void request_handler();

void join_session(struct client *curr_client, int client_socket, char *session_id, int data_size);
void leave_session(struct client *curr_client);
void create_session(struct client *curr_client, char *session_id, int data_size);
void multicast_message(struct client *curr_client, char *message_data);

void usage(){
	fprintf(stderr, "Incorrect inputs\n");
	exit(1);
}

void *thread_request(int *client_socket);

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

    while(1){
        int new_socket = accept(mySocket, &addr, &addrlen);
        if(new_socket == -1){
            syserror("accept");
        }

        int *new_socket_ptr = malloc(sizeof(int));
        *new_socket_ptr = new_socket;

        pthread_t *new_thread = &threads_client[thread_index++];
        pthread_create(new_thread, NULL, (void*)thread_request, new_socket_ptr);

        //check if new user socket already in user list
        //add_to_client_list(new_socket);

        //request_handler();

        // if(client_count <= 0){
        //     break;
        // }
    }

    close(mySocket);
    freeaddrinfo(res);
    return 0;
}

// void request_handler(){
//     char buf[255];
//     struct message msg;

//     for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
//         if(recv(clients[i].client_socket, buf, 255, 0) < 0){
//             continue;
//         }

//         strcpy(msg.source, clients[i].ID);

//         int type = command_to_message(buf, &msg);
//         //should login in client.c to pass in the client socket
//         if(type == LOGIN){
//             ;
//         }else if(type == EXIT){
//             //quit session
//             //set current client to inactive
//         }else if(type == JOIN){
//             int j;
//             struct session_info *session_to_join = find_session(msg.data);

//             if(session_to_join == NULL){
//                 //response JN_NAK
//                 if(send(clients[i].client_socket, "JN_NAK", strlen("JN_NAK"), 0) < 0){
//                     syserror("send");
//                 }
//             }else{
//                 clients[i].in_session = true;
//                 clients[i].session_id = malloc(sizeof(char) * (strlen(msg.data) + 1));
//                 strcpy(clients[i].session_id, msg.data);

//                 //add to session_to_join->client_id_list
//                 add_client_to_session(clients[i].ID, &(session_to_join->list));
//                 session_to_join->num_clients ++;

//                 //response JN_ACK
//                 if(send(clients[i].client_socket, "JN_ACK", strlen("JN_ACK"), 0) < 0){
//                     syserror("send");
//                 }
//             }
            

//         }else if(type == LEAVE_SESS){
//             char *curr_session_id = malloc(sizeof(char) * (strlen(clients[i].session_id) + 1));
//             strcpy(curr_session_id, clients[i].session_id);
            
//             free(clients[i].session_id);
//             clients[i].in_session = false;

//             struct session_info *temp_session = find_session(msg.data);
            
//             //delete current client in the session
//             delete_client_in_session(clients[i].ID, &(temp_session->list));
//             temp_session->num_clients --;

//             if(temp_session->num_clients <= 0){
//                 //remove session
//                 clear_session(temp_session);
//                 session_count --;
//             }
//         }else if(type == NEW_SESS){         
//             struct session_info *new_session = malloc(sizeof(struct session_info));
//             new_session->session_id = malloc(sizeof(char) * (strlen(msg.data) + 1));
//             strcpy(new_session->session_id, msg.data);
//             new_session->num_clients = 0;
//             new_session->list.head = NULL;

//             //enque to session queue
//             enque_session_queue(new_session);
//             //response NS_ACK
//             if(send(clients[i].client_socket, "NS_ACK", strlen("NS_ACK"), 0) < 0){
//                 syserror("send");
//             }

//         }else if(type == MESSAGE){
//             //multicase the message to all clients in the same session
//         }else if(type == QUERY){
//             list_users(clients[i].client_socket);
//             list_sessions(clients[i].client_socket);
//         }


//     }
// }



void *thread_request(int *client_socket){
    char buf[255] = {0};
    struct message msg;
    bool logged_in = false;

    int type = -1;

    struct client *curr_client = NULL;

MAIN_LOOP:
    memset(buf, 0, sizeof(buf));
    if(recv(*client_socket, buf, 255, 0) < 0){
        syserror("recv");
        return NULL;
    }

    type = command_to_message(buf, &msg);
    if (curr_client != NULL){
        strcpy(msg.source, curr_client->ID);   
        printf("Source: %s\n", msg.source);
    } 

    printf("Command Received: %s\n", buf);

    if(type == LOGIN){

        //handle login request
        char *comma = strchr(msg.data, ' ');
        char *curr_client_id = malloc(sizeof(char) * (comma - (char*)msg.data));
        memset(curr_client_id, 0, sizeof(*curr_client_id));
        strncpy(curr_client_id, msg.data, comma - (char*)msg.data);
        
        int len = comma - (char*)msg.data;
        char *curr_password = malloc(sizeof(char) * (strlen(msg.data) - len - 1));
        memset(curr_password, 0, sizeof(*curr_password));
        char *comma_next = strchr(comma + 1, ' ');
        strncpy(curr_password, comma + 1, comma_next - comma - 1);

        int i;
        for(i = 0; i < MAX_CLIENTS_NUMBER; ++i){
            if(strcmp(curr_client_id, clients[i].ID) == 0 && strcmp(curr_password, clients[i].password) == 0){
                
                pthread_mutex_lock(&client_list_lock);

                if(clients[i].active){
                    pthread_mutex_unlock(&client_list_lock);
                    if(send(*client_socket, "LO_NAK: the client has already logged in", 
                            strlen("LO_NAK: the client has already logged in"), 0) < 0){
                        syserror("send");
                    }

                    free(curr_client_id);
                    free(curr_password);

                    goto MAIN_LOOP;
                }
                
                clients[i].active = true;
                clients[i].client_socket = *client_socket;
                curr_client = &clients[i];

                if(send(clients[i].client_socket, "LO_ACK", strlen("LO_ACK"), 0) < 0){
                    syserror("send");
                }

                logged_in = true;
                
                free(curr_client_id);
                free(curr_password);

                pthread_mutex_unlock(&client_list_lock);
                break;
            }
        }

        if(i == MAX_CLIENTS_NUMBER){
            if(send(*client_socket, "LO_NAK: wrong id or wrong password", 
                    strlen("LO_NAK: wrong id or wrong password"), 0) < 0){
                syserror("send");
            }

            free(curr_client_id);
            free(curr_password);          
        }
        goto MAIN_LOOP;
    }

    if(type == EXIT){
        pthread_mutex_lock(&client_list_lock);

        if(curr_client->in_session){
            leave_session(curr_client);
            // if(send(curr_client->client_socket, "Cannot log out: should leave session first", 
            //         strlen("Cannot log out: should leave session first"), 0) < 0){
            //     syserror("send");
            // }
            
            pthread_mutex_unlock(&client_list_lock);
            //goto MAIN_LOOP;
        }

        curr_client->active = false;
        curr_client->client_socket = -1;
        logged_in = false;

        pthread_mutex_unlock(&client_list_lock);
        printf("Client %s logged out.\n", curr_client->ID);
        //goto MAIN_LOOP;
    }

    else if(type == JOIN){
        join_session(curr_client, curr_client->client_socket, msg.data, msg.size);
              
        goto MAIN_LOOP;
    }

    else if(type == LEAVE_SESS){
        if(!curr_client->in_session){
            if(send(curr_client->client_socket, "Not in any session", strlen("Not in any session"), 0) < 0){
                syserror("send");
            }
            goto MAIN_LOOP;
        }
        leave_session(curr_client);
        goto MAIN_LOOP;
    }

    else if(type == NEW_SESS){
        create_session(curr_client, msg.data, msg.size);

        if(!curr_client->in_session && msg.size > 0){
            join_session(curr_client, curr_client->client_socket, msg.data, msg.size);
        }

        goto MAIN_LOOP;
    }

    else if(type == QUERY){
        pthread_mutex_lock(&client_list_lock);
        list_users(curr_client->client_socket);
        pthread_mutex_unlock(&client_list_lock);

        pthread_mutex_lock(&session_queue_lock);
        list_sessions(curr_client->client_socket);
        pthread_mutex_unlock(&session_queue_lock);
        goto MAIN_LOOP;
    }

    else if(type == MESSAGE){
        printf("Should multicast this message\n");

        if(curr_client->in_session){
            multicast_message(curr_client, msg.data);
        }else{
            printf("Current client is not in any session\n");
        }
        
        goto MAIN_LOOP;
    }
    
    //printf("out of loop\n");
    close(*client_socket);
    free(client_socket);
    return NULL;
}

void join_session(struct client *curr_client, int client_socket, char *session_id, int data_size){
    char buf[255];

    if(data_size < 0){
        strcpy(buf, "JN_NAK: ");
        strcat(buf, "Session id not given");

        if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
            syserror("send");
        }
        return;
    }

    if(curr_client->in_session){
        strcpy(buf, "JN_NAK: ");
        strcat(buf, "Current client already in a session");

        if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
            syserror("send");
        }
        return;
    }

    pthread_mutex_lock(&session_queue_lock);

    struct session_info *session_to_join = find_session(session_id);

    pthread_mutex_unlock(&session_queue_lock);

    if(session_to_join == NULL){
        //response JN_NAK
        strcpy(buf, "JN_NAK: ");
        strcat(buf, "wrong session id");

        if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
            syserror("send");
        }
    }else{
        pthread_mutex_lock(&client_list_lock);

        curr_client->in_session = true;
        curr_client->session_id = malloc(sizeof(char) * (strlen(session_id) + 1));
        strcpy(curr_client->session_id, session_id);

        pthread_mutex_unlock(&client_list_lock);

        pthread_mutex_lock(&session_queue_lock);

        //add to session_to_join->client_id_list
        add_client_to_session(curr_client->ID, &(session_to_join->list));
        session_to_join->num_clients ++;

        pthread_mutex_unlock(&session_queue_lock);

        //response JN_ACK
        strcpy(buf, "JN_ACK: ");
        strcat(buf, session_id);

        if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
            syserror("send");
        }
    }
}

void leave_session(struct client *curr_client){

    pthread_mutex_lock(&session_queue_lock);

    struct session_info *temp_session = find_session(curr_client->session_id);
    //delete current client in the session
    delete_client_in_session(curr_client->ID, &(temp_session->list));
    temp_session->num_clients --;
    printf("here1\n");

    if(temp_session->num_clients <= 0){
        //remove session
        clear_session(temp_session);
        session_count --;
    }
    printf("here2\n");
    pthread_mutex_unlock(&session_queue_lock);

    pthread_mutex_lock(&client_list_lock);

    free(curr_client->session_id);
    curr_client->in_session = false;
    printf("here3\n");

    pthread_mutex_unlock(&client_list_lock);
}

void create_session(struct client *curr_client, char *session_id, int data_size){
    char buf[255];

    if(data_size < 0){
        strcpy(buf, "JN_NAK: ");
        strcat(buf, "Session id not given");

        if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
            syserror("send");
        }
        return;
    }
    
    if(curr_client->in_session){
        strcpy(buf, "JN_NAK: ");
        strcat(buf, "Current client already in a session");

        if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
            syserror("send");
        }
    
        return;
    }

    struct session_info *new_session = malloc(sizeof(struct session_info));
    new_session->session_id = malloc(sizeof(char) * (strlen(session_id) + 1));
    strcpy(new_session->session_id, session_id);
    new_session->num_clients = 0;
    new_session->list.head = NULL;

    pthread_mutex_lock(&session_queue_lock);
    //enque to session queue
    enque_session_queue(new_session);
    pthread_mutex_unlock(&session_queue_lock);

    //response NS_ACK
    
    strcpy(buf, "NS_ACK: ");
    strcat(buf, session_id);
    if(send(curr_client->client_socket, buf, strlen(buf), 0) < 0){
        syserror("send");
    }
}

void multicast_message(struct client *curr_client, char *message_data){
    pthread_mutex_lock(&session_queue_lock);
    struct session_info *session_to_send = find_session(curr_client->session_id);

    struct client_index *temp = session_to_send->list.head;

    while(temp != NULL){
        //printf("%d\n", temp->index);

        if(send(clients[temp->index].client_socket, message_data, strlen(message_data), 0) < 0){
            syserror("send");
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&session_queue_lock);
}