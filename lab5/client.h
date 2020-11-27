#pragma once

#include "common.h"
#include <stdbool.h>

struct client_session_list {
    struct list_entry *head;
};

struct list_entry {
    char *session_id;
    struct list_entry *next;
};

struct client {
    char ID[MAX_NAME];
    char password[MAX_PASSWORD];
    bool active;
    int client_socket;
    //struct client *next;
    bool in_session;
    //char *session_id;
    struct client_session_list s_list;
};


// struct client_list {
//     struct client *head;
// };

// struct client_id {
//     char ID[MAX_NAME];
//     struct client_id *next;
// };

// struct client_id_list {
//     struct client_id *head;
// };

struct client_index {
    int index;
    struct client_index *next;
};

struct client_index_list {
    struct client_index *head;
};