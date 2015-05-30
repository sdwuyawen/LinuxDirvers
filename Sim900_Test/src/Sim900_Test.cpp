//============================================================================
// Name        : Sim900_Test.cpp
// Author      : LDD
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>//添加对线程的支持

#include "gprs_sim900.h"

#include "gprs_common.h"

pthread_mutex_t terminal_mutex;//对屏幕资源的锁定

int main(int argc,char *argv[])
{

	int rc;

	unsigned char ctr;//控制字符变量
	uint8_t at[100],recvat[1024];//AT指令缓冲区

	uint8_t senddat[20]="hello world!";

	uint8_t bytes;
	printf("new 5 %d \n",argc);

	hgprs *ctx;

	// 创建快速互斥锁（默认），锁的编号返回给ttys_mutex
	pthread_mutex_init(&terminal_mutex,NULL);


	ctx = gprs_link("/dev/ttyS5", 9600, 'N', 8, 1);
	if (ctx == NULL) {
		fprintf(stderr, "Unable to allocate libmodbus context\n");
		return -1;
	}

	ctx->backend->enable_dbg(ctx,true);//使能调试

	ctx->backend->set_ip_port(ctx,"\"218.57.140.130\"",13000,2);

	ctr = 'h';//'h'初始化
	while(1)
	{
		switch(ctr)
		{
		case 'a':
		close_connect(ctx,2);
		ctr = 'n';//清空指令
		break;
		case 'f':
		send_data_pack(ctx,2,senddat,12);
		ctr = 'n';//清空指令
		break;
		case 'h':
		printf("intput the dictate: \nf to send data hello world!\na to test \ni to init gprs\nc to connect\nd to break the link\nq to exit\ne to open the ttys5\nh for help\ns to send AT \ng to shutdown or start the sim900\nm to input the data to send\n");
		ctr = 'n';//清空指令
		break;
		case 'c':
		if(connect_to_server(ctx,2)!=1)
		printf("connect error!\n");
		ctr = 'n';//清空指令
		break;
		case 'i':
		if(gprs_init(ctx)!=1)
		printf("gprs_init error!\n");
		else
		printf("gprs_init success!\n");
		ctr = 'n';//清空指令
		break;
		case 'd':
		if(close_all_connect(ctx)!=1)
		printf("connect error!\n");
		ctr = 'n';//清空指令
		break;
		case 'e':
		printf("open the ttys5\n");
		if(ctx->basic_status.ttys_open == FALSE)
		{
		if (ctx->backend->connect(ctx) == -1) {
			printf("Connection failed,exit!\n");
			free(ctx);
			return -1;
		    }
		}
		else
		{
			printf("ttys5 has been opened!\n");
		}
		ctr = 'n';//清空指令
		break;
		case 's':
		if(ctx->basic_status.ttys_open == TRUE)
		{
			printf("intput the AT dictate:\n");
			/*if(scanf("%s",at)>0)
			{
				if(ctx->backend->send(ctx,at,getatlen(at))==getatlen(at))
					printf("the AT dictate Send Success");
			}*/
			if(scanf("%s",at)>0)
			{
				sacnf_at_pre(at);
				//printf("the input AT dictate :\n %s\n the number of string is :%d\n",at,getatlen(at));
				//切换为发送状态，实际是关闭监听
				ctx->sm_satus = SM_SEND_DATA;//设定状态机为接收，开启接收线程
				bytes = ctx->backend->send(ctx,at,getatlen(at));
				usleep(20000);
				bytes = receive_msg(ctx,recvat);
			}

		}
		else
		{
			printf("you need to open the ttys5 firstly!\n");
		}
		ctr = 'n';//清空指令
		break;
		case 'm':
		if(ctx->basic_status.ttys_open == TRUE)
		{
			printf("intput the mesg:\n");
			/*if(scanf("%s",at)>0)
			{
				if(ctx->backend->send(ctx,at,getatlen(at))==getatlen(at))
					printf("the AT dictate Send Success");
			}*/
			if(scanf("%s",at)>0)
			{
				sacnf_mesg_pre(at);
				//printf("the input AT dictate :\n %s\n the number of string is :%d\n",at,getatlen(at));
				ctx->sm_satus = SM_SEND_DATA;//设定状态机为接收，开启接收线程
				bytes = ctx->backend->send(ctx,at,getatlen(at));


				bytes = receive_msg(ctx,recvat);
			}



		}
		else
		{
			printf("you need to open the ttys5 firstly!\n");
		}
		ctr = 'n';//清空指令
		break;
		case 'g':
		//实现sim900的关机
		rc = ctx->backend->power_on_off(ctx);
		if(rc<0)
		{
			printf("sim900_on failed, the error number is %d\n",rc);
			return rc;
		}
		printf("sim900_on_off-------\n");
		ctr = 'n';//清空指令
		break;
		case 'q':

		if(ctx->basic_status.ttys_open == TRUE)
		{
		printf("close the ttys5\n");
		ctx->backend->close(ctx);
		}

		break;
		}

		if(ctr == 'q')
		{
			printf("close the ttys5 success, free the data and exit!");
			if(ctx != NULL)
			{
			free(ctx);
			}
			break;
		}

		ctr = getchar();

	}

	// 消除互斥锁
	pthread_mutex_destroy(&terminal_mutex);

	return 0;
}
