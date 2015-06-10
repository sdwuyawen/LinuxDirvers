#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


/*
	LCD 颜色代码，CL_是Color的简写
	16Bit由高位至低位， RRRR RGGG GGGB BBBB

	下面的RGB 宏将24位的RGB值转换为16位格式。
	启动windows的画笔程序，点击编辑颜色，选择自定义颜色，可以获得的RGB值。

	推荐使用迷你取色器软件获得你看到的界面颜色。
*/
#define RGB(R,G,B)	(((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3))	/* 将8位R,G,B转化为 16位RGB565格式 */
#define RGB565_R(x)  ((x >> 8) & 0xF8)
#define RGB565_G(x)  ((x >> 3) & 0xFC)
#define RGB565_B(x)  ((x << 3) & 0xF8)
enum
{
	CL_WHITE        = RGB(255,255,255),	/* 白色 */
	CL_BLACK        = RGB(  0,  0,  0),	/* 黑色 */
	CL_RED          = RGB(255,	0,  0),	/* 红色 */
	CL_GREEN        = RGB(  0,255,  0),	/* 绿色 */
	CL_BLUE         = RGB(  0,	0,255),	/* 蓝色 */
	CL_YELLOW       = RGB(255,255,  0),	/* 黄色 */

	CL_GREY			= RGB( 98, 98, 98), 	/* 深灰色 */
	CL_GREY1		= RGB( 150, 150, 150), 	/* 浅灰色 */
	CL_GREY2		= RGB( 180, 180, 180), 	/* 浅灰色 */
	CL_GREY3		= RGB( 200, 200, 200), 	/* 最浅灰色 */
	CL_GREY4		= RGB( 230, 230, 230), 	/* 最浅灰色 */

	CL_BUTTON_GREY	= RGB( 220, 220, 220), /* WINDOWS 按钮表面灰色 */

	CL_MAGENTA      = 0xF81F,	/* 红紫色，洋红色 */
	CL_CYAN         = 0x7FFF,	/* 蓝绿色，青色 */

	CL_BLUE1        = RGB(  0,  0, 240),		/* 深蓝色 */
	CL_BLUE2        = RGB(  0,  0, 128),		/* 深蓝色 */
	CL_BLUE3        = RGB(  68, 68, 255),		/* 浅蓝色1 */
	CL_BLUE4        = RGB(  0, 64, 128),		/* 浅蓝色1 */

	/* UI 界面 Windows控件常用色 */
	CL_BTN_FACE		= RGB(236, 233, 216),	/* 按钮表面颜色(灰) */
	CL_BOX_BORDER1	= RGB(172, 168,153),	/* 分组框主线颜色 */
	CL_BOX_BORDER2	= RGB(255, 255,255),	/* 分组框阴影线颜色 */


	CL_MASK			= 0x9999	/* 颜色掩码，用于文字背景透明 */
};


/* oled_test init
 * oled_test clear
 * oled_test clear <page>
 * oled_test <page> <col> <string>
 */

#define TFT_CMD_INIT       		0x100001
#define TFT_CMD_CLEAR_ALL  		0x100002




void print_usage(char *cmd)
{
    printf("Usage:\n");
    printf("%s init\n", cmd);
    printf("%s clear\n", cmd);
    printf("%s clear <page>\n", cmd);
    printf("%s <page> <col> <string>\n", cmd);
    printf("eg:\n");
    printf("%s 2 0 100ask.taobao.com\n", cmd);
    printf("page is 0,1,...,7\n");
    printf("col is 0,1,...,127\n");
}

int main(int argc, char **argv)
{
	int fd;

	fd = open("/dev/spigpio_tft", O_RDWR);
	if (fd < 0)
	{
		printf("can't open /dev/oled\n");
		return -1;
	}

	ioctl(fd, TFT_CMD_INIT);

//	ioctl(fd, TFT_CMD_CLEAR_ALL, 0xFF);
	ioctl(fd, TFT_CMD_CLEAR_ALL, CL_RED);
	ioctl(fd, TFT_CMD_CLEAR_ALL, CL_GREEN);
	ioctl(fd, TFT_CMD_CLEAR_ALL, CL_BLUE);

	return 0;
}

