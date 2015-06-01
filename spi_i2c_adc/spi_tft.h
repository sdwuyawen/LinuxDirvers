
#ifndef _LCD_DRIVER_H_
#define _LCD_DRIVER_H_

//-----------------------------SPI 总线配置--------------------------------------//
#define USE_HARDWARE_SPI     0		//1:Enable Hardware SPI;0:USE Soft SPI

//-------------------------屏幕物理像素设置--------------------------------------//
#define LCD_X_SIZE	        176
#define LCD_Y_SIZE	        220

/////////////////////////////////////用户配置区///////////////////////////////////	 
//支持横竖屏快速定义切换
#define USE_HORIZONTAL  		0	//定义是否使用横屏 		0,不使用.1,使用.

#ifdef USE_HORIZONTAL//如果定义了横屏 
#define X_MAX_PIXEL	        LCD_Y_SIZE
#define Y_MAX_PIXEL	        LCD_X_SIZE
#else
#define X_MAX_PIXEL	        LCD_X_SIZE
#define Y_MAX_PIXEL	        LCD_Y_SIZE
#endif
//////////////////////////////////////////////////////////////////////////////////
	 


#define RED  	0xf800
#define GREEN	0x07e0
#define BLUE 	0x001f
#define WHITE	0xffff
#define BLACK	0x0000
#define YELLOW  0xFFE0
#define GRAY0   0xEF7D   	//灰色0 3165 00110 001011 00101
#define GRAY1   0x8410      	//灰色1      00000 000000 00000
#define GRAY2   0x4208      	//灰色2  1111111111011111


#if 0
//本测试程序使用的是模拟SPI接口驱动
//可自由更改接口IO配置，使用任意最少4 IO即可完成本款液晶驱动显示
/******************************************************************************
接口定义在Lcd_Driver.h内定义，请根据接线修改并修改相应IO初始化LCD_GPIO_Init()
#define LCD_CTRL   	  	GPIOA		//定义TFT数据端口
#define LCD_LED        	接高电平    
#define LCD_RS         	GPIO_Pin_3	//PB10连接至TFT --RS
#define LCD_CS        	GPIO_Pin_4 //PB11 连接至TFT --CS
#define LCD_RST     	接单片机复位脚
#define LCD_SCL        	GPIO_Pin_5	//PB13连接至TFT -- CLK
#define LCD_SDA        	GPIO_Pin_7	//PB15连接至TFT - SDI
*******************************************************************************/
#define LCD_CTRL   	  	GPIOA		 //定义TFT数据端口
#define LCD_LED        	GPIO_Pin_xx  
#define LCD_RS         	GPIO_Pin_3	
#define LCD_CS        	GPIO_Pin_4  
#define LCD_RST     	GPIO_Pin_xx	
#define LCD_SCL        	GPIO_Pin_5	
#define LCD_SDA        	GPIO_Pin_7


//#define LCD_CS_SET(x) LCD_CTRL->ODR=(LCD_CTRL->ODR&~LCD_CS)|(x ? LCD_CS:0)

//液晶控制口置1操作语句宏定义
#define	LCD_CS_SET  	LCD_CTRL->BSRR=LCD_CS    
#define	LCD_RS_SET  	LCD_CTRL->BSRR=LCD_RS    
#define	LCD_SDA_SET  	LCD_CTRL->BSRR=LCD_SDA    
#define	LCD_SCL_SET  	LCD_CTRL->BSRR=LCD_SCL    
#define	LCD_RST_SET  	//LCD_CTRL->BSRR=LCD_RST    
#define	LCD_LED_SET  	//LCD_CTRL->BSRR=LCD_LED   

//液晶控制口置0操作语句宏定义
#define	LCD_CS_CLR  	LCD_CTRL->BRR=LCD_CS    
#define	LCD_RS_CLR  	LCD_CTRL->BRR=LCD_RS    
#define	LCD_SDA_CLR  	LCD_CTRL->BRR=LCD_SDA    
#define	LCD_SCL_CLR  	LCD_CTRL->BRR=LCD_SCL    
#define	LCD_RST_CLR  	//LCD_CTRL->BRR=LCD_RST    
#define	LCD_LED_CLR  	//LCD_CTRL->BRR=LCD_LED 


#define LCD_DATAOUT(x) LCD_DATA->ODR=x; //数据输出
#define LCD_DATAIN     LCD_DATA->IDR;   //数据输入

#define LCD_WR_DATA(data){\
LCD_RS_SET;\
LCD_CS_CLR;\
LCD_DATAOUT(data);\
LCD_WR_CLR;\
LCD_WR_SET;\
LCD_CS_SET;\
} 

#endif


void Lcd_WriteIndex(unsigned char Index);
void Lcd_WriteData(unsigned char Data);
void Lcd_WriteReg(unsigned char Index,unsigned char Data);
unsigned short Lcd_ReadReg(unsigned char LCD_Reg);
void Lcd_Reset(void);
void Lcd_Init(void);
void Lcd_SetXY(unsigned short x,unsigned short y);
void Gui_DrawPoint(unsigned short x,unsigned short y,unsigned short Data);
unsigned int Lcd_ReadPoint(unsigned short x,unsigned short y);
void Lcd_SetRegion(unsigned char x_start,unsigned char y_start,unsigned char x_end,unsigned char y_end);
void Lcd_WriteData_16Bit(unsigned short Data);

void spi_tft_init(void);
void lcd_clear(unsigned short color);

#endif

