/*
 * gprs_sim900.h
 *
 *  Created on: 2014年10月10日
 *      Author: LDD
 */

#ifndef GPRS_SIM900_H_
#define GPRS_SIM900_H_

#ifndef _MSC_VER
# include <stdint.h>
# include <sys/time.h>
#else
# include "stdint.h"
# include <time.h>
typedef int ssize_t;
#endif


#include <termios.h>	//uart配置结构体需要
#include <sys/types.h>
#include <stdbool.h>

#include "em9287_gpio.h"//实现对GPIO的控制，完成sim900的开关机


#ifdef  __cplusplus
# define GPRS_BEGIN_DECLS  extern "C" {
# define GPRS_END_DECLS    }
#else
# define GPRS_BEGIN_DECLS
# define GPRS_END_DECLS
#endif


GPRS_BEGIN_DECLS

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef OFF
#define OFF 0
#endif

#ifndef ON
#define ON 1
#endif



#define MAX_DATA_LEN 1500 //gprs data的最大长度


#define SIM900_ON	GPIO16//SIM900的开机端口，拉高电平1.2s后释放实现开关机
#define SIM900_DTR	GPIO17//SIM900的DTR引脚，通过该引脚控制sim900进入或
						  //者离开睡眠模式，具体如下：当DTR为高电平时，同时没有其
						  //它事务处理，也没有硬件中断(如GPIO中断  或串口上有数据
						  //传输)，模块将自动进入SLEEP模式。如果DTR引脚被拉低，
					      //就可以使模块从SLEEP模式退出，DTR信号变低大约50ms
						  //后，串口就可激活
#define SIM900_RI	GPIO24//待机情况下为高电平，当收到 SMS，RI脚将变低，保持低电平 120ms后，又变成高电平



/* Timeouts in microsecond (0.5 s) */
#define _RESPONSE_TIMEOUT    500000
#define _BYTE_TIMEOUT        500000


typedef enum
{
	IP_INITIAL     = (1 << 1),//初始化开机附着网络后的状态(+CGATT: 1)
	IP_START       = (1 << 2),//启动任务，正确设定APN后(AT+CSTT)
	IP_GPRSACT     = (1 << 3),//接受场景配置的状态
	IP_STATUS      = (1 << 4),//已经可以获取IP，可以进行TCP的连接
	IP_TCP_CONNECT = (1 << 5),//建立了TCP的连接，执行CIPSTART后
	IP_TCP_CLOSED  = (1 << 6),//关闭TCP的连接，回到IP_GPRSACT的状态
}gprs_net_status;//标志着模块的网络状态



typedef struct
{
	bool ttys_open;//当前gprs模块连接的串口是否打开，打开后是TRUE
	bool auto_echo;//当前gprs回显标志，TRUE表示自动回显开启（ATE1）
	bool power_on;//模块上电状态，通过AT指令检查状态，初始值为FALSE
	bool enable_heartbeat;//心跳使能，在建立TCP长连接的时候有效
	bool debug;//调试使能
	bool init_sucess;//初始化成功
	bool work_machine;//标志着线程状态
}gprs_basic_status;//标志着模块的基本工作状态


typedef enum
{
	SM_NOT_OPNE    = (1 << 1),//状态机没有开启，就是线程还没有开启，初始状态
	SM_IDLE        = (1 << 2),//状态机空闲状态，可以理解为挂起，因为什么事情都不做
	SM_SEND_DATA   = (1 << 3),//状态机处于发送数据的状态
	SM_RECV_DATA   = (1 << 4),//状态机处于接受数据的状态
	SM_CLOSE       = (1 << 5),//关闭状态机，退出线程的运行
}gprs_work_status;//当前状态机的工作形态



#define TCP_LINK_CHANNEL_NUM 8





typedef struct _gprs_uart {
    /* Device: "/dev/ttyS0", "/dev/ttyUSB0" or "/dev/tty.USA19*" on Mac OS X for
       KeySpan USB<->Serial adapters this string had to be made bigger on OS X
       as the directory+file name was bigger than 19 bytes. Making it 67 bytes
       for now, but OS X does support 256 byte file names. May become a problem
       in the future. */
    char device[16];
    /* Bauds: 9600, 19200, 57600, 115200, etc */
    int baud;
    /* Data bit */
    uint8_t data_bit;
    /* Stop bit */
    uint8_t stop_bit;
    /* Parity: 'N', 'O', 'E' */
    char parity;
    /* Save old termios settings */
    struct termios old_tios;
} gprs_ttys;


typedef struct _simcom900 hgprs;

//将常用的函数封装到该结构体
typedef struct gprs_func {
    int (*connect) (hgprs *ctx);//打开串口，实现连接
    void (*close) (hgprs *ctx);//释放串口
    ssize_t (*send) (hgprs *ctx, const uint8_t *req, int req_length);//发送数据
    ssize_t (*recv) (hgprs *ctx, uint8_t *rsp, int rsp_length);//读取数据
    void (*enable_dbg)(hgprs *ctx,uint8_t flag);
    int (*flush) (hgprs *ctx);//清楚缓冲区
    int (*select) (hgprs *ctx, fd_set *rfds, struct timeval *tv, int msg_length);
    int (*filter_request) (hgprs *ctx, int slave);
    void (*set_ip_port)(hgprs *ctx,const char *ip,int port,int n);
    void (*set_heart_char)(hgprs *ctx,char heart);
    int (*power_on_off)(hgprs *ctx);
    //int (*create_listen_thread)(hgprs *ctx);
} gprs_func_t;


struct _simcom900 {
    /* sim900的串口句柄 */
    int s;
    /*调试使能*/
   /* int debug;
    int ttys_open;//表明当前串口的连接状态
    int auto_echo;//是否回显,TRUE回显使能
*/
    gprs_basic_status basic_status;
    gprs_net_status net_status;

    char bandip[20];//待连接的公网IP
    int port;//待连接的公网IP端口
    char tcp_link_at[TCP_LINK_CHANNEL_NUM][46];//使用AT指令连接到IP时使用的AT指令，如：AT+CIPSTART="TCP","218.57.140.130",10001

    bool channel_status[TCP_LINK_CHANNEL_NUM];

    gprs_work_status sm_satus;

    char heart_beat_char;//维护心跳的字符

    struct timeval response_timeout;
    struct timeval byte_timeout;
    const gprs_func_t *backend;
    gprs_ttys ttys_property;
    void *backend_data;
};




GPRS_END_DECLS

#endif /* GPRS_SIM900_H_ */
