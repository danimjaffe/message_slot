#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H
#define MAJOR_NUM 235
#define BUFFER_LEN 128
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define DEVICE_NAME "char_device"
#endif 
