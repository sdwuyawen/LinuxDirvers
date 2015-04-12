#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void my_func(int sign_no)
{
	if(sign_no==SIGINT)
		printf("I have get SIGINT\n");
	else if(sign_no==SIGQUIT)
		printf("I have get SIGQUIT\n");	
	else if(sign_no==SIGUSR1)
		printf("I have get SIGUSR1\n");
}
int main()
{
	printf("Waiting for signal SIGINT or SIGQUIT \n ");
	
	/*注册信号处理函数*/
	signal(SIGINT, my_func);
	signal(SIGQUIT, my_func);
	signal(SIGUSR1, my_func);
	
	while(1)
	{
		sleep(1000);
	}
	pause();
	exit(0);
}

