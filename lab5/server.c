/*  ECE361 Lab5 server program
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
#include <sys/time.h>
#include <fcntl.h>

#define NUM_CONNECTIONS 8

pthread_mutex_t client_list_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t session_queue_lock = PTHREAD_MUTEX_INITIALIZER;

//used to create thread for each client
pthread_t threads_client[MAX_THREADS];
int thread_index = 0;

struct session_info {
    char *session_id;
    int num_clients;
    struct client_index_list list;
    struct session_info *next;
};

struct session_queue {
    struct session_info *head;
};

struct session_queue session_q;

//initialize clients info
struct client clients[MAX_CLIENTS_NUMBER] = {
    {.ID = "test_id1", .password = "password1", .active = false, 
        .client_socket = -1, .in_session = false,  
        .s_list = {NULL}},
    {.ID = "test_id2", .password = "password2", .active = false, 
        .client_socket = -1, .in_session = false, 
        .s_list = {NULL}},
    {.ID = "test_id3", .password = "password3", .active = false, 
        .client_socket = -1, .in_session = false, 
        .s_list = {NULL}}
};

int client_count = 0;
int session_count = 0;

//debug purpose, used to print in list_sessions function
void print_session(struct session_info *session){
    if(session->num_clients <= 0){
        return;
    }
    printf("session: %s , users: ", session->session_id);
    struct client_index *temp = session->list.head;
    while(temp != NULL){
        printf("%s | ", clients[temp->index].ID);
        temp = temp->next;
    }
    printf("\n");
}

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
    }else{
        struct session_info *temp_session = session_q.head;   

        while(temp_session->next != NULL){
            if(temp_session->next == session){
                break;
            }
            temp_session = temp_session->next;
        }
        temp_session->next = temp_session->next->next;
    }

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

//add the client to the client list of a session
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
}

//add the session to the list of sessions in the client struct
void add_session_to_client(struct client *curr_client, char *session_id){
    curr_client->in_session = true;

    if(curr_client->s_list.head == NULL){
        curr_client->s_list.head = malloc(sizeof(struct list_entry));
        curr_client->s_list.head->next = NULL;
        curr_client->s_list.head->session_id = malloc(sizeof(char) * (strlen(session_id) + 1));
        strcpy(curr_client->s_list.head->session_id, session_id);
        return;
    }

    struct list_entry *temp_entry = curr_client->s_list.head;
    while(temp_entry->next != NULL){
        temp_entry = temp_entry->next;
    }

    temp_entry->next = malloc(sizeof(struct list_entry));
    temp_entry = temp_entry->next;
    temp_entry->next = NULL;
    temp_entry->session_id = malloc(sizeof(char) * (strlen(session_id) + 1));
    strcpy(temp_entry->session_id, session_id);
}

//delete a givin session from the list of sessions in the client struct
void delete_session_from_client(struct client *curr_client, char *session_id){
    struct list_entry *temp_delete;
    if(strcmp(curr_client->s_list.head->session_id, session_id) == 0){
        temp_delete = curr_client->s_list.head;
        curr_client->s_list.head = curr_client->s_list.head->next;

        free(temp_delete->session_id);
        free(temp_delete);
        
        if(curr_client->s_list.head == NULL){
            curr_client->in_session = false;
        }
        return;
    }

    struct list_entry *temp_entry = curr_client->s_list.head;

    while(temp_entry->next != NULL){
        if(strcmp(temp_entry->next->session_id, session_id) == 0)   break;  
    }
    temp_delete = temp_entry->next;
    temp_entry->next = temp_entry->next->next;

    free(temp_delete->session_id);
    free(temp_delete);
}

//clear all joined sessions in the client struct
void clear_session_list_of_client(struct client *curr_client){
    struct list_entry *temp_entry = curr_client->s_list.head;
    struct list_entry *temp_delete;
    while(temp_entry != NULL){
        temp_delete = temp_entry;
        temp_entry = temp_entry->next;

        free(temp_delete->session_id);
        free(temp_delete);
    }
    curr_client->in_session = false;
}

//delete a given client from the session
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

//delete a given client in all sessions that were joined; call
//when the client logs out
void delete_client_in_all_sessions(char *curr_client_id){
    //find index in client list
    int index = -1;
    
    for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
        if(strcmp(clients[i].ID, curr_client_id) == 0){
            index = i;
            break;
        }
    }
    
    if(index == -1){
        printf("Error: cannot find the client\n");
        return;
    }

    struct session_info *temp_session = session_q.head;
    struct client_index_list *list = &(temp_session->list);

    while(temp_session != NULL){
        bool found_client = false;
        if(list->head->index == index){
            found_client = true;
            struct client_index *temp_delete = list->head;
            list->head = list->head->next;
            free(temp_delete);
        }
        else{
            struct client_index *temp = list->head;
            while(temp->next != NULL){
                if(temp->next->index == index){
                    found_client = true;
                    struct client_index *temp_delete = temp->next;
                    temp->next = temp->next->next;
                    free(temp_delete);
                    break;
                }
            }
        }

        if(found_client){
            temp_session->num_clients --;
        }
  
        struct session_info *next_session = temp_session->next;

        if(temp_session->num_clients <= 0){
            //remove session
            clear_session(temp_session);
            session_count --;
        }

        temp_session = next_session;
        list = &(temp_session->list);
    }

}

//send list of users to the requesting client
void list_users(char* buf, int client_socket){
    strcpy(buf, "QU_ACK: \nOnline users:\n");
    for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
        if(clients[i].active){
            strcat(buf, clients[i].ID);

            if(clients[i].in_session){
                strcat(buf, ", in session: ");

                struct list_entry *temp_entry = clients[i].s_list.head;
                //append all sessions corresponding to the user to the back 
                while(temp_entry != NULL){
                    strcat(buf, "[");
                    strcat(buf, temp_entry->session_id);
                    strcat(buf, "] ");
                    temp_entry = temp_entry->next;
                }
            }
            strcat(buf, "\n");
        }
    }
}

//send list of sessions to the requesting client
void list_sessions(char* buf, int client_socket){

    strcat(buf, "\nAvailable sessions:\n");

    struct session_info *curr_session = session_q.head;
    
    //append all sessions
    while(curr_session != NULL){
        //debug print
        //print_session(curr_session);

        strcat(buf, curr_session->session_id);
        strcat(buf, "\n");

        curr_session = curr_session->next;
    }

    char message[255] = {0};
    message_to_command(buf, message, "N/A");
    if(send(client_socket, message, strlen(message), 0) < 0){
        syserror("send");
    }
}

void join_session(struct client *curr_client, int client_socket, char *session_id, int data_size, int type);
void leave_session(struct client *curr_client, char *session_id);
bool create_session(struct client *curr_client, char *session_id, int data_size);
void multicast_message(struct client *curr_client, char *message_data, char *session_id);
void invite(struct client *curr_client, char *client_to_invite, char *session_id, char *recv_buf);

void usage(){
	fprintf(stderr, "Incorrect inputs\n");
	exit(1);
}

void *thread_request(int *client_socket);

int main(int argc, char** argv){
    //first check input format
    if (argc != 2) usage(); 

    char* port = argv[1];

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

    //accept new client socket, create a thread for each client with the given socket
    while(1){
        int new_socket = accept(mySocket, &addr, &addrlen);
        if(new_socket == -1){
            syserror("accept");
        }

        int *new_socket_ptr = malloc(sizeof(int));
        *new_socket_ptr = new_socket;

        pthread_t *new_thread = &threads_client[thread_index++];
        pthread_create(new_thread, NULL, (void*)thread_request, new_socket_ptr);

    }

    close(mySocket);
    freeaddrinfo(res);
    return 0;
}


void *thread_request(int *client_socket){
    char recv_buf[255] = {0};
    struct message msg;
    bool logged_in = false;

    int type = -1;

    struct client *curr_client = NULL;

    //set timer
    time_t start, end;

    struct timeval timer;
    timer.tv_sec = DISCONNECT_INTERVAL;
    timer.tv_usec = 0;

    if (setsockopt(*client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timer, sizeof(timer)) < 0)
        syserror("set timer");

MAIN_LOOP:
    memset(recv_buf, 0, sizeof(recv_buf));
    
    time(&start);

    //when timeout, disconnect the client, force the client to log out
    if(recv(*client_socket, recv_buf, 255, 0) <= 0){     
        if(!logged_in){
            goto MAIN_LOOP;
        }   

        time(&end);

        double time_interval = end - start;
        if (time_interval >= DISCONNECT_INTERVAL){
            printf("Disconnect %s due to long time inactivity (%d sec).\n", curr_client->ID, time_interval);
            goto EXIT_HANDLER;
        } else{
            printf("Lost connectiton from client %s.\n", curr_client->ID);
            goto EXIT_HANDLER;
        }
    }

    
    type = command_to_message(recv_buf, &msg);

    if(type == LOGIN){
        //retrieve the client id and password from the client's input
        char *comma = strchr(msg.data, ',');
        char *curr_client_id = malloc(sizeof(char) * (comma - (char*)msg.data + 1));
        memset(curr_client_id, 0, sizeof(char) * (comma - (char*)msg.data + 1));
        strncpy(curr_client_id, msg.data, comma - (char*)msg.data);
        
        int len = comma - (char*)msg.data;
        char *curr_password = malloc(sizeof(char) * (strlen(msg.data) - len));
        memset(curr_password, 0, sizeof(char) * (strlen(msg.data) - len));
        strncpy(curr_password, comma + 1, strlen(comma + 1));

        //check if login is successful
        int i;
        for(i = 0; i < MAX_CLIENTS_NUMBER; ++i){
            if(strcmp(curr_client_id, clients[i].ID) == 0 && strcmp(curr_password, clients[i].password) == 0){
                
                pthread_mutex_lock(&client_list_lock);

                if(clients[i].active){
                    pthread_mutex_unlock(&client_list_lock);
                    char ack[255] = {0};
                    message_to_command("LO_NAK: the client has already logged in", ack, "N/A");
                    if(send(*client_socket, ack, strlen(ack) + 1, 0) < 0){
                        syserror("send");
                    }

                    free(curr_client_id);
                    free(curr_password);

                    goto OUT_LOOP;
                }
                
                clients[i].active = true;
                clients[i].client_socket = *client_socket;
                curr_client = &clients[i];

                char ack[255] = {0};
                message_to_command("LO_ACK", ack, "N/A");
                if(send(clients[i].client_socket, ack, strlen(ack) + 1, 0) < 0){
                    syserror("send");
                }
                printf("Client %s logged in.\n", clients[i].ID);

                logged_in = true;
                
                free(curr_client_id);
                free(curr_password);

                pthread_mutex_unlock(&client_list_lock);
                break;
            }
        }

        if(i == MAX_CLIENTS_NUMBER){
            char ack[255] = {0};
            message_to_command("LO_NAK: wrong id or wrong password", ack, "N/A");
            if(send(*client_socket, ack, strlen(ack) + 1, 0) < 0){
                syserror("send");
            }

            free(curr_client_id);
            free(curr_password);      
            goto OUT_LOOP;  
        }
        goto MAIN_LOOP;
    }

    else if(type == EXIT){
EXIT_HANDLER:
        pthread_mutex_lock(&client_list_lock);

        //if the client is in any session, clear theses sessions
        if(curr_client->in_session){
            clear_session_list_of_client(curr_client);

            pthread_mutex_lock(&session_queue_lock);
            delete_client_in_all_sessions(curr_client->ID);
            pthread_mutex_unlock(&session_queue_lock);
        }

        curr_client->active = false;
        curr_client->client_socket = -1;

        logged_in = false;

        pthread_mutex_unlock(&client_list_lock);
        printf("Client %s logged out.\n", curr_client->ID);
    }

    else if(type == JOIN){

        join_session(curr_client, curr_client->client_socket, msg.data, msg.size, msg.type);
              
        goto MAIN_LOOP;
    }

    else if(type == LEAVE_SESS){

        if(!curr_client->in_session){
            char ack[255] = {0};
            message_to_command("Not in any session", ack, "Error");
            if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
                syserror("send");
            }
            goto MAIN_LOOP;
        }

        if(curr_client->s_list.head->next == NULL){
            pthread_mutex_lock(&client_list_lock);
            leave_session(curr_client, curr_client->s_list.head->session_id);
            pthread_mutex_unlock(&client_list_lock);
        }else{
            pthread_mutex_lock(&client_list_lock);
            //to simplify implementation, in LEAVE_SESS we set the data field as the session name
            leave_session(curr_client, msg.data); 
            pthread_mutex_unlock(&client_list_lock);
        }

        goto MAIN_LOOP;
    }

    else if(type == NEW_SESS){
        bool create_success = create_session(curr_client, msg.data, msg.size);

        //joined the created session if the session is successfully created
        if(create_success && msg.size > 0){
            join_session(curr_client, curr_client->client_socket, msg.data, msg.size, msg.type);
        }

        goto MAIN_LOOP;
    }

    else if(type == QUERY){
        char buf[1024] = {0};
        pthread_mutex_lock(&client_list_lock);
        list_users(buf, curr_client->client_socket);
        pthread_mutex_unlock(&client_list_lock);

        pthread_mutex_lock(&session_queue_lock);
        list_sessions(buf, curr_client->client_socket);
        pthread_mutex_unlock(&session_queue_lock);
        goto MAIN_LOOP;
    }

    else if(type == MESSAGE){
        multicast_message(curr_client, recv_buf, msg.session);
        
        goto MAIN_LOOP;
    }
    
    else if(type == INVITE){

        //retrieve the id of the client to invite from the client's command
        int count = 0;
        int invite_index1 = 0;
        int invite_index2 = 0;
        bool flag1 = true;
    
        for(int i = 0; i < strlen(recv_buf); ++i){
            if(recv_buf[i] == ':'){
                count ++;
            }
            if(count == 3 && flag1){
                invite_index1 = i;
                flag1 = false;
            }else if(count == 4){
                invite_index2 = i;
                break;
            }
        }

        int len = invite_index2 - invite_index1 - 1;
        char *client_to_invite = malloc(sizeof(char) * (len + 1));
        strncpy(client_to_invite, recv_buf + invite_index1 + 1, len);
        client_to_invite[len] = '\0';
        printf("client to invite: %s\n", client_to_invite);

        //send invitation
        invite(curr_client, client_to_invite, msg.session, recv_buf);

        goto MAIN_LOOP;
    }
OUT_LOOP:
    close(*client_socket);
    free(client_socket);
    return NULL;
}

void join_session(struct client *curr_client, int client_socket, char *session_id, int data_size, int type){
    char buf[255];

    //check if the session id is givin
    if(data_size <= 0){
        if (type == JOIN){
            strcpy(buf, "JN_NAK: ");
            strcat(buf, "Session id not given");

            char ack[255] = {0};
            message_to_command(buf, ack, "N/A");
            if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
                syserror("send");
            }
        }
        
        return;
    }

    //check if the current client is already in this session
    if(curr_client->in_session){
        struct list_entry *temp_entry = curr_client->s_list.head;
        while(temp_entry != NULL){
            if(strcmp(temp_entry->session_id, session_id) == 0){
                strcpy(buf, "JN_NAK: ");
                strcat(buf, "Current client already in this session");

                char ack[255] = {0};
                message_to_command(buf, ack, "N/A");
                if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
                    syserror("send");
                }

                return;
            }
            temp_entry = temp_entry->next;
        }
    }
    
    //check if the session exist
    pthread_mutex_lock(&session_queue_lock);

    struct session_info *session_to_join = find_session(session_id);

    pthread_mutex_unlock(&session_queue_lock);

    if(session_to_join == NULL){
        //response JN_NAK
        strcpy(buf, "JN_NAK: ");
        strcat(buf, session_id);
        strcat(buf, ", ");
        strcat(buf, "wrong session id");

        char ack[255] = {0};
        message_to_command(buf, ack, "N/A");
        if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
            syserror("send");
        }
    }else{
        pthread_mutex_lock(&client_list_lock);

        add_session_to_client(curr_client, session_id);

        pthread_mutex_unlock(&client_list_lock);

        pthread_mutex_lock(&session_queue_lock);

        add_client_to_session(curr_client->ID, &(session_to_join->list));
        session_to_join->num_clients ++;

        pthread_mutex_unlock(&session_queue_lock);

        //response JN_ACK
        if(type == JOIN){
            strcpy(buf, "JN_ACK: ");
            strcat(buf, session_id);

            char ack[255] = {0};
            message_to_command(buf, ack, "N/A");
            if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
                syserror("send");
            }
            printf("Client %s joined session %s\n", curr_client->ID, session_id);
        }        
    }
}

void leave_session(struct client *curr_client, char *session_id){

    pthread_mutex_lock(&session_queue_lock);

    printf("Client %s left session %s\n", curr_client->ID, session_id);

    //check if the session exists
    struct session_info *temp_session = find_session(session_id);

    if(temp_session == NULL){
        printf("Fail to leave session: No matched session\n");
        pthread_mutex_unlock(&session_queue_lock);
        return;
    }

    //check if session is joined by current user
    bool flag = false;
    struct list_entry *temp_entry = curr_client->s_list.head;
    while(temp_entry != NULL){
        if(strcmp(temp_entry->session_id, session_id) == 0){
            flag = true;
            break;
        }
        temp_entry = temp_entry->next;
    }
    
    if(!flag){
        printf("Fail to leave session: The session is not joined by the current user\n");
        pthread_mutex_unlock(&session_queue_lock);
        return;
    }

    //delete current client in the session
    delete_client_in_session(curr_client->ID, &(temp_session->list));
    temp_session->num_clients --;

    if(temp_session->num_clients <= 0){
        //remove session
        clear_session(temp_session);
        session_count --;
    }

    pthread_mutex_unlock(&session_queue_lock);

    delete_session_from_client(curr_client, session_id);
}

bool create_session(struct client *curr_client, char *session_id, int data_size){
    char buf[255];

    //check if the session id is given
    if(data_size <= 0){
        strcpy(buf, "JN_NAK: ");
        strcat(buf, "Session id not given");

        char ack[255] = {0};
        message_to_command(buf, ack, "N/A");
        if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
            syserror("send");
        }
        return false;
    }
    
    //check if the session already exists
    pthread_mutex_lock(&session_queue_lock);
    struct session_info *session_to_create = find_session(session_id);
    pthread_mutex_unlock(&session_queue_lock);

    if(session_to_create != NULL){
        //response JN_NAK
        strcpy(buf, "JN_NAK: ");
        strcat(buf, session_id);
        strcat(buf, ", ");
        strcat(buf, "wrong session id");

        char ack[255] = {0};
        message_to_command(buf, ack, "N/A");
        if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
            syserror("send");
        }
        return false;
    }
    
    //create a new session, and add it to session queue
    struct session_info *new_session = malloc(sizeof(struct session_info));
    new_session->session_id = malloc(sizeof(char) * (strlen(session_id) + 1));
    strcpy(new_session->session_id, session_id);
    new_session->num_clients = 0;
    new_session->list.head = NULL;
    new_session->next = NULL;

    pthread_mutex_lock(&session_queue_lock);
    enque_session_queue(new_session);
    pthread_mutex_unlock(&session_queue_lock);

    //response NS_ACK
    strcpy(buf, "NS_ACK: ");
    strcat(buf, session_id);
    char ack[255] = {0};
    message_to_command(buf, ack, "N/A");
    if(send(curr_client->client_socket, ack, strlen(ack) + 1, 0) < 0){
        syserror("send");
    }
    printf("Client %s created and joined session %s\n", curr_client->ID, session_id);
    return true;
}

void multicast_message(struct client *curr_client, char *message_data, char *session_id){
    printf("Multicasting message: %s\n", message_data);

    pthread_mutex_lock(&session_queue_lock);
    
    struct session_info *session_to_send = find_session(session_id);
    if(session_to_send == NULL){
        printf("No matched session\n");
        pthread_mutex_unlock(&session_queue_lock);
        return;
    }

    //check if session is joined by current user
    bool flag = false;
    struct list_entry *temp_entry = curr_client->s_list.head;
    while(temp_entry != NULL){
        if(strcmp(temp_entry->session_id, session_id) == 0){
            flag = true;
            break;
        }
        temp_entry = temp_entry->next;
    }
    
    if(!flag){
        printf("The session is not joined by the current user, no permission to send message\n");
        pthread_mutex_unlock(&session_queue_lock);
        return;
    }

    //send the message to all active clients in the session
    struct client_index *temp = session_to_send->list.head;

    while(temp != NULL){        
        if(send(clients[temp->index].client_socket, message_data, strlen(message_data), 0) < 0){
            syserror("send");
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&session_queue_lock);
}

void invite(struct client *curr_client, char *client_to_invite, char *session_id, char *recv_buf){
    //check client_to_invite exists and has logged in
    bool client_active = false;
    int index = -1;
    pthread_mutex_lock(&client_list_lock);
    for(int i = 0; i < MAX_CLIENTS_NUMBER; ++i){
        if(strcmp(client_to_invite, clients[i].ID) == 0 && clients[i].active){
            client_active = true;
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&client_list_lock);

    if(!client_active){
        printf("Fail to invite: The client to invite does not exist or is not active\n");
        return;
    }

    //check if the session exists
    pthread_mutex_lock(&session_queue_lock);

    struct session_info *session_to_send = find_session(session_id);
    if(session_to_send == NULL){
        printf("Fail to invite: No matched session\n");
        pthread_mutex_unlock(&session_queue_lock);
        return;
    }

    //check current client has joined the session
    bool curr_client_flag = false;
    struct list_entry *temp_entry = curr_client->s_list.head;
    while(temp_entry != NULL){
        if(strcmp(temp_entry->session_id, session_id) == 0){
            curr_client_flag = true;
            break;
        }
        temp_entry = temp_entry->next;
    }
    
    if(!curr_client_flag){
        printf("Fail to invite: The session is not joined by the current user\n");
        pthread_mutex_unlock(&session_queue_lock);
        return;
    }

    //check if client_to_invite already in the session
    bool invite_client_flag = false;
    struct client_index *temp_index = session_to_send->list.head;
    
    while(temp_index != NULL){
        if(temp_index->index == index){
            printf("Fail to invite: the client to invite already in the session\n");
            pthread_mutex_unlock(&session_queue_lock);
            return;
        }
        temp_index = temp_index->next;
    }

    pthread_mutex_unlock(&session_queue_lock);

    //send invitation 
    if(send(clients[index].client_socket, recv_buf, strlen(recv_buf), 0) < 0){
        syserror("send");
    }
}