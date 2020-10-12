#pragma once

struct packet{
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename;
    char filedata[1000];
};

//transfer packet data to string
char* p_to_s(struct packet* myPacket){
    char* result = (char*)malloc(sizeof(char) * 2000);
    memset(result, 0, sizeof(result));
    
    //copy first four fields
    char number[10000] = {0};
    sprintf(number, "%d", myPacket->total_frag);
    strcat(result, number);
    strcat(result, ":");
    sprintf(number, "%d", myPacket->frag_no);
    strcat(result, number);
    strcat(result, ":");
    sprintf(number, "%d", myPacket->size);
    strcat(result, number);
    strcat(result, ":");
    strcat(result, myPacket->filename);
    strcat(result, ":");
    //copy filedata one by one, incase of '\0' in middle
    int i = strlen(result);
    for (int j = 0; j < myPacket->size; ++j){
        result[i] = myPacket->filedata[j];
        ++i;
    }

    return result;
}

//transfer string message to packet
struct packet* s_to_p(char* buffer){
    //TBD
}