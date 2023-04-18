#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "process_ioctl.h"

int fd;

int leave = 0;

int idx;

void * send(void *inp) 
{
	char *message;
	message = (char *)malloc(64);

	while(!leave)
	{
		char rem[64];
		int length = -1;

		memset(message, 0, 64);

		message[0] = idx;

		printf("> ");
		fgets(rem, 63, stdin);
		
		if(strcmp(rem, "Bye!\n") == 0)
		{
			leave = 1;
			break;
		}

		memcpy(message + 1, rem, 63);

		int ret = write(fd, message, 64);
		if(ret == -1)
		{
			printf("P%d write failed\n", idx);
		}
	}

	free(message);
	return NULL;
}


int main(int argc, char const *argv[]) 
{
	fd = open("/dev/mihir", O_RDWR);
	if( fd == -1) 
	{
		printf("P%d open failed\n", getpid());
		exit(1);
	}

	if(ioctl(fd, CONNECT_USER, &idx) == -1)
	{
		printf("P%d join failed\n", getpid());
		close(fd);
		exit(2);
	}
	else
	{
		printf("P%d joined\n", idx+1);
		pthread_t t_id;

		pthread_create(&t_id, NULL, send, NULL);

		char *message;
		message = (char *)malloc(64);

		while(!leave) 
		{
			memset(message, 0, 64);

			int rflag = read(fd, message, 64);

			if(rflag == -1) 
			{
				printf("P%d read failed\n", idx+1);
			}
			else if(rflag > 0)
			{
				printf("%s\n", message);
			}
		}

		free(message);
		pthread_join(t_id, NULL);
	}
	printf("P%d leaving  the chat\n", idx+1);
	close(fd);
	exit(0);
}