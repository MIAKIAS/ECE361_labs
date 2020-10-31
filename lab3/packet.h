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

    sscanf(buffer, "%u:%u:%u:", &fragment->total_frag, &fragment->frag_no, &fragment->size);

    int count = 0;
    char *dataIndex = buffer;
    char *nameIndex = buffer;
    while(count < 4){
        if(*dataIndex == ':')    count ++;
        dataIndex ++;

        if(count < 3)   nameIndex ++;
        if(count == 4){
            nameIndex ++;
            break;
        }    
    }   

    int name_length = dataIndex - nameIndex - 1;
    fragment->filename = malloc(sizeof(char) * (name_length + 1));
    strncpy(fragment->filename, nameIndex, name_length);
    fragment->filename[name_length] = '\0';
    
    memset(fragment->filedata, 0, 1000);
    memcpy(fragment->filedata, dataIndex, sizeof(char) * fragment->size);
    fragment->filedata[fragment->size] = '\0';
   
    return fragment;
}

void clear_packet(struct packet *fragment){
    free(fragment->filename);
    free(fragment);
}