#pragma once
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_NAME 255
#define MAX_DATA 255
#define MAX_CLIENTS_NUMBER 3
#define MAX_PASSWORD 255
#define MAX_THREADS 999
#define DISCONNECT_INTERVAL 30000

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
#define INVITE 14
#define INVITE_ACK 15
#define INVITE_NAK 16

#define MAX_SESSIONS 10

extern pthread_mutex_t lock;

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME]; 
    unsigned char data[MAX_DATA];
    unsigned char session[MAX_NAME];
};

int login();
int logout();


//int create_session(char *session_id, struct client_list session_clients[MAX_SESSIONS]);

int quit();
int send_message();
int command_to_message(char* buf, struct message *msg);
bool message_to_command(char* buf, char* msg, char* curr_client_id);
void syserror(char* name);

void syserror(char* name){
    fprintf(stderr, "%s\n", name);
    exit(1);
}

//stransfer serialized message to struct message
bool message_to_command(char* buf, char* msg, char* curr_client_id){
    memset(msg, 0, sizeof(msg));
    if (strncmp(buf, "/login", strlen("/login")) == 0){
        char data[255] = {0};
        char* temp = strchr(buf + strlen("/login "), ' ');
        if (temp == NULL) return false;
        strncpy(data, buf + strlen("/login "), temp - (buf + strlen("/login ")));
        strcat(data, ",");
        if (strchr(temp + 1, ' ') == NULL) return false;
        strncat(data, temp + 1, strchr(temp + 1, ' ') - (temp + 1));
        sprintf(msg, "%d:%d:%s:%s", LOGIN, strlen(data), curr_client_id, data);
    } else if (strncmp(buf, "/logout", strlen("/logout")) == 0){
        sprintf(msg, "%d:%d:%s:%s", EXIT, 0, curr_client_id, "");
    } else if (strncmp(buf, "/joinsession", strlen("/joinsession")) == 0){
        sprintf(msg, "%d:%d:%s:%s", JOIN, strlen(buf) - strlen("/joinsession "), curr_client_id, buf + strlen("/joinsession "));
    } else if (strncmp(buf, "/leavesession", strlen("/leavesession")) == 0){
        if (strlen(buf) - strlen("/leavesession ") <= 0){
            printf("Please indicate which session you want to leave...\n");
            return false;
        }
        sprintf(msg, "%d:%d:%s:%s", LEAVE_SESS, strlen(buf) - strlen("/leavesession "), curr_client_id, buf + strlen("/leavesession "));
    } else if (strncmp(buf, "/createsession", strlen("/createsession")) == 0){
        sprintf(msg, "%d:%d:%s:%s", NEW_SESS, strlen(buf) - strlen("/createsession "), curr_client_id, buf + strlen("/createsession "));
    } else if (strncmp(buf, "/list", strlen("/list")) == 0){
        sprintf(msg, "%d:%d:%s:%s", QUERY, 0, curr_client_id, "");
    } else if (strncmp(buf, "/invite", strlen("/invite")) == 0){
        char session[255] = {0};
        char user[255] = {0};
        char* temp = strchr(buf + strlen("/invite "), ' ');
        if (temp == NULL) return false;
        strncpy(session, buf + strlen("/invite "), temp - (buf + strlen("/invite ")));
        strcpy(user, temp + 1);
        sprintf(msg, "%d:%d:%s:%s:%s", INVITE, strlen(session) + strlen(user), curr_client_id, session, user);
    } else if (strncmp(buf, "LO_ACK", strlen("LO_ACK")) == 0 && strcmp(curr_client_id, "N/A") == 0){
        sprintf(msg, "%d:%d:%s:%s", LO_ACK, 0, curr_client_id, "");
    } else if (strncmp(buf, "LO_NAK", strlen("LO_NAK")) == 0 && strcmp(curr_client_id, "N/A") == 0){
        sprintf(msg, "%d:%d:%s:%s", LO_NAK, strlen(buf) - strlen("LO_NAK: "), curr_client_id, buf + strlen("LO_NAK: "));
    } else if (strncmp(buf, "JN_ACK", strlen("JN_ACK")) == 0 && strcmp(curr_client_id, "N/A") == 0){
        sprintf(msg, "%d:%d:%s:%s", JN_ACK, strlen(buf) - strlen("JN_ACK: "), curr_client_id, buf + strlen("JN_ACK: "));
    } else if (strncmp(buf, "JN_NAK", strlen("JN_NAK")) == 0 && strcmp(curr_client_id, "N/A") == 0){
        sprintf(msg, "%d:%d:%s:%s", JN_NAK, strlen(buf) - strlen("JN_NAK: "), curr_client_id, buf + strlen("JN_NAK: "));
    } else if (strncmp(buf, "NS_ACK", strlen("NS_ACK")) == 0 && strcmp(curr_client_id, "N/A") == 0){
        sprintf(msg, "%d:%d:%s:%s", NS_ACK, strlen(buf) - strlen("NS_ACK: "), curr_client_id, buf + strlen("NS_ACK: "));
    } else if (strncmp(buf, "QU_ACK", strlen("QU_ACK")) == 0 && strcmp(curr_client_id, "N/A") == 0){
        sprintf(msg, "%d:%d:%s:%s", QU_ACK, strlen(buf) - strlen("QU_ACK: "), curr_client_id, buf + strlen("QU_ACK: "));
    } else {
        sprintf(msg, "%d:%d:%s:%s", MESSAGE, strlen(buf) - 1, curr_client_id, buf);
    }
    printf("Seriablization: %s\n", msg);
    return true;
}

//transfer input string to serialized message according to the protocol
int command_to_message(char* buf, struct message *msg){
    memset(msg->data, 0, sizeof(msg->data));
    memset(msg->source, 0, sizeof(msg->source));
    memset(msg->session, 0, sizeof(msg->session));
    msg->type = -1;
    msg->size = 0;
    if (strncmp(buf, "1:", strlen("1:")) == 0){ //LOGIN
        msg->type = LOGIN;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return LOGIN;
    } else if (strncmp(buf, "4:", strlen("4:")) == 0){
        msg->type = EXIT;
        msg->size = 0;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        strcpy(msg->data, "");
        return EXIT;
    } else if (strncmp(buf, "5:", strlen("5:")) == 0){
        msg->type = JOIN;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return JOIN;
    } else if (strncmp(buf, "8:", strlen("8:")) == 0){
        msg->type = LEAVE_SESS;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        msg->size = strlen(data_begin + 1);
        strcpy(msg->data, data_begin + 1);
        return LEAVE_SESS;
    } else if (strncmp(buf, "9:", strlen("9:")) == 0){
        msg->type = NEW_SESS;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return NEW_SESS;
    } else if (strncmp(buf, "12:", strlen("12:")) == 0){
        msg->type = QUERY;
        msg->size = 0;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        strcpy(msg->data, "");
        return QUERY;
    } else if (strncmp(buf, "2:", strlen("2:")) == 0){
        msg->type = LO_ACK;
        msg->size = 0;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        strcpy(msg->data, "");
        return LO_ACK;
    } else if (strncmp(buf, "3:", strlen("3:")) == 0){
        msg->type = LO_NAK;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return LO_NAK;
    } else if (strncmp(buf, "6:", strlen("6:")) == 0){
        msg->type = JN_ACK;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return JN_ACK;
    } else if (strncmp(buf, "7:", strlen("7:")) == 0){
        msg->type = JN_NAK;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return JN_NAK;
    } else if (strncmp(buf, "10:", strlen("10:")) == 0){
        msg->type = NS_ACK;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return NS_ACK;
    } else if (strncmp(buf, "13:", strlen("13:")) == 0){
        msg->type = QU_ACK;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* data_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, data_begin - colon - 1);
        data_begin++;
        msg->size = strlen(data_begin);
        strcpy(msg->data, data_begin);
        return QU_ACK;
    } else if (strncmp(buf, "14:", strlen("14:")) == 0){
        msg->type = INVITE;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* name_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, name_begin - colon - 1);
        colon = strchr(name_begin + 1, ':');
        strncpy(msg->session, name_begin + 1, colon - name_begin - 1);
        colon++;
        msg->size = strlen(colon);
        strcpy(msg->data, colon);
        printf("type: %d\nsize: %d\nsource: %s\ndata: %s\nsession: %s\n", msg->type, msg->size, msg->source, msg->data, msg->session);
        return INVITE;
    } else{
        msg->type = MESSAGE;
        char* colon = strchr(buf, ':');
        colon = strchr(colon + 1, ':');
        char* name_begin = strchr(colon + 1, ':');
        strncpy(msg->source, colon + 1, name_begin - colon - 1);
        colon = strchr(name_begin + 1, ':');
        strncpy(msg->session, name_begin + 1, colon - name_begin - 1);
        colon++;
        msg->size = strlen(colon);
        strcpy(msg->data, colon);
        printf("type: %d\nsize: %d\nsource: %s\ndata: %s\nsession: %s\n", msg->type, msg->size, msg->source, msg->data, msg->session);
        return MESSAGE;
    }

}





