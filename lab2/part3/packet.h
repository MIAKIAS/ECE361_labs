#pragma once
#include <string.h>

struct packet{
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

//transfer packet data to string
//return string transfered from the packet
//also set the string length(include '\0') to size
char* p_to_s(struct packet* myPacket, int* size){
    char* result = (char*)malloc(sizeof(char) * 2000);
    memset(result, 0, 2000);
    
    //copy first four fields
    char number[10000] = {0};
    sprintf(number, "%d", myPacket->total_frag);
    strcat(result, number);
    strcat(result, ":");
    memset(number, 0, sizeof(number));
    sprintf(number, "%d", myPacket->frag_no);
    strcat(result, number);
    strcat(result, ":");
    memset(number, 0, sizeof(number));
    sprintf(number, "%d", myPacket->size);
    strcat(result, number);
    strcat(result, ":");
    strcat(result, myPacket->filename);
    strcat(result, ":");
    //set the temporary size
    *size = strlen(result);
    //copy filedata one by one, incase of '\0' in middle
    int i = strlen(result);
    for (int j = 0; j < myPacket->size; ++j){
        result[i] = myPacket->filedata[j];
        ++i;
    }
    *size += myPacket->size;
    return result;
}

//transfer string message to packet
struct packet* s_to_p(char* buffer){
    printf("buffer: %s\n", buffer);
    
    struct packet *fragment = malloc(sizeof(struct packet));

    char temp_name[1024];
    char *temp_ptr;

    sscanf(buffer, "%u:%u:%u:%s:", &fragment->total_frag, &fragment->frag_no, &fragment->size, temp_name);
 
    temp_ptr = strchr(temp_name, ':');
    fragment->filename = malloc(sizeof(char) * (temp_ptr - temp_name + 1));
    strncpy(fragment->filename, temp_name, temp_ptr - temp_name);
    fragment->filename[temp_ptr - temp_name] = '\0';

    printf("fragment->filename: %s\n", fragment->filename);
    printf("fragment->size: %u\n", fragment->size);
    
    memset(fragment->filedata, 0, 1000);
    memcpy(fragment->filedata, temp_ptr + 1, sizeof(char) * fragment->size);
    printf("fragment->filedata: %s\n", fragment->filedata);
    
    return fragment;
}

void clear_packet(struct packet *fragment){
    free(fragment->filename);
    free(fragment);
}