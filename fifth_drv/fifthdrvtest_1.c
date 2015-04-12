
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


/* fifthdrvtest 
  */
int fd;

void my_signal_fun(int signum)
{
	unsigned char key_val;
	read(fd, &key_val, 1);
	printf("key_val: 0x%x\n", key_val);
}

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret;
	int Oflags;

	/* 注册信号处理函数 */
	signal(SIGIO, my_signal_fun);
	
	fd = open("/dev/buttons", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	/* F_SETOWN是告诉驱动程序应用程序的PID
		驱动程序无须处理该项，内核会自动完成
		在控制命令的处理中设置filp->f_owner为对应进程PID */
	fcntl(fd, F_SETOWN, getpid());
	
	/* 读取文件的flag，并加上FASYNC标志，FASYNC标志的改变会调用驱动程序的fasync() */
	Oflags = fcntl(fd, F_GETFL); 
	fcntl(fd, F_SETFL, Oflags | FASYNC);


	while (1)
	{
		sleep(1000);
	}
	
	return 0;
}

