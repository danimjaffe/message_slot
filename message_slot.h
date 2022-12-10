#include <stddef.h>
#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H
#define MAJOR_NUM 235
#define BUFFER_LEN 128
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define DEVICE_NAME "device"

typedef struct node {
    struct node *next;
    char msg_content[BUFFER_LEN];
    int msg_len;
    unsigned long id;
} Channel;

typedef struct c_list {
    Channel *head;
    Channel *tail;
} Channel_List;

#endif 
