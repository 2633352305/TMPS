/*************************************************************
                      PS002头文件
 
实现功能：PS002的控制

功能说明：PS002模拟信号传感器通过ADC0832CCN模数转换大气压
***************************************************************/
#ifndef _PS002_H_
#define _PS002_H_
#include <reg52.H>	 
#include <LCD1602.h>
#include <intrins.H> 
 
#define  uchar   unsigned char
#define  uint    unsigned int
#define  ADC_REF   510       // 基准电压5.10V（定点数放大100倍）
#define  ADC_RES   25600    // 8位ADC分辨率（定点数放大100倍）
#define  KPA_COEF  177830   // kPa转换系数（定点数放大1000倍）
#define  ERR_ADC_TIMEOUT  0xFF  // ADC读取超时错误码

/*****************PS002引脚定义*******************/
sbit CS_0832 = P2^6;    // ADC0832片选信号
sbit CLK_0832 = P2^5;   // ADC0832时钟信号
sbit DO_0832 = P2^4;    // ADC0832数据输出
sbit DI_0832 = P2^4;    // ADC0832数据输入(DI和DO共用同一个引脚)

/*****************PS002变量定义*******************/
#define CH0 0x00       // 通道0选择参数
#define CH1 0x01       // 通道1选择参数

static int ad1, v1, v2, p;     // AD值、初始电压值、当前电压值、压力值

/*****************PS002函数定义*******************/
void PS002_delay(uint ms);                // 毫秒延时函数
void PS002_init();                       // PS002初始化
unsigned char PS002_read0832();          // 读取ADC0832数值
void PS002_Convert();                    // PS002读取并转换

// 定义脉冲函数，用于ADC0832时钟
#define pulse0832() _nop_();_nop_();

#endif
