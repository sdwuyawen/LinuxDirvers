/*
 * gprs_common.h
 *
 *  Created on: 2014年10月12日
 *      Author: LDD
 */

#ifndef GPRS_COMMON_H_
#define GPRS_COMMON_H_

#include "gprs_sim900.h"


/*对gprs通信过程中可能出现的错误进行定义*/

GPRS_BEGIN_DECLS

/* Random number to avoid errno conflicts */
#define GPRS_ENOBASE 12000000

/* Protocol exceptions */
enum {
    GPRS_EXCEPTION_READ_OVERTIME = 0x01,
    GPRS_EXCEPTION_SEND_OVERTIME,
    GPRS_EXCEPTION_OPEN_TTYS_FAILED,
    GPRS_EXCEPTION_CLOSE_TTYS_FAILED,
    GPRS_EXCEPTION_HAVE_NO_RESPOND,
    GPRS_EXCEPTION_Structure_Parameter_Wrong,
    GPRS_GPIO_ERROR,
    GPRS_NO_ERROR
};

#define ER_GPRSRO  (GPRS_ENOBASE + GPRS_EXCEPTION_READ_OVERTIME)
#define ER_GPRSWO  (GPRS_ENOBASE + GPRS_EXCEPTION_SEND_OVERTIME)
#define ER_GPRSOE  (GPRS_ENOBASE + GPRS_EXCEPTION_OPEN_TTYS_FAILED)
#define ER_GPRSCE  (GPRS_ENOBASE + GPRS_EXCEPTION_CLOSE_TTYS_FAILED)
#define ER_GPRSNR  (GPRS_ENOBASE + GPRS_EXCEPTION_HAVE_NO_RESPOND)
#define ER_GPRSPE  (GPRS_ENOBASE + GPRS_EXCEPTION_Structure_Parameter_Wrong)
#define ER_GPRSIOE  (GPRS_ENOBASE + GPRS_GPIO_ERROR)
#define ER_NOERROR (GPRS_ENOBASE + GPRS_NO_ERROR)

//网络注册状态
typedef enum
{
    SIM900_NET_NOT = 0, //未注册
    SIM900_NET_YES = 1, //已经注册
    SIM900_NET_SEA = 2, //未注册,正在搜索
    SIM900_NET_TUR = 3, //注册被拒绝
    SIM900_NET_UNK = 4, //未知
    SIM900_NET_ROA = 5, //已经注册,但是漫游
    SIM900_NET_ERROR=0XFF//错误
}SIM900_NETSTATUS;

//串口与GPRS的通信状态

#define TTYS_ENOBASE	12000100

typedef enum
{
	OPEN_TTYS_ERROR = 0x01,
	SEND_DATA_ERROR,
	SET_WAITING_ERROR,
};



#define TTYS_OPEN_ERROR  (TTYS_ENOBASE + OPEN_TTYS_ERROR)
#define TTYS_SEND_DATA_ERROR  (TTYS_ENOBASE + SEND_DATA_ERROR)
#define TTYS_SET_WAITING_ERROR  (TTYS_ENOBASE + SET_WAITING_ERROR)

//
#define MAX_AT_LEN 1024 //gprs AT 指令的最大长度
#define MAX_RECV_DATA_LENGTH	1036+1024//申请2048个char的空间接收数据
#define MAX_SEND_DATA_LENGTH	1036+100//申请2048个char的空间接收数据

#define SEND_HEART_BEAT_RATE	4*60//定时发送心跳包的时间间隔，4min(建议4-5min之间)

//延时常量
#define WAITING_OVERTIME_1500MS			1500//1.5s
#define WAITING_OVERTIME_4000MS			4000//4s
#define WAITING_OVERTIME_10000MS		10000//10s
#define WAITING_OVERTIME_20000MS		20000//20s

#define AT_OK	".*OK.*" //同步波特率，这里利用它来检查模块上电情况
#define AT_OK_SIZE 10+10

#define AT_CPIN_OK ".*READY.*"//查询是否检测到SIM卡
#define AT_CPIN_OK_SIZE 22+10

#define AT_ATE0_OK	".*OK.*" //关闭回显成功
#define AT_ATE0_OK_SIZE 12

#define AT_CSQ_OK ".*\\+CSQ:\\s*(([1-2][0-9])|([0-9])|(31)|(30)),[0-7].*"//信号质量<=31
#define AT_CSQ_OK_SIZE 20+10

#define AT_CREG_OK	".*\\+CREG:\\s[0-2],[1|5].*" //已经注册到网络
#define AT_CREG_OK_SIZE 21+10

#define AT_CGATT_OK	".*\\+CGATT:\\s1.*" //已经注册到网络
#define AT_CGATT_OK_SIZE 19+10

#define AT_IP_INITIAL_OK	".*IP\\sINITIAL.*" //当前设备处于IP INITIAL状态下
#define AT_CIPSTATUS_OK_SIZE 1024

#define AT_RESPOND_OK	".*OK.*" //已经设定过APN
#define AT_RESPOND_OK_SIZE 6+20//确保出现error的现象

#define AT_CIPCLOSE_OK	".*CLOSE\\sOK.*" //关闭移动场景
#define AT_CIPCLOSE_OK_SIZE 20+20

#define AT_CIPSHUT_OK	".*SHUT\\sOK.*" //关闭移动场景
#define AT_CIPSHUT_OK_SIZE 11+20

#define AT_CIFSR_OK	".*[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}.*" //查询设备IP
#define AT_CIFSR_OK_SIZE 19+10

#define AT_CIPSTART_FAIL	".*CONNECT\\sFAIL.*" //如果不是失败们就是成功
#define AT_CIPSTART_FAIL_SIZE 20+20

#define AT_CIPSTART_OK	"(.*ALREAY\\sCONNECT.*)|(.*CONNECT\\sOK.*)" //如果不是失败们就是成功
#define AT_CIPSTART_OK_SIZE 80+20

#define AT_CIPSEND_OK	".*>.*" //匹配该数据可以发送数据
#define AT_CIPSEND_OK_SIZE 4+20

#define AT_SEND_OK	".*SEND\\sOK.*" //匹配该数据说明成功发送到远端
#define AT_SEND_OK_SIZE 14+20

const char *gprs_strerror(int errnum);
void gprs_error_print(hgprs *ctx, const char *context);

size_t strlcpy(char *dest, const char *src, size_t dest_size);
size_t sacnf_mesg_pre(uint8_t *at);
size_t sacnf_at_pre(uint8_t *at);
size_t getatlen(const uint8_t *at);


int receive_msg(hgprs *ctx, uint8_t *msg);
//int send_at_com(hgprs *ctx, uint8_t *cmd);
hgprs* gprs_link(const char *device,
                 int baud,
                 char parity,
                 int data_bit,
                 int stop_bit);
int connect_to_server(hgprs *ctx,int n);
int close_connect(hgprs *ctx,int channel_num);
int gprs_init(hgprs *ctx);
void Test(hgprs *ctx);
int send_data_pack(hgprs *ctx,int channel,uint8_t *dat,unsigned int length_to_send);
int close_all_connect(hgprs *ctx);
GPRS_END_DECLS

#endif /* GPRS_COMMON_H_ */
