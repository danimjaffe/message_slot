#include "message_slot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/ioctl.h> 
#include <linux/ioctl.h>

/*
argv[1]: message slot file path.
argv[2]: the target message channel id. Assume a non-negative integer.
argv[3]: the message to pass.
*/

int main(int argc, char** argv)
{
	int fd;
	int val;
    char* file_path;
	int tar_channel_id; 
    char* msg_to_pass;
    //validate that the correct number of command line
	if (argc != 4) 
    {
		perror("invalid number of args");
		exit(1);
    }

    file_path = argv[1];
    tar_channel_id = strtol(argv[2],NULL,10);
    msg_to_pass = argv[3];
	fd = open(file_path, O_RDWR);

	if (fd < 0) 
    {
		perror("FD failed");
		exit(1);
	}

	val = ioctl(fd, MSG_SLOT_CHANNEL, tar_channel_id);
	if (val < 0) 
    {
		perror("ioctl failed");
		exit(1);
	}
	val = write(fd, msg_to_pass, strlen(msg_to_pass));
	
    if (val != strlen(msg_to_pass))
    {
		perror("write failed.");
		exit(1);
	}
	close(fd);
	return 0;
}