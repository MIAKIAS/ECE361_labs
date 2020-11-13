#pragma once
#include <string.h>

#define MAX_NAME 255
#define MAX_DATA 255
#define MAX_CLIENTS_NUMBER 10
#define MAX_PASSWORD 255

#define LOGIN 1
#define LO_ACK 2
#define LO_NAK 3
#define EXIT 4
#define JOIN 5
#define JN_ACK 6
#define JN_NAK 7
#define LEAVE_SESS 8
#define NEW_SESS 9
#define NS_ACK 10
#define MESSAGE 11
#define QUERY 12
#define QU_ACK 13

#define MAX_SESSIONS 10

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME]; 
    unsigned char data[MAX_DATA];
};

int login();
int logout();
int join_session();
int leave_session();
//int create_session(char *session_id, struct client_list session_clients[MAX_SESSIONS]);
int list();
int quit();
int send_message();
int command_to_message(char buf[255], struct message *msg);

int command_to_message(char buf[255], struct message *msg){
    //char *temp_str = "/login";
    if(strncmp(buf, "/login", strlen("/login")) == 0){

    }else if(strncmp(buf, "/logout", strlen("/logout"))){

    }else if(strncmp(buf, "/joinsession", strlen("/joinsession"))){

    }else if(strncmp(buf, "/leavesession", strlen("/leavesession"))){
        msg->type = LEAVE_SESS;
        msg->size = 0;
        strcpy(msg->data, "\0");
    }else if(strncmp(buf, "/createsession", strlen("/createsession"))){

    }else if(strncmp(buf, "/list", strlen("/list"))){

    }else if(strncmp(buf, "/quit", strlen("/quit"))){

    }else{

    }
    return 0;
}





