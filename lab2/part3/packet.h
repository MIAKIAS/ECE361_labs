#pragma once

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
    //TBD
}