/*
 * gprs_common.c
 *
 *  Created on: 2014年10月12日
 *      Author: ldd
 */


#include <errno.h>
#include <pthread.h>//添加对线程的支持
//添加对正则表达式的支持
#include <sys/types.h>
#include <regex.h>

#include <signal.h>//添加定时器

#include "gprs_common.h"

static pthread_t ttys_listen_tid;//串口监听线程
static pthread_t heart_beat_tid;//心跳发送线程

extern pthread_mutex_t terminal_mutex;//对屏幕资源的锁定
static pthread_mutex_t gprs_mutex;//对gprs的锁定

/*
*函数名称:           gprs_strerror
*函数原型:           const char *gprs_strerror(int errnum);
*函数功能:           翻译错误码
*函数返回:           返回对应文本形式的错误码，方便打印
*参数说明:           ctx-当前实例句柄，context-想要打印的字符串数据
*所属文件:           <gprs_common.h>
 */
const char *gprs_strerror(int errnum) {
    switch (errnum) {
    case ER_GPRSRO:
        return "GPRS sead overtime";
    case ER_GPRSWO:
        return "GPRS send overtime";
    case ER_GPRSOE:
        return "GPRS open ttys falied";
    case ER_GPRSCE:
        return "GPRS close ttys falied";
    case ER_GPRSNR:
        return "GPRS have no respond";
    case ER_GPRSPE:
        return "GPRS parameter error";
    case ER_GPRSIOE:
    	return "GPRS IO set error";
    case TTYS_OPEN_ERROR:
    	return "Can't open ttys";
    case TTYS_SET_WAITING_ERROR:
    	return "ttys set waiting error";
    case TTYS_SEND_DATA_ERROR:
    	return "Ttys send data error";
    default:
        return strerror(errnum);
    }
}
/*
*函数名称:           gprs_error_print
*函数原型:           void gprs_error_print(hgprs *ctx, const char *context);
*函数功能:           打印出errno所标志的错误码
*函数返回:           无
*参数说明:           ctx-当前实例句柄，context-想要打印的字符串数据
*所属文件:           <gprs_common.h>
 */
void gprs_error_print(hgprs *ctx, const char *context)
{
    if (ctx->basic_status.debug) {
        fprintf(stderr, "ERROR %s", gprs_strerror(errno));
        if (context != NULL) {
            fprintf(stderr, ": %s\n", context);
        } else {
            fprintf(stderr, "\n");
        }
    }
}
/*
*函数名称:           strlcpy
*函数原型:           size_t strlcpy(char *dest, const char *src, size_t dest_size);
*函数功能:           从src复制dest_size个字节数到dest，dest_size>src的字节数保证完成复制
*函数返回:           返回src字符串的字节个数
*参数说明:           dest-目标字符串，src-源字符串，dest_size-要复制的字节个数
*所属文件:           <gprs_common.h>
 */
size_t strlcpy(char *dest, const char *src, size_t dest_size)
{
    register char *d = dest;
    register const char *s = src;
    register size_t n = dest_size;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
    }

    /* Not enough room in dest, add NUL and traverse rest of src */
    if (n == 0) {
        if (dest_size != 0)
            *d = '\0'; /* NUL-terminate dest */
        while (*s++);
    }

    return (s - src - 1); /* count does not include NUL */
}
/*
*函数名称:           get_at_length
*函数原型:           static size_t get_at_length(uint8_t *at);
*函数功能:           对AT指令计算补充结束字符后的长度，如*at="AT",添加结束字符之后是*at="AT\r\n"
*函数返回:           返回at指向的AT指令的实际发送长度，不含\0
*参数说明:           at-at指令的字符串
*所属文件:           <gprs_common.c>
 */
static size_t get_at_length(const uint8_t *at)
{
	size_t len=0;
	while((*at++!='\0')&&(len<MAX_AT_LEN))
	len++;

	return len;
}
/*
*函数名称:           data_pre
*函数原型:           static size_t data_pre(uint8_t *at);
*函数功能:           在发送的数据之后添加0x1a，启动数据发送
*函数返回:           返回dat指向的字符串的长度，已经添加0x1a之后，不含\0
*参数说明:           dat-要发送的数据
*所属文件:           <gprs_common.>
 */
static size_t data_pre(uint8_t *dat)
{
	size_t len=0;
	while((*dat++!='\0')&&(len< MAX_SEND_DATA_LENGTH))
	len++;
	*(dat-1)=0x1a;
	*dat='\0';
	return len;
}
/*
*函数名称:           send_at_cmd
*函数原型:           int send_at_cmd(hgprs *ctx, uint8_t *msg);
*函数功能:           向Sim900发送指令，如*cmd="ATE0",实际串口发送"ATE0\r\n"
*函数返回:           发送正常返回值>0
*参数说明:           ctx-当前实例句柄，at-at指令的字符串
*所属文件:           <gprs_common.c>
 */
static int send_at_cmd(hgprs *ctx, uint8_t *cmd)
{
	size_t leng_to_send = 0;
	leng_to_send = get_at_length(cmd);
	return leng_to_send==ctx->backend->send(ctx,cmd,leng_to_send);//发送指令;
}
/*
*函数名称:           switch_state_machine
*函数原型:           static int switch_state_machine(hgprs *ctx,gprs_work_status new_satus);
*函数功能:           切换gprs的工作状态
*函数返回:           发送正常返回值>0
*参数说明:           ctx-当前实例句柄，new_satus-新的工作状态
*所属文件:           <gprs_common.c>
 */
static int switch_state_machine(hgprs *ctx,gprs_work_status new_satus)
{

	if(new_satus == ctx->sm_satus)//已经是该状态
	return 1;

	if(new_satus!=SM_RECV_DATA)
	{
		tcflush(ctx->s,TCIFLUSH);//将缓冲区中所有数据清空,确保不会干扰到下一次读数据
	}

	// 对互斥锁上锁
	if(pthread_mutex_lock(&gprs_mutex)!=0)
	{
	printf("Thread %d lock failed!\n",ttys_listen_tid);
	pthread_exit(NULL);
	}
	ctx->sm_satus = new_satus;//设定状态机为接收，开启接收线程
	// 解锁
	pthread_mutex_unlock(&gprs_mutex);
}
/*
*函数名称:           check_at_return
*函数原型:           int check_at_return(hgprs *ctx,uint8_t *at_act_respond,uint16_t length_to_read,const uint8_t *at_exp_respond,uint16_t overtime);
*函数功能:           验证AT指令的返回值是否正确
*函数返回:           =0表示等待超时，=1表示返回正常，=-1表示等待函数设定错误
*参数说明:           ctx-当前实例句柄，at_act_respond-串口实际接受到at返回，length_to_read-读取的数据长度（实际就是at_exp_respond的长度），at_exp_respond-正确的at返回,overtime-超时设定
*所属文件:           <gprs_common.h>
 */
int check_at_return(hgprs *ctx,uint8_t *at_act_respond,uint32_t length_to_read,const uint8_t *at_exp_respond,uint16_t overtime)
{
	int rc;
	fd_set rfds;
	struct timeval tv;
	regex_t reg;
	int cflags = REG_EXTENDED;
	regmatch_t pmatch[1];



	int nErrCode = 0;
	char szErrMsg[1024] = {0};
	size_t unErrMsgLen = 0;


	errno = ER_NOERROR;//假设没有错误

	/* 设定一个读等待事件 */
	FD_ZERO(&rfds);
	FD_SET(ctx->s, &rfds);

	/*清除接收缓冲区*/
	memset(at_act_respond, 0, length_to_read * sizeof(uint8_t));

	/*设定超时时间*/
	tv.tv_sec = overtime/1000;
	tv.tv_usec = (overtime-tv.tv_sec*1000)*1000;

	/* 轮询 */
	rc = ctx->backend->select(ctx, &rfds, &tv, length_to_read);

	switch(rc)
	{
		case -1://select error
			errno = TTYS_SET_WAITING_ERROR;
			return -1;
			break;

		case 0://超时
			errno = ER_GPRSRO;//读取数据超时
			return 0;
			break;

		default://可能是等待的事件到了
			break;
	}

	rc = ctx->backend->recv(ctx, at_act_respond, length_to_read);

	if (rc == 0) //如果没有收到指定长度的AT响应，说明AT指令发送错误
	{
		errno = ER_GPRSNR;//GPRS没有响应
		tcflush(ctx->s,TCIFLUSH);//将所有缓冲区中数据清空
		return -1;//AT指令返回错误
	}

	tcflush(ctx->s,TCIFLUSH);//将缓冲区中所有数据清空,确保不会干扰到下一次读数据

	/* 编译正则表达式
	 * reg存放编译后的正则表达式
	 * at_exp_respond是待编译的正则表达式
	 * 成功返回0
	 */
	nErrCode = regcomp(&reg,at_exp_respond,cflags);
	if(nErrCode!=0)
	{
		printf("regcomp error,error code=%d\n",nErrCode);

		unErrMsgLen = regerror(nErrCode, &reg, szErrMsg, sizeof(szErrMsg));
		unErrMsgLen = unErrMsgLen < sizeof(szErrMsg) ? unErrMsgLen : sizeof(szErrMsg) - 1;
		szErrMsg[unErrMsgLen] = '\0';
		printf("ErrMsg: %s\n", szErrMsg);
		regfree(&reg);//释放掉资源
		return -1;
	}

	/* 匹配编译后的正则表达式和目标文本
	 * 返回0表示匹配成功，pmatch中返回匹配的位置
	 */
	nErrCode = regexec(&reg,at_act_respond,1,pmatch,0);
	if(nErrCode!=0)
	{
		printf("regexec error,error code=%d\n",nErrCode);

		unErrMsgLen = regerror(nErrCode, &reg, szErrMsg, sizeof(szErrMsg));
		unErrMsgLen = unErrMsgLen < sizeof(szErrMsg) ? unErrMsgLen : sizeof(szErrMsg) - 1;
		szErrMsg[unErrMsgLen] = '\0';
		printf("ErrMsg: %s\n", szErrMsg);
		return -1;
	}


	printf("befer regfree \n");

	regfree(&reg);//释放掉资源

	printf("after regfree \n");

	return 1;//AT指令返回正常
}
/*
*函数名称:           close_connect
*函数原型:           int close_connect(hgprs *ctx,int channel_num);
*函数功能:           关闭指定的通道连接
*函数返回:           返回==1所有连接都关闭，返回==0存在可用的连接
*参数说明:           ctx-需要发送的数据连接
*所属文件:           <gprs_common.h>
 */
static int is_all_channel_closed(hgprs *ctx)
{
	int rc;
	for(rc=0;rc<8;rc++)
	{
		if(ctx->channel_status[rc])
		{
			rc = 10; //rc == 1说明，还有其他处在连接状态
			break;
		}
	}

	if(rc == 10)
		rc =0;//有链接存在
	else
		rc =1;//所有通道已关闭

	return rc;

}
/*
*函数名称:           send_data_pack
*函数原型:           int send_data_pack(hgprs *ctx,int channel,uint8_t *dat,unsigned int length_to_send);
*函数功能:           发送数据包
*函数返回:           返回>0成功，否则失败
*参数说明:           ctx-当前实例句柄，channel-要发到哪个IP，dat-要发送的数据包，length_to_send-要发送的数据字节数
*所属文件:           <gprs_common.h>
 */
int send_data_pack(hgprs *ctx,int channel,uint8_t *dat,unsigned int length_to_send)
{
	int rc;
	uint8_t at_respond[MAX_AT_LEN];//接受at指令的返回值

	if(channel>8||channel<1)
	return -1;

	if(length_to_send>MAX_SEND_DATA_LENGTH)
	return -1;

	if(ctx->channel_status[channel-1] == false)
	{
		printf("you need to connect to ip if send data vai %d channel\n",channel);
		return -1;
	}

	char at_send_data_head[19];
	if(sprintf(at_send_data_head,"AT+CIPSEND=%d,%d\r\n",channel-1,length_to_send)<0)
	{
		errno = EINVAL;
		return -1;
	}

	switch_state_machine(ctx,SM_SEND_DATA);//切换为发送状态,这里只是关闭串口的接收

	//1.发送AT指令
	rc = send_at_cmd(ctx,at_send_data_head);//先发送发送数据的头 ex:AT+CIPSEND=1,1024
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;


	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_CIPSEND_OK_SIZE,AT_CIPSEND_OK,WAITING_OVERTIME_4000MS)!=1)
	{
		if(ctx->basic_status.debug)
			printf("AT+CIPSEND < return failed\n");
		return -1;//连接错误
	}

	//2.发送数据包
	rc = ctx->backend->send(ctx,dat,length_to_send);
	if(rc != length_to_send)
	return TTYS_SEND_DATA_ERROR;

	//3.ctrl+z启动模块发送
	char ctrl_z[2];
	ctrl_z[0]=0x1a;//ctrl+z
	ctrl_z[1]='\0';//结束字符

	rc = send_at_cmd(ctx,ctrl_z);
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;


	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_SEND_OK_SIZE,AT_SEND_OK,WAITING_OVERTIME_4000MS)!=1)
	{
		if(ctx->basic_status.debug)
			printf("SEND failed\n");
		return -1;//连接错误
	}
	if(is_all_channel_closed(ctx) == 0 )//如果还存在其他连接，就进入监听
	switch_state_machine(ctx,SM_RECV_DATA);//切换为接收状态,监听端口的数据

	return 1;
}
/*
*函数名称:           close_connect
*函数原型:           int close_connect(hgprs *ctx,int channel_num);
*函数功能:           关闭指定的通道连接
*函数返回:           返回>0成功关闭，-1，表示肯那个连接已经关闭
*参数说明:           ctx-需要发送的数据连接
*所属文件:           <gprs_common.h>
 */
int close_connect(hgprs *ctx,int channel_num)
{
	int rc;

	uint8_t at_respond[MAX_AT_LEN];//接受at指令的返回值

	if(channel_num>8||channel_num<1)
	return -1;

	if(ctx->channel_status[channel_num-1] == false)
	{
		printf("the %d connection has been closed\n",channel_num);
		return 1;
	}

	char at_close_connect[18];
	if(sprintf(at_close_connect,"AT+CIPCLOSE=%d[1]\r\n",channel_num-1)<0)//选择快速关闭
	{
		errno = EINVAL;
		return -1;
	}

	switch_state_machine(ctx,SM_SEND_DATA);//切换为发送状态,这里只是关闭串口的接受

	//发送AT指令
	rc = send_at_cmd(ctx,at_close_connect);//先发送发送数据的头 ex:AT+CIPSEND=1,1024
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms

	printf("check_at_return\n");
	if(check_at_return(ctx,at_respond,AT_CIPCLOSE_OK_SIZE,AT_CIPCLOSE_OK,WAITING_OVERTIME_4000MS)!=1)
	{
		printf("check_at_return fail\n");
		//默认只要发出去就会将连接成功关闭
		ctx->channel_status[channel_num-1] = false;//表明连接已经断开
		if(is_all_channel_closed(ctx) == 0 )//如果还存在其他连接，就进入监听
		switch_state_machine(ctx,SM_RECV_DATA);//切换为接收状态,监听端口的数据
		return -1;//连接错误
	}

	//默认只要发出去就会将连接成功关闭
	ctx->channel_status[channel_num-1] = false;//表明连接已经断开
	if(is_all_channel_closed(ctx) == 0 )//如果还存在其他连接，就进入监听
	switch_state_machine(ctx,SM_RECV_DATA);//切换为接收状态,监听端口的数据

	return 1;
}
/*
*函数名称:           close_all_connect
*函数原型:           int close_all_connect(hgprs *ctx);
*函数功能:           关闭所有的连接，发送AT+CIPSHUT 关闭移动场景
*函数返回:           返回>0成功，否则失败
*参数说明:           ctx-需要发送的数据连接
*所属文件:           <gprs_common.h>
 */
int close_all_connect(hgprs *ctx)
{
	int rc,i;
	uint8_t at_respond[MAX_AT_LEN];//接受at指令的返回值

	switch_state_machine(ctx,SM_SEND_DATA);//切换为发送状态,这里只是关闭串口的接受

	//发送AT指令
	rc = send_at_cmd(ctx,"AT+CIPSHUT\r\n");//先发送发送数据的头 ex:AT+CIPSEND=1,1024
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms

	//关闭场景的延时是20s
	if(check_at_return(ctx,at_respond,AT_CIPSHUT_OK_SIZE,AT_CIPSHUT_OK,WAITING_OVERTIME_20000MS)!=1)
	{
		if(ctx->basic_status.debug)
			printf("AT+CIPSHUIT? failed\n");
		return -1;//连接错误
	}

	for(i=0;i<TCP_LINK_CHANNEL_NUM;i++)
	{
		ctx->channel_status[i] = false;//表明连接已经断开
	}

	ctx->net_status = IP_INITIAL;

	return 1;
}
/*
*函数名称:           send_heart_pack
*函数原型:           static send_heart_pack(hgprs *ctx);
*函数功能:           发送心跳包
*函数返回:           无
*参数说明:           ctx-需要发送的数据连接
*所属文件:           <gprs_common.c>
 */
static void send_heart_pack(hgprs *ctx)
{
	int i;
	switch_state_machine(ctx,SM_SEND_DATA);//切换为发送状态,这里只是关闭串口的接收
	for(i=0;i<TCP_LINK_CHANNEL_NUM;i++)
	{
		if(ctx->channel_status[i])//如果连接已经建立，发送心跳数据包
			if(send_data_pack(ctx,i+1,&ctx->heart_beat_char,1)<0)
				printf("send heart beat failed!\n");
	}
	if(is_all_channel_closed(ctx) == 0 )//如果还存在其他连接，就进入监听
	switch_state_machine(ctx,SM_RECV_DATA);//切换为接收状态
}
/*
*函数名称:           machine
*函数原型:           static void *gprs_heartbeat_func(void *arg);
*函数功能:           建立TCP连接后，如果需要长时间的连接，使能心跳保持稳定连接
*函数返回:           无
*参数说明:           arg-需要传入的参数
*所属文件:           <gprs_common.c>
 */
static void *gprs_heartbeat_func(void *arg)
{
	hgprs *ctx;
	ctx = (hgprs *)arg;
	while(1)
	{
		if(ctx->basic_status.enable_heartbeat)//如果心跳使能，每隔
			send_heart_pack(ctx);//发送心跳包
		sleep(SEND_HEART_BEAT_RATE);
	}
	pthread_exit(NULL);//退出线程
}
/*
*函数名称:           gprs_state_machine
*函数原型:           static void *gprs_state_machine(void *arg);
*函数功能:           监听ttys的线程函数
*函数返回:           无
*参数说明:           arg-需要传入的参数
*所属文件:           <gprs_common.c>
 */
static void *gprs_state_machine(void *arg)
{

	hgprs *ctx;
	ctx = (hgprs *)arg;
	uint8_t recv_respond[MAX_RECV_DATA_LENGTH];

	int rc;
	fd_set rfds;
	struct timeval tv;//等待超时,这里设定为100ms,有数据就会读取，没有数据就会放弃，就是4s


	/*设定超时时间*/
	tv.tv_sec = 0;
	tv.tv_usec = 100000;

	while(1)
	{
		switch(ctx->sm_satus)
		{
		case SM_RECV_DATA: //监听数据接收
			printf("SM_RECV_DATA listen\n");
			//只要状态不改变就会一直监听
			while(ctx->sm_satus == SM_RECV_DATA)
			{
				/* 设定一个读等待事件 */
				FD_ZERO(&rfds);
				FD_SET(ctx->s, &rfds);

				/*清除接收缓冲区*/
				memset(recv_respond, 0, MAX_RECV_DATA_LENGTH * sizeof(uint8_t));

				rc = ctx->backend->select(ctx, &rfds, &tv, MAX_RECV_DATA_LENGTH);

				if(rc>0)
				{
				rc = ctx->backend->recv(ctx, recv_respond, MAX_RECV_DATA_LENGTH);
				tcflush(ctx->s,TCIFLUSH);//将缓冲区中所有数据清空,确保不会干扰到下一次读数据



				}

				usleep(10000);//睡眠10us
			}
			break;
		case SM_SEND_DATA: //数据发送
			//printf("SM_SEND_DATA\n");
			break;
		case SM_IDLE: //空闲状态
			printf("SM_IDLE\n");
			break;
		case SM_CLOSE: //即将关闭状态机
			printf("SM_CLOSE\n");
			pthread_exit(NULL);
			break;
		}

		usleep(200000);	//睡200ms
	}

	pthread_exit(NULL);
}
/*
*函数名称:           _create_state_machine
*函数原型:           static int _create_state_machine(hgprs *ctx);
*函数功能:           创建监听线程
*函数返回:           返回值>0成功创建线程
*参数说明:           ctx-包含串口句柄,为了控制调试加入
*所属文件:           <gprs_common.c>
 */
static int _create_state_machine(hgprs *ctx)
{

	 // 创建快速互斥锁（默认），锁的编号返回给ttys_mutex
	pthread_mutex_init(&gprs_mutex,NULL);

	if(pthread_create(&ttys_listen_tid,NULL,gprs_state_machine,ctx)!=0)
	{
		printf("Create thread error!\n");
		return 0;
	 }

	if(ctx->basic_status.debug)
		printf("Create thread success, tid is %d\n", ttys_listen_tid);
	return 1;
}
/*
*函数名称:           _create_gprs_heartbeat
*函数原型:           static int _create_gprs_heartbeat(hgprs *ctx);
*函数功能:           创建心跳线程
*函数返回:           返回值>0成功创建线程
*参数说明:           ctx-包含串口句柄,为了控制调试加入
*所属文件:           <gprs_common.c>
 */
static int _create_gprs_heartbeat(hgprs *ctx)
{

	if(pthread_create(&heart_beat_tid,NULL,gprs_heartbeat_func,ctx)!=0)
	{
		printf("Create thread error!\n");
		return 0;
	 }

	if(ctx->basic_status.debug)
		printf("Create thread success, tid is %d\n", heart_beat_tid);
	return 1;
}
/*
*函数名称:           Test
*函数原型:           void Test(hgprs *ctx);
*函数功能:           验证函数功能，开发人员使用，将自己撰写的程序代码放在这里测试
*函数返回:           无
*参数说明:           ctx-当前实例句柄
*所属文件:           <gprs_common.h>
 */
void Test(hgprs *ctx)
{
	_create_state_machine(ctx);

	sleep(2);//睡1s
	// 对互斥锁上锁
	if(pthread_mutex_lock(&gprs_mutex)!=0)
	{
	printf("Thread %d lock failed!\n",ttys_listen_tid);
	pthread_exit(NULL);
	}
	ctx->sm_satus = SM_RECV_DATA;
	// 解锁
	pthread_mutex_unlock(&gprs_mutex);


	sleep(2);//睡1s

	ctx->sm_satus = SM_SEND_DATA;
	sleep(2);//睡1s

	ctx->sm_satus = SM_IDLE;
	sleep(2);//睡1s

	ctx->sm_satus = SM_CLOSE;
	sleep(2);//睡1s


}
/*
*函数名称:           send_data
*函数原型:           int send_data(hgprs *ctx, uint8_t *dat);
*函数功能:           向Sim900发送数据，如*dat={0x1a,0x13,0x14},实际串口发送*dat={0x1a,0x13,0x14}+
*函数返回:           返回实际发生的数据，不含\0
*参数说明:           ctx-当前连接句柄，dat-要发送的数据
*所属文件:           <gprs_common.h>
 */
int send_data(hgprs *ctx, uint8_t *dat)
{
	size_t leng_to_send;
	leng_to_send = data_pre(dat);

	return ctx->backend->send(ctx,dat,leng_to_send);
}
/*
*函数名称:           gprs_init
*函数原型:           int gprs_init(hgprs *ctx);
*函数功能:           完成对gprs模块的初始化，初始化不成功不能进行任何其他操作
*函数返回:           =1,初始化成功
*参数说明:           ctx-当前连接句柄
*所属文件:           <gprs_common.h>
 */
int gprs_init(hgprs *ctx)
{
	int rc;
	size_t leng_to_send;
	uint8_t at_respond[MAX_AT_LEN];//接受at指令的返回值

	errno = ER_NOERROR;//假设没有错误


	//1. 检查串口是否被打开，，没有打开的话先打开
	if(ctx->basic_status.ttys_open == false)
	{
		if (ctx->backend->connect(ctx) == -1)
		{
		printf("open the ttys failed!\n");
		return -1;
		}
	}

	if(ctx->basic_status.debug)
	printf("ttys5 has been opened!\n");

	//2. 检查模块是否被打开，没有打开的话打开
	int trytime=8;//尝试8次，如果不行退出


	rc = send_at_cmd(ctx,"AT\r\n");//同步波特率，如果回应错误，认为没有开机，执行一次开机
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;


	while(trytime--)
	{
		rc = send_at_cmd(ctx,"AT\r\n");//同步波特率，如果回应错误，认为没有开机，执行一次开机
		if(rc<0)
		return TTYS_SEND_DATA_ERROR;

		sleep(1);
		if(check_at_return(ctx,at_respond,AT_OK_SIZE,AT_OK,WAITING_OVERTIME_4000MS)!=1)//如果匹配不到认为没有开机，执行开机
		{
			ctx->backend->power_on_off(ctx);

			printf("power on the gprs , waiting for 5s-------");
			sleep(5);//等待5s开机
		}
		else
		{
			trytime = 1;
			break;
		}

	}

	if(trytime==0)//开机失败，或者是通信失败
	return -1;

	ctx->basic_status.power_on = true;//返回说明成功开机，并且通信正常


	if(ctx->basic_status.debug)
	printf("sim900 has been powered!\n");

	//3. 检查模块是否插卡
	rc = send_at_cmd(ctx,"AT+CPIN?\r\n");//查询是否检测到SIM卡
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_CPIN_OK_SIZE,AT_CPIN_OK,WAITING_OVERTIME_4000MS)!=1)
	{
		if(ctx->basic_status.debug)
			printf("AT+CPIN? failed\n");
		return -1;//连接错误
	}

	//4. 关闭模块回显

	rc = send_at_cmd(ctx,"ATE0\r\n");//关闭回显
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回
	if(check_at_return(ctx,at_respond,AT_ATE0_OK_SIZE,AT_ATE0_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//连接错误

	ctx->basic_status.auto_echo = false;


	//5. 检查模块信号质量
	rc = send_at_cmd(ctx,"AT+CSQ\r\n");//查询信号质量
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回
	if(check_at_return(ctx,at_respond,AT_CSQ_OK_SIZE,AT_CSQ_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//连接错误

	//6. 是否注册并附着于网络
	rc = send_at_cmd(ctx,"AT+CREG?\r\n");//是否注册网络
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回
	if(check_at_return(ctx,at_respond,AT_CREG_OK_SIZE,AT_CREG_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//连接错误

	rc = send_at_cmd(ctx,"AT+CGATT?\r\n");//查询是否附着网络
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回
	if(check_at_return(ctx,at_respond,AT_CGATT_OK_SIZE,AT_CGATT_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//连接错误

	ctx->net_status = IP_INITIAL;//附着到网络成功后是初始化状态

	ctx->basic_status.init_sucess = true;

	//1. 检查当前模块的网络状态
	rc = send_at_cmd(ctx,"AT+CIPSTATUS\r\n");//查询网络状态
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_CIPSTATUS_OK_SIZE,AT_IP_INITIAL_OK,WAITING_OVERTIME_4000MS)!=1)
	{
		//1. 设定APN
		rc = send_at_cmd(ctx,"AT+CIPSHUT\r\n");//将SIM的状态转移到初始化状态
		if(rc<0)
		return TTYS_SEND_DATA_ERROR;

		usleep(200000);//延时200ms，等待AT返回

		//超时设定为20s，确保设备成功关闭移动场景
		if(check_at_return(ctx,at_respond,AT_CIPSHUT_OK_SIZE,AT_CIPSHUT_OK,WAITING_OVERTIME_20000MS)!=1)
		return -1;//连接错误


	}

	rc = send_at_cmd(ctx,"AT+CIPMUX=1\r\n");//使能多中心连接
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_RESPOND_OK_SIZE,AT_RESPOND_OK,WAITING_OVERTIME_10000MS)!=1)
	return -1;//；连接错误


	rc = send_at_cmd(ctx,"AT+CSTT\r\n");//设置APN，网络接入点
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_RESPOND_OK_SIZE,AT_RESPOND_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//；连接错误

	ctx->net_status = IP_START;//启动任务状态

	rc = send_at_cmd(ctx,"AT+CIICR\r\n");//激活移动场景
	if(rc<0)
	{
	printf("AT+CIICR Send error!");
	return TTYS_SEND_DATA_ERROR;
	}

	usleep(200000);//延时200ms，等待AT返回

	rc = check_at_return(ctx,at_respond,AT_RESPOND_OK_SIZE,AT_RESPOND_OK,WAITING_OVERTIME_20000MS);
	if(rc == 0)
	printf("AT+CIICR Respond over time");

	if(rc!=1)
	return -1;//；连接错误

	ctx->net_status = IP_GPRSACT;//启动任务状态

	rc = send_at_cmd(ctx,"AT+CIFSR\r\n");//获取GPRS的IP
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_CIFSR_OK_SIZE,AT_CIFSR_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//；连接错误

	ctx->net_status = IP_STATUS;//已经获取IP可以连接IP

	printf("ghgffth");

	return 1;
}
/*
*函数名称:           connect_server
*函数原型:           int connect_server(hgprs *ctx);
*函数功能:           向服务器发起连接，发送前确保ctx中的ip和端口已配置
*函数返回:           返回值>0表示连接成功
*参数说明:           ctx-当前连接句柄, n-要连接7个IP的哪一个
*所属文件:           <gprs_common.h>
 */
int connect_to_server(hgprs *ctx,int n)
{
	int rc;
	size_t leng_to_send;
	uint8_t at_respond[MAX_AT_LEN];//接受at指令的返回值

	errno = ER_NOERROR;//假设没有错误

	if(n>8&&n<1)//连接数小于7
	{
	errno = EINVAL;
	return -1;
	}

	if(ctx->channel_status[n-1])//如果已经建立了连接，返回1
	return 1;

	for(rc=0;rc<8;rc++)
	{
		if(ctx->channel_status[rc])
		{
			rc = 10; //rc == 1说明，还有其他处在连接状态
			break;
		}

	}

	if(rc != 10 )//只要rc!=10说明所有的通道连接都没有建立过，先初始化
	{
		rc = gprs_init(ctx);
		if(rc<0)
		return -1;
	}


	if(ctx->basic_status.init_sucess == false)//还没有初始化,在连接之前必须先初始化模块
	{
		if(ctx->basic_status.debug)
		printf("you must call function \"gprs_init\",before connecting \n");

		return -1;
	}

	switch_state_machine(ctx,SM_SEND_DATA);//切换为发送状态，实际是关闭监听

	//确保连接在IP_STATUS的状态下
	rc = send_at_cmd(ctx,"AT+CIFSR\r\n");//获取GPRS的IP
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	if(check_at_return(ctx,at_respond,AT_CIFSR_OK_SIZE,AT_CIFSR_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//；连接错误

	ctx->net_status = IP_STATUS;//已经获取IP可以连接IP

	char *tcp_link = &ctx->tcp_link_at[n-1];

	if(sacnf_at_pre(tcp_link)<35)//ctx->tcp_link_at指令长度至少在35，否则连接无效
	return -1;//；连接错误


	//AT+CIPSTART=1,"TCP","218.57.140.130",13000,连接到公网ip，
	rc = send_at_cmd(ctx,tcp_link);//
	if(rc<0)
	return TTYS_SEND_DATA_ERROR;

	usleep(200000);//延时200ms，等待AT返回

	//返回值分两次，第一次返回OK表示建立连接的命令格式正确
	if(check_at_return(ctx,at_respond,AT_RESPOND_OK_SIZE,AT_RESPOND_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//；连接错误

	usleep(200000);//延时200ms，等待AT返回

	//第二次返回值是n,CONNECT OK，表示建立连接成功
	if(check_at_return(ctx,at_respond,AT_CIPSTART_OK_SIZE,AT_CIPSTART_OK,WAITING_OVERTIME_4000MS)!=1)
	return -1;//；连接错误

	//目前为止成功建立channel n的TCP连接
	ctx->channel_status[n-1] = true;

	ctx->net_status = IP_TCP_CONNECT;//目前的状态已经连接到TCP

	if (ctx->basic_status.debug)
	{
	  printf("channel %d :%s\nRespond: CONNECT OK\n",n,&ctx->tcp_link_at[n-1]);
	}

	if(ctx->basic_status.work_machine == false)//如果状态机没有开，说明线程还没有开启
	{
		switch_state_machine(ctx,SM_RECV_DATA);//切换为接受状态,监听端口的数据
		_create_state_machine(ctx);//开启接收线程
		_create_gprs_heartbeat(ctx);//开启心跳发送线程
		ctx->basic_status.work_machine = true;
	}
	else
	{
		//如果线程已经开启，更改状态为接收
		switch_state_machine(ctx,SM_RECV_DATA);//切换为接受状态,监听端口的数据
	}

	return 1;//模块已经成功连接到网络
}
/*
*函数名称:           connect_server
*函数原型:           int connect_server(hgprs *ctx);
*函数功能:           向服务器发起连接，发送前确保ctx中的ip和端口已配置
*函数返回:           返回值>0表示连接成功
*参数说明:           ctx-当前连接句柄
*所属文件:           <gprs_common.h>
 */
int disconnect_server(hgprs *ctx)
{
	int rc;
	size_t leng_to_send;
	uint8_t at_respond[MAX_AT_LEN];//接受at指令的返回值

	errno = ER_NOERROR;//假设没有错误
}
/*
*函数名称:           sacnf_at_pre
*函数原型:           size_t sacnf_at_pre(uint8_t *at);
*函数功能:           在程序初期，为了辅助调试使用，调整通过sacnf打入的字符串,将at在NULL后以\r\n代替
*函数返回:           返回at指向的字符串从开始知道遇到第一个\n后的字节数
*参数说明:           at-at指令的字符串
*所属文件:           <gprs_common.h>
 */
size_t sacnf_at_pre(uint8_t *at)
{
	size_t len=0;
	while((*at++!='\0')&&(len<MAX_AT_LEN))
	len++;
	*(at-1)='\r';
	*at='\n';
	*(at+1)='\0';
	return len+2;
}
/*
*函数名称:           sacnf_mesg_pre
*函数原型:           size_t sacnf_mesg_pre(uint8_t *at);
*函数功能:           在程序初期，为了辅助调试使用，调整通过sacnf打入的字符串,将at在NULL后以0x1a
*函数功能:          	 代替
*函数返回:           返回at指向的字符串从开始知道遇到第一个\n后的字节数
*参数说明:           at-at指令的字符串
*所属文件:           <gprs_common.h>
 */
size_t sacnf_mesg_pre(uint8_t *at)
{
	size_t len=0;
	while((*at++!='\0')&&(len<MAX_AT_LEN))
	len++;
	*(at-1)=0x1a;
	*at='\n';
	*(at+1)='\0';
	return len+2;
}
/*
*函数名称:           getatlen
*函数原型:           size_t getatlen(const uint8_t *at);
*函数功能:           获取要发送的AT指令的数据字节数，方便write函数的调用
*函数返回:           返回at指向的字符串从开始知道遇到第一个\n后的字节数
*参数说明:           at-at指令的字符串
*所属文件:           <gprs_common.h>
 */
/*AT指令字符串的操作,所有的AT指令都是以\r和\n结束的*/
size_t getatlen(const uint8_t *at)
{
	size_t len=0;
	while((*at++!='\n')&&(len<MAX_AT_LEN))
		len++;
	return len+1;
}

/*
*函数名称:           receive_msg
*函数原型:           int receive_msg(hgprs *ctx, uint8_t *msg);
*函数功能:           等待串口数据，ctx包含串口句柄，如果接受到数据，将数据保存到msg所指向的缓冲区
*函数返回:           返回实际接受到的数据个数
*参数说明:           ctx-包含串口句柄，msg-数据保存到msg所指向的缓冲区
*所属文件:           <gprs_common.h>
 */
int receive_msg(hgprs *ctx, uint8_t *msg)
{
	int rc;
	fd_set rfds;
	struct timeval tv;
	struct timeval *p_tv;
	int length_to_read;
	int msg_length = 0;

	/* Add a file descriptor to the set */
	FD_ZERO(&rfds);
	FD_SET(ctx->s, &rfds);

	/* We need to analyse the message step by step.  At the first step, we want
	 * to reach the function code because all packets contain this
	 * information. */

	length_to_read = 2048;
	memset(msg, 0, length_to_read * sizeof(uint8_t));
	/* Wait for a message, we don't know when the message will be
	 * received */
	p_tv = NULL;

	rc = ctx->backend->select(ctx, &rfds, p_tv, length_to_read);

	if (rc == -1) {
		printf("waiting over time !\n");
		return -1;
		}

	rc = ctx->backend->recv(ctx, msg, length_to_read);
	if (rc == 0) {
		errno = ECONNRESET;
		printf("recv 0 bytes msg !\n");
		rc = -1;
	}

	tcflush(ctx->s,TCIFLUSH);//将缓冲区中所有数据清空,确保不会干扰到下一次读数据

	/* Display the hex code of each character received */
	if (ctx->basic_status.debug) {
		int i;
		for (i=0; i < rc; i++)
			printf("%c", msg[msg_length + i]);
	}

	return rc;
}





