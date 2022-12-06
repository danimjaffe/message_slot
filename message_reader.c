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
*/
int main(int argc, char** argv)
{
	int fd;
    char* file_path;
	int val;
	char buffer[BUFFER_LEN];
	int tar_channel_id;

    //validate that the correct number of command line
	if (argc!= 3) 
    {
		perror("invalid number of arguments");
		exit(1);
	}
    
    file_path = argv[1];
    tar_channel_id = strtol(argv[2],NULL,10);
	fd = open(file_path, O_RDWR);

	if (fd<0) 
    {
        perror("fail open file");
        exit(1);

	}

	val = ioctl(fd, MSG_SLOT_CHANNEL, tar_channel_id);
	if (val < 0) 
    {
		perror("ioctl failed");
		exit(1);
	}

	val = read(fd, buffer, BUFFER_LEN);
	if (val < 0) {
		perror("fail read data");
		exit(1);
	}

	if (write(1, buffer, val) < val) 
    {
        perror("failed print");
		exit(1);
	}
	close(fd);
	return 0;
}