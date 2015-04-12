#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
//#include <asm/types.h>
#include <linux/videodev2.h>

#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
//typedef FLOAT               *PFLOAT;
//typedef BOOL near           *PBOOL;
//typedef BOOL far            *LPBOOL;
//typedef BYTE near           *PBYTE;

typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

struct buffer {
	void *                  start;   //一般指针有两种属性，指向变量/对象的地址和长度，指针只存储地址，长度取决于指针的类型  void *只有地址，没有定义类型
	size_t                  length;
};

static char *           dev_name        = "/dev/video0";//摄像头设备名
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;

FILE *file_fd;  FILE *hf;
static unsigned long file_length;
static unsigned char *file_name;     //全局变量和静态全局变量的区别：都是静态存储方式，但全局变量可为多个源文件共同使用，静态全局变量只在本文件中使用
//////////////////////////////////////////////////////
//获取一帧数据
//////////////////////////////////////////////////////

BYTE clip255(long v)
{
	if(v < 0) v=0;
	else if( v > 255) v=255;
	return (BYTE)v;
}
void yuv2bmp(BYTE *YUV2buff,BYTE *RGBbuff,DWORD dwSize)
{
    DWORD count;
	for(  count = 0; count < dwSize; count += 4 )
	{
		//Y0 U0 Y1 V0
		BYTE Y0 = *YUV2buff;
		BYTE U  = *(++YUV2buff);
		BYTE Y1 = *(++YUV2buff);
		BYTE V  = *(++YUV2buff);
		++YUV2buff;
		long Y,C,D,E;
		BYTE R,G,B;
		Y = Y0;
		C = Y - 16;
		D = U - 128;
		E = V - 128;
		R = clip255(( 298 * C           + 409 * E + 128) >> 8);
		G = clip255(( 298 * C - 100 * D - 208 * E + 128) >> 8);
		B = clip255(( 298 * C + 516 * D           + 128) >> 8);
		
		*(RGBbuff)   = B;
		*(++RGBbuff) = G;
		*(++RGBbuff) = R;
		Y = Y1;
		C = Y-16;
		D = U-128;
		E = V-128;
		R = clip255(( 298 * C           + 409 * E + 128) >> 8);
		G = clip255(( 298 * C - 100 * D - 208 * E + 128) >> 8);
		B = clip255(( 298 * C + 516 * D           + 128) >> 8);
		*(++RGBbuff) = B;
		*(++RGBbuff) = G;
		*(++RGBbuff) = R;
		++RGBbuff;
	}
}

unsigned char a[153600];
unsigned char rgbout[230400]="";
unsigned char rgbout2[230400]="";


static int read_frame (void)         //函数声明为static，只能在本文件中调用
{
	struct v4l2_buffer buf;
	unsigned int i;
	
	CLEAR (buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	
	ioctl (fd, VIDIOC_DQBUF, &buf); //出列采集的帧缓冲
	
	assert (buf.index < n_buffers);
	printf ("buf.index dq is %d,\n",buf.index);
	
	fwrite(buffers[buf.index].start, buffers[buf.index].length, 1, file_fd); //将其写入文件中
	
	ioctl (fd, VIDIOC_QBUF, &buf); //再将其入列
	
	return 1;
}

int v4l2_get_fmt()
{
    struct v4l2_format my_fmt;
	
}



int main (int argc,char ** argv)
{
	struct v4l2_capability cap;
	struct v4l2_format fmt,get_fmt;
	unsigned int i;
	enum v4l2_buf_type type;
	unsigned int format;
	file_fd = fopen("test-mmap.txt", "w");//图片文件名
	
	fd = open (dev_name, O_RDWR | O_NONBLOCK, 0);//打开设备，一般用fopen打开普通文件，绕过进入系统内核，比较节省时间；一般用open打开设备文件，不得不进入内核
	
	///ioctl (fd, VIDIOC_QUERYCAP, &cap);//获取摄像头参数
	
	
	CLEAR (fmt);
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = 320;//640
	fmt.fmt.pix.height      = 240;//480
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//V4L2_PIX_FMT_RGB32;//YUYV
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
	ioctl (fd, VIDIOC_S_FMT, &fmt); //设置图像格式
	printf("fmt.fmt.pix.width    %d \n ",fmt.fmt.pix.width);
	printf("fmt.fmt.pix.bytesperline   %d \n",fmt.fmt.pix.bytesperline);
	
	
	CLEAR(get_fmt);
	get_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if(-1 == ioctl(fd,VIDIOC_G_FMT,&get_fmt))
		printf("VIDIOC_G_FMT");
	format = fmt.fmt.pix.pixelformat;
	char code[5];
	int j = 0;
	for(j = 0; j < 4; j++)
		code[j] = (format & (0xff << j*8)) >> j*8;
	code[4] = 0;
	printf("format(human readble):%s\n",code);
	
	
	file_length = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height; //计算图片大小
	printf("file_length is %d  %ld",file_length,file_length);
	
	struct v4l2_requestbuffers req;
	CLEAR (req);
	req.count               = 4;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;
	
	ioctl (fd, VIDIOC_REQBUFS, &req); //申请缓冲，count是申请的数量
	
	if (req.count < 2)
		printf("Insufficient buffer memory\n");
	
	buffers = calloc (req.count, sizeof (*buffers));//内存中建立对应空间
	
	for (n_buffers = 0; n_buffers < req.count; ++n_buffers)   //这个for循环内部的buf，只在此循环内有效，循环结束就没有了；
	{                     //关于临时变量，可以参考文章http://blog.csdn.net/Sasoritattoo/archive/2011/01/13/6133662.aspx
		struct v4l2_buffer buf;   //驱动中的一帧-----------
		CLEAR (buf);
		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = n_buffers;
		
		printf("buf.length is %d",buf.length);
		if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buf)) //映射用户空间
			printf ("VIDIOC_QUERYBUF error\n");
		
		buffers[n_buffers].length = buf.length;
		printf("buf.length  %d\n\n",buf.length);
		printf("buf.length is displayed");
		buffers[n_buffers].start =
			mmap (NULL ,    //通过mmap建立映射关系
			buf.length,
			PROT_READ | PROT_WRITE ,
			MAP_SHARED ,
			fd, buf.m.offset);
		
		if (MAP_FAILED == buffers[n_buffers].start)
			printf ("mmap failed\n");
		//        }                //这里注释的一大段不用也可以，用了反而不易让人看懂。
		//printf("\n\n");
		//for (i = 0; i < n_buffers; ++i)
		//{
		//  struct v4l2_buffer buf;
		//  CLEAR (buf);
		//printf("\nbuf.length is %d; ",buf.length);
		//   buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		//   buf.memory      = V4L2_MEMORY_MMAP;
		//   buf.index       = i;
		printf("buf.length is %d;",buf.length);
		if (-1 == ioctl (fd, VIDIOC_QBUF, &buf))//申请到的缓冲进入列队
			printf ("VIDIOC_QBUF failed\n");
		printf("buf.length is %d\n.",buf.length);
	}
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	if (-1 == ioctl (fd, VIDIOC_STREAMON, &type)) //开始捕捉图像数据
		printf ("VIDIOC_STREAMON failed\n");
	else printf("VIDIOC_STREAMON succeed\n");
	
	for (;;) //这一段涉及到异步IO
	{
		fd_set fds;
		struct timeval tv;
		int r;
		
		FD_ZERO (&fds);//将指定的文件描述符集清空
		FD_SET (fd, &fds);//在文件描述符集合中增加一个新的文件描述符
		
		
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		
		r = select (fd + 1, &fds, NULL, NULL, &tv);//判断是否可读（即摄像头是否准备好），tv是定时
		
		if (-1 == r) {
			if (EINTR == errno)
				continue;
			printf ("select err\n");
		}
		if (0 == r) {
			fprintf (stderr, "select timeout\n");
			exit (EXIT_FAILURE);
		}
		
		if (read_frame ())//如果可读，执行read_frame ()函数，并跳出循环
			break;
	}
	
unmap:
	for (i = 0; i < n_buffers; ++i)
		if (-1 == munmap (buffers[i].start, buffers[i].length))
			printf ("munmap error");
		close (fd);
		fclose (file_fd);
		printf("end-begin");
		FILE *p;
		p=fopen("test-mmap.txt","r");
		fread(a,1,153600,p);
		fclose(p);
		unsigned char *yuv=a;
		printf("1\n");
		
		unsigned char *q=rgbout;
		yuv2bmp(yuv,rgbout,153600);
		printf("2\n");
		
		
		
		
		int j0,k;
		for ( j0 =0 ; j0<240; j0++)
		{
			for ( k=0; k <320*3 ;k++)
			{
				rgbout2[k+320*j0*3] = rgbout[320*(240-j0-1)*3+k] ;
			}
		}
		
		
		//  HANDLE hf = CreateFile(
		//  "F://Frame.bmp", GENERIC_WRITE, FILE_SHARE_READ, NULL,
		//   CREATE_ALWAYS, NULL, NULL );
		
		hf=fopen("frame.bmp","w");
		if( hf == -1 )return 0;
		//if( hf == INVALID_HANDLE_VALUE )return 0;
		typedef struct tagBITMAPFILEHEADER {
			WORD    bfType;
			DWORD   bfSize;
			WORD    bfReserved1;
			WORD    bfReserved2;
			DWORD   bfOffBits;
		} BITMAPFILEHEADER;
		typedef struct tagBITMAPINFOHEADER{
			DWORD      biSize;
			long       biWidth;
			long       biHeight;
			WORD       biPlanes;
			WORD       biBitCount;
			DWORD      biCompression;
			DWORD      biSizeImage;
			long       biXPelsPerMeter;
			long       biYPelsPerMeter;
			DWORD      biClrUsed;
			DWORD      biClrImportant;
		} BITMAPINFOHEADER;
		// 写文件头
		BITMAPFILEHEADER bfh;
		// printf("bfh1=%d\n",sizeof(WORD)+sizeof(DWORD)+sizeof(WORD)+sizeof(WORD)+sizeof(DWORD));
		memset( &bfh, 0,sizeof(BITMAPFILEHEADER) );
		bfh.bfType = 'MB';
		bfh.bfSize = 14+ 230400 + sizeof( BITMAPINFOHEADER );
		bfh.bfOffBits = 14+ sizeof( BITMAPFILEHEADER );
		// DWORD dwWritten = 0;
		// WriteFile( hf, &bfh, sizeof( bfh ), &dwWritten, NULL );
		printf("bfh=%d\n",sizeof(BITMAPFILEHEADER));
		printf("word=%d\n",sizeof(WORD));
		printf("Dword=%d\n",sizeof(DWORD));
		fwrite(&bfh.bfType,sizeof(WORD),1,hf);
		fwrite(&bfh.bfSize,sizeof(DWORD),1,hf);
		fwrite(&bfh.bfReserved1,sizeof(WORD),1,hf);
		fwrite(&bfh.bfReserved2,sizeof(WORD),1,hf);
		fwrite(&bfh.bfOffBits,sizeof(DWORD),1,hf);
		// 写位图格式
		BITMAPINFOHEADER bih;
		memset( &bih, 0, sizeof( bih ) );
		bih.biSize = sizeof( bih );
		bih.biWidth =320;
		bih.biHeight = 240;
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		fwrite(&bih,sizeof(bih),1,hf);
		// printf("bih=%d\n",sizeof(bih));
		//WriteFile( hf, &bih, sizeof( bih ), &dwWritten, NULL );
		// 写位图数据
		//WriteFile( hf, rgbout2, 921600, &dwWritten, NULL );
		// CloseHandle( hf );
		fwrite(rgbout2,230400,1,hf);
		fclose(hf);
		//exit (EXIT_SUCCESS);
		
		return 0;
}