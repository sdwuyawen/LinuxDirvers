#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <assert.h>  
#include <getopt.h>    
#include <fcntl.h>    
#include <unistd.h>  
#include <errno.h>  
#include <sys/stat.h>  
#include <sys/types.h>  
#include <sys/time.h>  
#include <sys/mman.h>  
#include <sys/ioctl.h>  
#include <asm/types.h>  
#include <linux/videodev2.h>  
#include <linux/fb.h>  
#define CLEAR(x) memset (&(x), 0, sizeof (x))   

#define IMG_WIDTH	320
#define IMG_HEIGHT	240
    
struct buffer {   
    void * start;   
    size_t length;   
};   
    
static char * dev_name = NULL;   
static int fd = -1;   
struct buffer * buffers = NULL;   
static unsigned int n_buffers = 0;   
static int time_in_sec_capture=5;   
static int fbfd = -1;   
static struct fb_var_screeninfo vinfo;   
static struct fb_fix_screeninfo finfo;   
static char *fbp=NULL;   
static long screensize=0;   
    
static void errno_exit (const char * s)   
{   
    fprintf (stderr, "%s error %d, %s\n",s, errno, strerror (errno));   
    exit (EXIT_FAILURE);   
}   
    
static int xioctl (int fd,int request,void * arg)   
{   
    int r;   
    do r = ioctl (fd, request, arg);   
    while (-1 == r && EINTR == errno);   
    return r;   
}   
    
inline int clip(int value, int min, int max) {   
    return (value > max ? max : value < min ? min : value);   
  }   
  
  
static void process_image (const void * p){   
    
       
    //ConvertYUVToRGB32   
 
    unsigned char* in=(char*)p;   
    int width=IMG_WIDTH;   
    int height=IMG_HEIGHT;   
    int istride=width*2;   
    int x,y,j;   
    int y0,u,y1,v,r,g,b;   
    long location=0;   
    
    for ( y = 10; y < height + 10; ++y) {   
        for (j = 0, x=10; j < width * 2 ; j += 4,x +=2) {   
          location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) +   
            (y+vinfo.yoffset) * finfo.line_length;   
               
          y0 = in[j];   
          u = in[j + 1] - 128;                   
          y1 = in[j + 2];           
          v = in[j + 3] - 128;           
    
          r = (298 * y0 + 409 * v + 128) >> 8;   
          g = (298 * y0 - 100 * u - 208 * v + 128) >> 8;   
          b = (298 * y0 + 516 * u + 128) >> 8;   
           
          fbp[ location + 0] = ((clip(g, 0, 255)&0x1c)<< 3 ) | ( clip(b, 0, 255)  >> 3 );   
          fbp[ location + 1] = ((clip(r, 0, 255) & 0xf8) ) | ( clip(g, 0, 255)>> 5);   
    
          r = (298 * y1 + 409 * v + 128) >> 8;   
          g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;   
          b = (298 * y1 + 516 * u + 128) >> 8;   
    
          fbp[ location + 2] = ((clip(g, 0, 255)&0x1c)<< 3 ) | ( clip(b, 0, 255)  >> 3 );   
          fbp[ location + 3] = ((clip(r, 0, 255) & 0xf8) ) | ( clip(g, 0, 255)>> 5);   
          }   
        in +=istride;   
       }   
}   

static void process_image_bak (const void * p){   
    
       
    //ConvertYUVToRGB32   
    1;   
    unsigned char* in=(char*)p;   
    int width=320;   
    int height=240;   
    int istride=640;   
    int x,y,j;   
    int y0,u,y1,v,r,g,b;   
    long location=0;   
    
    for ( y = 100; y < height + 100; ++y) {   
        for (j = 0, x=100; j < width * 2 ; j += 4,x +=2) {   
          location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) +   
            (y+vinfo.yoffset) * finfo.line_length;   
               
          y0 = in[j];   
          u = in[j + 1] - 128;                   
          y1 = in[j + 2];           
          v = in[j + 3] - 128;           
    
          r = (298 * y0 + 409 * v + 128) >> 8;   
          g = (298 * y0 - 100 * u - 208 * v + 128) >> 8;   
          b = (298 * y0 + 516 * u + 128) >> 8;   
           
          fbp[ location + 0] = clip(b, 0, 255);   
          fbp[ location + 1] = clip(g, 0, 255);   
          fbp[ location + 2] = clip(r, 0, 255);       
          fbp[ location + 3] = 255;       
    
          r = (298 * y1 + 409 * v + 128) >> 8;   
          g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;   
          b = (298 * y1 + 516 * u + 128) >> 8;   
    
           fbp[ location + 4] = clip(b, 0, 255);   
          fbp[ location + 5] = clip(g, 0, 255);   
          fbp[ location + 6] = clip(r, 0, 255);       
          fbp[ location + 7] = 255;       
          }   
        in +=istride;   
       }   
}   
    
static int read_frame (void)   
{   
	struct v4l2_buffer buf;   
	unsigned int i;   

	CLEAR (buf);   
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
	buf.memory = V4L2_MEMORY_MMAP;   
#if 1
	if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {   		/* VIDIOC_DQBUF：把数据放回缓存队列 */
		switch (errno) {   
		case EAGAIN:   
		return 0;   
		case EIO:       
		default:   
		errno_exit ("VIDIOC_DQBUF");   
		}   
	}   
#endif
	assert (buf.index < n_buffers);   
//	printf("v4l2_pix_format->field(%d)\n", buf.field);   
//	assert (buf.field ==V4L2_FIELD_NONE);   
	process_image (buffers[buf.index].start);   

#if 1
	if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))   		/* VIDIOC_QBUF：把数据从缓存中读取出来 */
		errno_exit ("VIDIOC_QBUF");   
#endif
	return 1;   
}   
    
static void run (void)   
{   
    unsigned int count;   
    int frames;   
    frames = 30 * time_in_sec_capture;   
    
    	while (frames-- > 0) 
	{   
		for (;;) 
		{   
			fd_set fds;   
			struct timeval tv;   
			int r;   
			FD_ZERO (&fds);   
			FD_SET (fd, &fds);   


			tv.tv_sec = 2;   
			tv.tv_usec = 0;   

			r = select (fd + 1, &fds, NULL, NULL, &tv);   

			printf("count=%d\n", ++count);   

			if (-1 == r) {   
			if (EINTR == errno)   
				continue;   
				errno_exit ("select");   
			}   

			if (0 == r) {   
				fprintf (stderr, "select timeout\n");   
				exit (EXIT_FAILURE);   
			}   

			if (read_frame ())   
				break;   

		}   
    }   
}   
    
static void stop_capturing (void)   
{   
    enum v4l2_buf_type type;   
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
    if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))   
        errno_exit ("VIDIOC_STREAMOFF");   
}   
    
static void start_capturing (void)   
{   
    unsigned int i;   
    enum v4l2_buf_type type;   
    
    for (i = 0; i < n_buffers; ++i) {   
        struct v4l2_buffer buf;   
        CLEAR (buf);   
    
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
        buf.memory = V4L2_MEMORY_MMAP;   
        buf.index = i;   
    
        if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))   
            errno_exit ("VIDIOC_QBUF");   
        }   
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
    
    if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))   
        errno_exit ("VIDIOC_STREAMON");   
       
}   
    
static void uninit_device (void)   
{   
    unsigned int i;   
    
    for (i = 0; i < n_buffers; ++i)   
        if (-1 == munmap (buffers[i].start, buffers[i].length))   
            errno_exit ("munmap");   
       
    if (-1 == munmap(fbp, screensize)) {   
          printf(" Error: framebuffer device munmap() failed.\n");   
          exit (EXIT_FAILURE) ;   
        }       
    free (buffers);   
}   
    
    
static void init_mmap (void)   
{   
    struct v4l2_requestbuffers req;   
    
    //mmap framebuffer   
        fbp = (char *)mmap(NULL,screensize,PROT_READ | PROT_WRITE,MAP_SHARED ,fbfd, 0);   
        if ((int)fbp == -1) {   
            printf("Error: failed to map framebuffer device to memory.\n");   
            exit (EXIT_FAILURE) ;   
        }   
    memset(fbp, 0, screensize);   
    CLEAR (req);   
    
    req.count = 4;   
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
    req.memory = V4L2_MEMORY_MMAP;   
    
    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {   
        if (EINVAL == errno) {   
            fprintf (stderr, "%s does not support memory mapping\n", dev_name);   
            exit (EXIT_FAILURE);   
        } else {   
            errno_exit ("VIDIOC_REQBUFS");   
        }   
    }   
    
    if (req.count < 4) {    //if (req.count < 2)   
        fprintf (stderr, "Insufficient buffer memory on %s\n",dev_name);   
        exit (EXIT_FAILURE);   
    }   
    
    buffers = calloc (req.count, sizeof (*buffers));   
    
    if (!buffers) {   
        fprintf (stderr, "Out of memory\n");   
        exit (EXIT_FAILURE);   
    }   
    
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {   
        struct v4l2_buffer buf;   
    
        CLEAR (buf);   
    
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
        buf.memory = V4L2_MEMORY_MMAP;   
        buf.index = n_buffers;   
    
        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))   
            errno_exit ("VIDIOC_QUERYBUF");   
    
        buffers[n_buffers].length = buf.length;   
        buffers[n_buffers].start =mmap (NULL,buf.length,PROT_READ | PROT_WRITE ,MAP_SHARED,fd, buf.m.offset);   
    
        if (MAP_FAILED == buffers[n_buffers].start)   
            errno_exit ("mmap");   
    }   
    
}   
    
    
    
static void init_device (void)   
{   
    struct v4l2_capability cap;   
    struct v4l2_cropcap cropcap;   
    struct v4l2_crop crop;   
    struct v4l2_format fmt;   
    unsigned int min;   
    
    
    // Get fixed screen information   
	//得到固定屏幕的信息
      if (-1==xioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {   
            printf("Error reading fixed information.\n");   
            exit (EXIT_FAILURE);   
        }   
    
        // Get variable screen information   
	//得到可变屏幕的信息
     if (-1==xioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {   
            printf("Error reading variable information.\n");   
            exit (EXIT_FAILURE);   
       }   

       screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	//输出可变屏幕和固定屏幕的信息
	printf("vinfo: xoffset:%d  yoffset:%d bits_per_pixel:%d xres:%d yres:%d\n",vinfo.xoffset, vinfo.yoffset, vinfo.bits_per_pixel, vinfo.xres, vinfo.yres);
	printf("finfo: line_length:%d  screensize:%d\n", finfo.line_length, screensize);     
    
	//获取camere的信息，复制到cap中
    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) { 
	
        if (EINVAL == errno) {   
            fprintf (stderr, "%s is no V4L2 device\n",dev_name);   
            exit (EXIT_FAILURE);   
        } else {   
            errno_exit ("VIDIOC_QUERYCAP");   
          }   
    }   

    printf("cap.capabilities = %d\n",cap.capabilities);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {   //#define V4L2_CAP_VIDEO_CAPTURE		0x00000001  /* Is a video capture device */
        fprintf (stderr, "%s is no video capture device\n",dev_name);   
        exit (EXIT_FAILURE);   
    }   
    
	
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {   
        fprintf (stderr, "%s does not support streaming i/o\n",dev_name);  //#define V4L2_CAP_STREAMING              0x04000000  /* streaming I/O ioctls */ 
        exit (EXIT_FAILURE);   
    }   
    
       
    
    CLEAR (cropcap);   
    
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   

    //VIDIOC_CROPCAP：查询驱动的修剪能力
    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {   
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
        crop.c = cropcap.defrect;   
    
        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {   
            switch (errno) {   
            case EINVAL:       
            break;   
            default:   
            break;   
            }   
        }   
    }else {     }   
    
    CLEAR (fmt);   
    /*采集信号的type width height 帧格式*/
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   
    fmt.fmt.pix.width = IMG_WIDTH;
    fmt.fmt.pix.height = IMG_HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;   //V4L2_PIX_FMT_JPEG;//V4L2_PIX_FMT_YUYV;   
 
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;   
    
	//VIDIOC_S_FMT：设置当前驱动的频捕获格式
    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))   
        errno_exit ("VIDIOC_S_FMT");   

	/* 新增加 */
	{
		/* 获取帧率 */
		struct v4l2_streamparm Stream_Parm;
		memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
		Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 

		if(0 == xioctl(fd, VIDIOC_G_PARM, &Stream_Parm))
		{
			printf("numerator: %d, denominator: %d\n",
			Stream_Parm.parm.capture.timeperframe.numerator,
			Stream_Parm.parm.capture.timeperframe.denominator);
		}
		
		/* 设置帧率 */
		memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
		Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 

		Stream_Parm.parm.capture.timeperframe.numerator = 1;
		Stream_Parm.parm.capture.timeperframe.denominator = 30;;
	
		if(0 == xioctl(fd, VIDIOC_S_PARM, &Stream_Parm))
		{
			printf("set stream_param ok\n");
		}
		
		/* 获取帧率 */
		memset(&Stream_Parm, 0, sizeof(struct v4l2_streamparm));
		Stream_Parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 

		if(0 == xioctl(fd, VIDIOC_G_PARM, &Stream_Parm))
		{
			printf("numerator: %d, denominator: %d\n",
			Stream_Parm.parm.capture.timeperframe.numerator,
			Stream_Parm.parm.capture.timeperframe.denominator);
		}
	}

	/* 新增加 结束 */
    
    init_mmap ();   
    
}   
    
static void close_device (void)   
{   
    if (-1 == close (fd))   
    errno_exit ("close");   
    fd = -1;   
    close(fbfd);   
}   
    
static void open_device (void)   
{   
    struct stat st;     
    
    if (-1 == stat (dev_name, &st)) {   
    fprintf (stderr, "Cannot identify '%s': %d, %s\n",dev_name, errno, strerror (errno));   
    exit (EXIT_FAILURE);   
    }   
    
    if (!S_ISCHR (st.st_mode)) {   
    fprintf (stderr, "%s is no device\n", dev_name);   
    exit (EXIT_FAILURE);   
    }   
    
    //open framebuffer   
        fbfd = open("/dev/fb0", O_RDWR);   
        if (fbfd==-1) {   
            printf("Error: cannot open framebuffer device.\n");   
            exit (EXIT_FAILURE);   
        }   
    
    //open camera   
    fd = open (dev_name, O_RDWR| O_NONBLOCK, 0);   
    
    if (-1 == fd) {   
    fprintf (stderr, "Cannot open '%s': %d, %s\n",dev_name, errno, strerror (errno));   
    exit (EXIT_FAILURE);   
    }   
}   
    
static void usage (FILE * fp,int argc,char ** argv)   
{   
fprintf (fp,   
"Usage: %s [options]\n\n"  
"Options:\n"  
"-d | --device name Video device name [/dev/video]\n"  
"-h | --help Print this message\n"  
"-t | --how long will display in seconds\n"  
"",   
argv[0]);   
}   
    
static const char short_options [] = "d:ht:";   
static const struct option long_options [] = {   
{ "device", required_argument, NULL, 'd' },   
{ "help", no_argument, NULL, 'h' },   
{ "time", no_argument, NULL, 't' },   
{ 0, 0, 0, 0 }   
};   
    
int main (int argc,char ** argv)   
{   
	dev_name = "/dev/video2";   
    
	for (;;)     
	{   
		int index;   
		int c;   

		c = getopt_long (argc, argv,short_options, long_options,&index);   
		if (-1 == c)   
		break;   

		switch (c) {   
			case 0:   
			break;   

			case 'd':   
			dev_name = optarg;   
			break;   

			case 'h':   
			usage (stdout, argc, argv);   
			exit (EXIT_SUCCESS);   
			case 't':   
			time_in_sec_capture = atoi(optarg);   
			break;   

			default:   
			usage (stderr, argc, argv);   
			exit (EXIT_FAILURE);   
		}   
	}   
    
	open_device ();   

	init_device ();   

	start_capturing ();   

	run ();   

	stop_capturing ();   

	uninit_device ();   

	close_device ();   

	exit (EXIT_SUCCESS);   

	return 0;   
}  


