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
	int cnt_total = 0;
	int cnt_data_error = 0;
	int cnt_read_error = 0;
	int ret;

	//DS18B20测试
	printf("DS18B20 test...\n");
	fd = open("/dev/DS18B20",O_RDONLY);		//open DS18B20
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
			if((ret = read(fd,&data,sizeof(data))) != 0)
			{
				cnt_read_error++;
				printf("app read error!, ret = %d\n", ret);
				continue;
			}
				
			cnt_total++;
			if(data > 4000)
			{
				cnt_data_error++;
			}
			printf("total = %d, ds18b20 = %4d, cnt_read_error = %d, cnt_data_error = %d\n", cnt_total, data, cnt_read_error, cnt_data_error);
			usleep(1000 * 200);	//1000MS
	}
}

