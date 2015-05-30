/*
 * gprs_sim900.c
 *
 *  Created on: 2014年10月10日
 *      Author: LDD
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include <assert.h>

#include "gprs_sim900.h"
#include "gprs_common.h"

static int fd_gpio,fd_irq;//gpio的配置文件，句柄，fd_irq中断控制句柄

/*
*函数名称:           sim900_irq
*函数原型:           static void sim900_irq(int signum);
*函数功能:           sim900的RI线上有上升沿后会触发该函数，比如收到短消息
*函数返回:           无
*参数说明:           触发该中断的信号
*所属文件:           <gprs_sim900.c>
 */
static void sim900_irq(int signum)
{
	static int rc=0;
	rc++;
	printf("%d times enter the sim900_irq，signal number is：%d\n",rc,signum);
}
/*
*函数名称:           sim900_IO_init
*函数原型:           int sim900_IO_init(void);
*函数功能:           sim900的开关机以及收到短信、数据的时候需要GPIO的控制，初始化GPIO
*函数返回:           返回rc>0成功
*参数说明:           ctx-包含串口句柄,为了控制调试加入
*所属文件:           <gprs_sim900.c>
 */
static int sim900_IO_init(hgprs *ctx)
{
	int rc=0;
	int oflags;
	errno = ER_NOERROR;//假设没有错误

	fd_gpio = open("/dev/em9280_gpio", O_RDWR);
	if(fd_gpio<0)
	{
	printf("can not open /dev/em9280_gpio error code :%d\n", rc);
	errno = ER_GPRSIOE;
	return fd_gpio;
	}

	if(ctx->basic_status.debug)
	printf("open file /dev/em9280_gpio = %d\n", fd_gpio);

	//设定来短信后的提示
	fd_irq = open("/dev/em9280_irq1", O_RDONLY);
	if (fd_irq < 0)
	{
		printf("can not open /dev/em9280_irq1 device file!\n");

		errno = ER_GPRSIOE;
		return fd_irq;
	}
	if(ctx->basic_status.debug)
	printf("open file /dev/em9280_irq1 = %d\n", fd_irq);

	signal(SIGIO, sim900_irq);    //  让em9280_irq_handler()处理SIGIO信号
	fcntl(fd_irq, F_SETOWN, getpid());//F_SETOWN 设置异步I/O所有权
	oflags = fcntl(fd_irq, F_GETFL);//F_GETFL 获得文件描述符标记，返回一个正的进程ID
	fcntl(fd_irq, F_SETFL, oflags | FASYNC);  //  F_SETFL 设置文件状态标记

	//对GPIO进行初始化配置
	rc = GPIO_OutEnable(fd_gpio,SIM900_ON|SIM900_DTR);
	if(rc<0)
	{
		printf("GPIO6_OutEnable::failed %d\n", rc);
		errno = ER_GPRSIOE;
		return rc;
	}
	return rc;
}
/*
*函数名称:           _sim900_on_off
*函数原型:           static int _sim900_on_off(hgprs *ctx);
*函数功能:           需要对Sim900开关机的时候调用此函数
*函数返回:           返回rc>0成功
*参数说明:           ctx-包含串口句柄,为了控制调试加入
*所属文件:           <gprs_sim900.c>
 */
static int _sim900_on_off(hgprs *ctx)
{
	int rc;
	errno = ER_NOERROR;//假设没有错误

	//拉高SIM900_ON，保持1.2s以上
	if(ctx->basic_status.debug)
	printf("Keep sim900's PWRKEY pin output high for 1.2s,waiting----\n");

	rc = GPIO_OutSet(fd_gpio, SIM900_ON);
	if(rc < 0)
	{
		printf("GPIO_OutSet::failed %d\n", rc);
		errno = ER_GPRSIOE;
		return rc;
	}

	usleep(1200000);

	if(ctx->basic_status.debug)
		printf("Keep sim900's PWRKEY pin output low for 1s-----\n");
	//释放SIM900_ON
	rc = GPIO_OutClear(fd_gpio, SIM900_ON);
	if(rc < 0)
	{
		printf("GPIO_OutClear::failed %d\n", rc);
		errno = ER_GPRSIOE;
		return rc;
	}
	usleep(1000000);//保证GPRS模块能够开机正常

	if(ctx->basic_status.debug)
	printf("it should work\n");

	return rc;
}
/* Sets up a serial port for sim900 communications */
/*
*函数名称:           _sim900_connect
*函数原型:           static int _sim900_connect(hgprs *ctx);
*函数功能:           sim900的模块需要与串口连接，在这里根据ctx->ttys_property的属性打开相关串口资源
*函数返回:           返回rc>0成功
*参数说明:           ctx-标志一个连接
*所属文件:           <gprs_sim900.c>
 */
static int _sim900_connect(hgprs *ctx)
{
    struct termios tios;
    speed_t speed;

    if (ctx->basic_status.debug) {
        printf("Opening %s at %d bauds (%c, %d, %d)\n",
        		ctx->ttys_property.device, ctx->ttys_property.baud, ctx->ttys_property.parity,
        		ctx->ttys_property.data_bit, ctx->ttys_property.stop_bit);
    }

    /* The O_NOCTTY flag tells UNIX that this program doesn't want
       to be the "controlling terminal" for that port. If you
       don't specify this then any input (such as keyboard abort
       signals and so forth) will affect your process

       Timeouts are ignored in canonical input mode or when the
       NDELAY option is set on the file via open or fcntl */
    ctx->s = open(ctx->ttys_property.device, O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL);
    if (ctx->s == -1) {
        fprintf(stderr, "ERROR Can't open the device %s (%s)\n",
        		ctx->ttys_property.device, strerror(errno));
        return -1;
    }

    /* Save */
    tcgetattr(ctx->s, &(ctx->ttys_property.old_tios));

    memset(&tios, 0, sizeof(struct termios));

    /* C_ISPEED     Input baud (new interface)
       C_OSPEED     Output baud (new interface)
    */
    switch (ctx->ttys_property.baud) {
    case 110:
        speed = B110;
        break;
    case 300:
        speed = B300;
        break;
    case 600:
        speed = B600;
        break;
    case 1200:
        speed = B1200;
        break;
    case 2400:
        speed = B2400;
        break;
    case 4800:
        speed = B4800;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    default:
        speed = B9600;
        if (ctx->basic_status.debug) {
            fprintf(stderr,
                    "WARNING Unknown baud rate %d for %s (B9600 used)\n",
                    ctx->ttys_property.baud, ctx->ttys_property.device);
        }
    }

    /* Set the baud rate */
    if ((cfsetispeed(&tios, speed) < 0) ||
        (cfsetospeed(&tios, speed) < 0)) {
        close(ctx->s);
        ctx->s = -1;
        return -1;
    }

    /* C_CFLAG      Control options
       CLOCAL       Local line - do not change "owner" of port
       CREAD        Enable receiver
    */
    tios.c_cflag |= (CREAD | CLOCAL);
    /* CSIZE, HUPCL, CRTSCTS (hardware flow control) */

    /* Set data bits (5, 6, 7, 8 bits)
       CSIZE        Bit mask for data bits
    */
    tios.c_cflag &= ~CSIZE;
    switch (ctx->ttys_property.data_bit) {
    case 5:
        tios.c_cflag |= CS5;
        break;
    case 6:
        tios.c_cflag |= CS6;
        break;
    case 7:
        tios.c_cflag |= CS7;
        break;
    case 8:
    default:
        tios.c_cflag |= CS8;
        break;
    }

    /* Stop bit (1 or 2) */
    if (ctx->ttys_property.stop_bit == 1)
        tios.c_cflag &=~ CSTOPB;
    else /* 2 */
        tios.c_cflag |= CSTOPB;

    /* PARENB       Enable parity bit
       PARODD       Use odd parity instead of even */
    if (ctx->ttys_property.parity == 'N') {
        /* None */
        tios.c_cflag &=~ PARENB;
    } else if (ctx->ttys_property.parity == 'E') {
        /* Even */
        tios.c_cflag |= PARENB;
        tios.c_cflag &=~ PARODD;
    } else {
        /* Odd */
        tios.c_cflag |= PARENB;
        tios.c_cflag |= PARODD;
    }

    /* Read the man page of termios if you need more information. */

    /* This field isn't used on POSIX systems
       tios.c_line = 0;
    */

    /* C_LFLAG      Line options

       ISIG Enable SIGINTR, SIGSUSP, SIGDSUSP, and SIGQUIT signals
       ICANON       Enable canonical input (else raw)
       XCASE        Map uppercase \lowercase (obsolete)
       ECHO Enable echoing of input characters
       ECHOE        Echo erase character as BS-SP-BS
       ECHOK        Echo NL after kill character
       ECHONL       Echo NL
       NOFLSH       Disable flushing of input buffers after
       interrupt or quit characters
       IEXTEN       Enable extended functions
       ECHOCTL      Echo control characters as ^char and delete as ~?
       ECHOPRT      Echo erased character as character erased
       ECHOKE       BS-SP-BS entire line on line kill
       FLUSHO       Output being flushed
       PENDIN       Retype pending input at next read or input char
       TOSTOP       Send SIGTTOU for background output

       Canonical input is line-oriented. Input characters are put
       into a buffer which can be edited interactively by the user
       until a CR (carriage return) or LF (line feed) character is
       received.

       Raw input is unprocessed. Input characters are passed
       through exactly as they are received, when they are
       received. Generally you'll deselect the ICANON, ECHO,
       ECHOE, and ISIG options when using raw input
    */

    /* Raw input */
    tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    /* C_IFLAG      Input options

       Constant     Description
       INPCK        Enable parity check
       IGNPAR       Ignore parity errors
       PARMRK       Mark parity errors
       ISTRIP       Strip parity bits
       IXON Enable software flow control (outgoing)
       IXOFF        Enable software flow control (incoming)
       IXANY        Allow any character to start flow again
       IGNBRK       Ignore break condition
       BRKINT       Send a SIGINT when a break condition is detected
       INLCR        Map NL to CR
       IGNCR        Ignore CR
       ICRNL        Map CR to NL
       IUCLC        Map uppercase to lowercase
       IMAXBEL      Echo BEL on input line too long
    */
    if (ctx->ttys_property.parity == 'N') {
        /* None */
        tios.c_iflag &= ~INPCK;
    } else {
        tios.c_iflag |= INPCK;
    }

    /* Software flow control is disabled */
    tios.c_iflag &= ~(IXON | IXOFF | IXANY);

    /* C_OFLAG      Output options
       OPOST        Postprocess output (not set = raw output)
       ONLCR        Map NL to CR-NL

       ONCLR ant others needs OPOST to be enabled
    */

    /* Raw ouput */
    tios.c_oflag &=~ OPOST;

    /* C_CC         Control characters
       VMIN         Minimum number of characters to read
       VTIME        Time to wait for data (tenths of seconds)

       UNIX serial interface drivers provide the ability to
       specify character and packet timeouts. Two elements of the
       c_cc array are used for timeouts: VMIN and VTIME. Timeouts
       are ignored in canonical input mode or when the NDELAY
       option is set on the file via open or fcntl.

       VMIN specifies the minimum number of characters to read. If
       it is set to 0, then the VTIME value specifies the time to
       wait for every character read. Note that this does not mean
       that a read call for N bytes will wait for N characters to
       come in. Rather, the timeout will apply to the first
       character and the read call will return the number of
       characters immediately available (up to the number you
       request).

       If VMIN is non-zero, VTIME specifies the time to wait for
       the first character read. If a character is read within the
       time given, any read will block (wait) until all VMIN
       characters are read. That is, once the first character is
       read, the serial interface driver expects to receive an
       entire packet of characters (VMIN bytes total). If no
       character is read within the time allowed, then the call to
       read returns 0. This method allows you to tell the serial
       driver you need exactly N bytes and any read call will
       return 0 or N bytes. However, the timeout only applies to
       the first character read, so if for some reason the driver
       misses one character inside the N byte packet then the read
       call could block forever waiting for additional input
       characters.

       VTIME specifies the amount of time to wait for incoming
       characters in tenths of seconds. If VTIME is set to 0 (the
       default), reads will block (wait) indefinitely unless the
       NDELAY option is set on the port with open or fcntl.
    */
    /* Unused because we use open with the NDELAY option */
    tios.c_cc[VMIN] = 0;
    tios.c_cc[VTIME] = 0;

    if (tcsetattr(ctx->s, TCSANOW, &tios) < 0) {
        close(ctx->s);
        ctx->s = -1;
        return -1;
    }

    ctx->basic_status.ttys_open = TRUE;

    return 0;
}
/*
*函数名称:           _sim900_close
*函数原型:           static void _sim900_close(hgprs *ctx);
*函数功能:           关闭sim900与系统连接的串口资源
*函数返回:           无
*参数说明:           ctx-标志一个连接
*所属文件:           <gprs_sim900.c>
 */
static void _sim900_close(hgprs *ctx)
{

	errno = ER_NOERROR;//假设没有错误

    tcsetattr(ctx->s, TCSANOW, &(ctx->ttys_property.old_tios));
    close(ctx->s);

    ctx->basic_status.ttys_open = FALSE;

    if(ctx->basic_status.debug)
    printf("sim900 closed!\n");

}
/*
*函数名称:           _sim900_close
*函数原型:           static void _sim900_close(hgprs *ctx);
*函数功能:           对异步事件的处理，等待事件发生
*函数返回:           返回值>0表示事件到来
*参数说明:           ctx-标志一个连接，rfds-读事件标志，tv-超时结构体，length_to_read-要读取的字节数
*所属文件:           <gprs_sim900.c>
 */
static int _sim900_select(hgprs *ctx, fd_set *rfds,
                       struct timeval *tv, int length_to_read)
{
    int s_rc;

    while ((s_rc = select(ctx->s+1, rfds, NULL, NULL, tv)) == -1) {
        if (errno == EINTR) {
            if (ctx->basic_status.debug) {
                fprintf(stderr, "A non blocked signal was caught\n");
            }
            /* Necessary after an error */
            FD_ZERO(rfds);
            FD_SET(ctx->s, rfds);
        } else {
            return -1;
        }
    }

    if (s_rc == 0) {
        /* Timeout */
        errno = ETIMEDOUT;
        return -1;
    }

    return s_rc;
}
/*
*函数名称:           _sim900_send
*函数原型:           ssize_t _sim900_send(hgprs  *ctx, const uint8_t *req, int req_length);
*函数功能:           将数据通过串口发送出去
*函数返回:           返回值发送出去的数据的字节数
*参数说明:           ctx-标志一个连接，req-要发送的数据串，req_length-要发送的字节数
*所属文件:           <gprs_sim900.c>
 */
ssize_t _sim900_send(hgprs  *ctx, const uint8_t *req, int req_length)
{

	int rc;
	uint8_t i;
	rc = write(ctx->s, req, req_length);
	if (ctx->basic_status.debug) {

		printf("Send %d bytes:\t",rc);
		for(i =0;i<req_length;i++)
		printf("%c",req[i]);
		printf("\n");
	        }

	return rc;
}
/*
*函数名称:           _sim900_recv
*函数原型:           ssize_t _sim900_recv(hgprs  *ctx, uint8_t *rsp, int rsp_length);
*函数功能:           读取缓冲区中的数据，非阻塞的方式
*函数返回:           返回值实际接收到的字节数
*参数说明:           ctx-标志一个连接，rsq-接受到的数据，rsq_length-要接受的数据长度
*所属文件:           <gprs_sim900.c>
 */
ssize_t _sim900_recv(hgprs  *ctx, uint8_t *rsp, int rsp_length)
{
    int rc;
	int i;
	rc = read(ctx->s, rsp, rsp_length);
		if (ctx->basic_status.debug) {

			printf("Recv %d bytes :\t",rc);
			for(i =0;i<rc;i++)
			printf("%c",rsp[i]);
			printf("\n");
		        }

	return rc;
}
/*
*函数名称:           _enable_dbg
*函数原型:           static void _enable_dbg(hgprs *ctx,uint8_t flag);
*函数功能:           使能调试
*函数返回:           无
*参数说明:           ctx-标志一个连接，flag-TRUE使能，FLASE禁止
*所属文件:           <gprs_sim900.c>
 */
static void _enable_dbg(hgprs *ctx,uint8_t flag)
{
	ctx->basic_status.debug = flag;
}
/*
*函数名称:           _gprs_set_heart_char
*函数原型:           static void _gprs_set_heart_char(hgprs *ctx,char heart);
*函数功能:           设定心跳发送的字符
*函数返回:           无
*参数说明:           ctx-标志一个连接，heart-心跳字符
*所属文件:           <gprs_sim900.c>
 */
static void _gprs_set_heart_char(hgprs *ctx,char heart)
{
	ctx->heart_beat_char = heart;
}
/*
*函数名称:           _gprs_set_ip_port
*函数原型:           static void _gprs_set_ip_port(hgprs *ctx,const char *ip,int port);
*函数功能:           设定要连接的网络
*函数返回:           >0表示成功
*参数说明:           ctx-标志一个连接，ip-需要连接的外网ip，port-要连接的端口号, n-对应第n个连接
*所属文件:           <gprs_sim900.c>
 */
static int _gprs_set_ip_port(hgprs *ctx,const char *ip,int port,int n)
{
	size_t dest_size;
	size_t ret_size;

	if(n>8||n<1)//连接数小于8
	{
	errno = EINVAL;
	return -1;
	}

	char *object_at = &ctx->tcp_link_at[n-1];

	errno = ER_NOERROR;//假设没有错误

    dest_size = sizeof(ctx->bandip);
	ret_size = strlcpy(ctx->bandip, ip, dest_size);
	if (ret_size == 0) {
	   fprintf(stderr, "The ip string is empty\n");
	   if (ctx != NULL)
	   {
		free(ctx);
	   }
	   errno = EINVAL;
	}


	dest_size = sizeof(ctx->tcp_link_at[n-1]);
	ret_size = sprintf(object_at, "AT+CIPSTART=%d,\"TCP\",",n-1);
	//ret_size = strlcpy(object_at, "AT+CIPSTART=\"TCP\",",n, dest_size);
	if (ret_size == 0) {
	   fprintf(stderr, "Set ctx->tcp_link_at string error\n");
	   if (ctx != NULL)
	   {
		free(ctx);
	   }
	   errno = EINVAL;
	}

	object_at = strcat(object_at,ip);//AT+CIPSTART="TCP","128.58.140.130"
	object_at = strcat(object_at,",");//AT+CIPSTART="TCP","128.58.140.130",
	if(object_at==NULL)
	{
		fprintf(stderr, "Cat ctx->tcp_link_at ip string error\n");
		if (ctx != NULL)
		{
		free(ctx);
		}
		errno = EINVAL;
	}


	if(port>1024&&port<65535)
	{
	ctx->port = port;
	}
	else
	{
	if (ctx != NULL)
	{
	free(ctx);
	}
	errno = EINVAL;
	}

	char portstr[6];
	if(sprintf(portstr,"%d",port)<0)
	{
	if (ctx != NULL)
	{
	free(ctx);
	}
	errno = EINVAL;
	}

	object_at = strcat(object_at,portstr);//AT+CIPSTART="TCP","128.58.140.130",13000
	if(object_at==NULL)
	{
		fprintf(stderr, "Cat ctx->tcp_link_at port string error\n");
		if (ctx != NULL)
		{
		free(ctx);
		}
		errno = EINVAL;
	}


	if(ctx->basic_status.debug)
	{
		printf("tcp_link_at channel %d: %s \n",n,&ctx->tcp_link_at[n-1]);
	}

	return 1;
}
/*
*函数名称:           _gprs_init_common
*函数原型:           void _gprs_init_common(hgprs *ctx);
*函数功能:           初始化参数
*函数返回:           无
*参数说明:           ctx-标志一个连接
*所属文件:           <gprs_sim900.c>
 */
static void _gprs_init_common(hgprs *ctx)
{
	/*默认属性配置*/
    /* Handle are initialized to -1 */
    ctx->s = -1;

    ctx->net_status=IP_INITIAL;
    ctx->basic_status.auto_echo = true;
    ctx->basic_status.debug = false;
    ctx->basic_status.ttys_open = false;
    ctx->basic_status.power_on = false;
    ctx->basic_status.enable_heartbeat = false;
    ctx->basic_status.init_sucess = false;
    ctx->basic_status.work_machine = false;

    ctx->sm_satus = SM_NOT_OPNE;

    ctx->heart_beat_char = 0x12;//现在随便写一个

    ctx->response_timeout.tv_sec = 0;
    ctx->response_timeout.tv_usec = _RESPONSE_TIMEOUT;

    ctx->byte_timeout.tv_sec = 0;
    ctx->byte_timeout.tv_usec = _BYTE_TIMEOUT;

    memset(ctx->channel_status,false,TCP_LINK_CHANNEL_NUM*sizeof(bool));
    memset(ctx->tcp_link_at,0,TCP_LINK_CHANNEL_NUM*46*sizeof(char));

}
const gprs_func_t gprs_func_backend = {
		_sim900_connect,
		_sim900_close,
		_sim900_send,
		_sim900_recv,
		_enable_dbg,
		NULL,
		_sim900_select,
		NULL,
		_gprs_set_ip_port,
		_gprs_set_heart_char,
		_sim900_on_off,
};

/*
*函数名称:           gprs_link
*函数原型:           hgprs* gprs_link(const char *device,
                         	 	 	  int baud,
                         	 	 	  char parity,
                         	 	 	  int data_bit,
                         	 	 	  int stop_bit,
                         	 	 	  const char *ip,
                         	 	 	  int port)
*函数功能:           新建一个与GPRS模块的（sim900）连接，并配置连接属性；使能GPIO
*函数返回:           返回包含连接属性的结构体
*参数说明:           device-选择串口（ex:"/dev/ttys5"）
 	 	 	 	 	 baud-串口波特率
 	 	 	 	 	 parity-奇偶校验
 	 	 	 	 	 data_bit-数据位
 	 	 	 	 	 stop_bit-停止位
 	 	 	 	 	 ip-要连接的目标公网的IP
 	 	 	 	 	 port-要连接的公网端口
*所属文件:           <gprs_common.h>
 */
//新建一个gprs的连接，申请控制句柄，配置参数，还没有进行正式的连接
hgprs* gprs_link(const char *device,
                         int baud, char parity, int data_bit,
                         int stop_bit)
{
    hgprs *ctx;

    size_t dest_size;
    size_t ret_size;


    errno = ER_NOERROR;//假设没有错误

    ctx = (hgprs *) malloc(sizeof(hgprs));
    _gprs_init_common(ctx);

    ctx->backend = &gprs_func_backend;//赋值函数指针

    dest_size = sizeof(ctx->ttys_property.device);
    ret_size = strlcpy(ctx->ttys_property.device, device, dest_size);
    if (ret_size == 0) {
        fprintf(stderr, "The device string is empty\n");

        if (ctx != NULL)
        {
		free(ctx);
        }

        errno = EINVAL;
        return NULL;
    }

    if (ret_size >= dest_size) {
        fprintf(stderr, "The device string has been truncated\n");
        if (ctx != NULL)
        {
		free(ctx);
        }
        errno = EINVAL;
        return NULL;
    }

    ctx->ttys_property.baud = baud;
    if (parity == 'N' || parity == 'E' || parity == 'O') {
    	ctx->ttys_property.parity = parity;
    } else {
	   if (ctx != NULL)
		{
		free(ctx);
		}
        errno = EINVAL;
        return NULL;
    }
    ctx->ttys_property.data_bit = data_bit;
    ctx->ttys_property.stop_bit = stop_bit;


    sim900_IO_init(ctx);//初始化GPIO

    return ctx;
}








