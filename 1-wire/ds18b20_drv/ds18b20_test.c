/****************************************************************************************************************
 * 文件名称	：	ds18b20_teset.c
 * 简介		:	OK6410 DS18B20驱动测试程序
 * 作者		：	异灵元（cp1300@139.com）
 * 创建时间	：	2012/09/19 22：10
 * 修改时间	：	2012/09/19
 * 说明		：	OK6410 开发板（S3C6410）DS18B20（GPIO）驱动测试程序
 ****************************************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>



int main(void)
{
	int fd;
	int data;

	//DS18B20测试
	printf("DS18B20 test...\n");
	fd = open("/dev/OK6410_DS18B20",O_RDONLY);		//open DS18B20
	if(fd == -1)
	{
		printf("open DS18B20 error!\n");
		exit(-1);
	}
	else
	{
		printf("open DS18B20 ok!\n");
	}
	while(1)
	{
			if(read(fd,&data,(size_t)2))
				printf("read error!\n");
			printf("ds18b20 = %d\n",data);
			usleep(1000 * 1000);	//1000MS
	}
}

