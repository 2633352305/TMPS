/*************************************************************
                      LCD1602头文件
 
实现功能：LCD1602的控制

补充说明：
***************************************************************/
#ifndef _LCD1602_H_
#define _LCD1602_H_
#include<reg52.h>
#include<intrins.h> // intrins.h for _nop_

#define uchar unsigned char
#define uint unsigned int

/*****************LCD1602引脚定义*******************/
#define LCD_DB P0  //数据口D0~D7
sbit LCD_RS = 0x91;//数据/命令选择 引脚 (P1^1)
sbit LCD_RW = 0x92;//读/写选择 引脚 (P1^2)
sbit LCD_E  = 0x94;//使能信号 引脚 (P1^4)

/*****************LCD1602自定义字符地址*******************/
#define CGRAM_ADDR(n) (0x40 | ((n) << 3))

/*****************外部变量/数组声明*******************/
extern unsigned char code CGRAM_TABLE[]; // 声明 CGRAM_TABLE 为外部 code 类型
extern unsigned char pic, zeng, jian; // 声明在 main.c 中定义的变量

/*****************LCD1602函数声明*******************/
void Timer0_Delay(uint ms);                            // 使用定时器0的精确延时
void LCD_init(void);								   // 初始化函数
void LCD_write_command(uchar command);				   // 写指令函数
void LCD_write_data(uchar dat);						   // 写数据函数
void LCD_disp_char(uchar x, uchar y, uchar dat);	   // 显示一个字符
void lcd1602_write_character(uchar x, uchar y, uchar *s); // 显示一个字符串
void lcd1602_write_pic(uchar add, uchar *pic_num);      // 写入自定义字符
void LCD_clear(void);                                  // 清屏函数
void delay_n40us(uint n);                              // 粗略延时函数

#endif // 确保文件末尾有 #endif