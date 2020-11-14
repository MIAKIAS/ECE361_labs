#pragma once
#include <string.h>
#include <pthread.h>

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
#define LOGOUT 14

#define MAX_SESSIONS 10

extern pthread_mutex_t lock;

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
int command_to_message(char* buf, struct message *msg);
void syserror(char* name);

void syserror(char* name){
    fprintf(stderr, "%s\n", name);
    exit(1);
}


int command_to_message(char* buf, struct message *msg){
    if(strncmp(buf, "/login", strlen("/login")) == 0){

        msg->type = LOGIN;
        msg->size = strlen(buf) - strlen("/login "); 
        strcpy(msg->data, buf + strlen("/login "));

        if (msg->size == 0){
            return -1;
        }

        //get the user ID
        for (char* i = buf + strlen("/login "); *i != '\0'; i++){
            if (*i == ' '){
                strncpy(msg->source, buf + strlen("/login "), i - (buf + strlen("/login ") - 1));
                break;
            }
            if (*(i + 1) == '\0'){
                return -1;
            }
        }
        
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/login", msg->size, msg->source, msg->data);

        return LOGIN;

    }else if(strncmp(buf, "/logout", strlen("/logout")) == 0){

        msg->type = LOGOUT;
        msg->size = 0;
        strcpy(msg->data, "");
        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/logout", msg->size, msg->source, msg->data);

        return LOGOUT;

    }else if(strncmp(buf, "/joinsession", strlen("/joinsession")) == 0){

        msg->type = JOIN;
        msg->size = strlen(buf) - strlen("/joinsession ");
        strcpy(msg->data, buf + strlen("/joinsession "));

        if (msg->size == 0){
            return -1;
        }

        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/joinsession", msg->size, msg->source, msg->data);

        return JOIN;

    }else if(strncmp(buf, "/leavesession", strlen("/leavesession")) == 0){

        msg->type = LEAVE_SESS;
        msg->size = 0;
        strcpy(msg->data, "");
        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/leavesession", msg->size, msg->source, msg->data);

        return LEAVE_SESS;

    }else if(strncmp(buf, "/createsession", strlen("/createsession")) == 0){

        msg->type = NEW_SESS;
        msg->size = strlen(buf) - strlen("/createsession ");
        strcpy(msg->data, buf + strlen("/createsession "));

        if (msg->size == 0){
            return -1;
        }

        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/createsession", msg->size, msg->source, msg->data);

        return NEW_SESS;

    }else if(strncmp(buf, "/list", strlen("/list")) == 0){

        msg->type = QUERY;
        msg->size = 0;
        strcpy(msg->data, "");
        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/list", msg->size, msg->source, msg->data);

        return QUERY;

    }else if(strncmp(buf, "/quit", strlen("/quit")) == 0){

        msg->type = EXIT;
        msg->size = 0;
        strcpy(msg->data, "");
        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "/quit", msg->size, msg->source, msg->data);

        return EXIT;

    } else {
        msg->type = MESSAGE;
        msg->size = strlen(buf);
        strcpy(msg->data, buf);
        //leave the USER ID Later
        printf("Type: %s\nSize: %d\nSource: %s\nData: %s\n",
        "message", msg->size, msg->source, msg->data);
        return MESSAGE;
    }
        
    return -1;

}





