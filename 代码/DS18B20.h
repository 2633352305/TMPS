
#ifndef _DS18B20_H_
#define _DS18B20_H_
#include <reg52.H>	 
#include <intrins.H>
#include <math.h>

#define  uchar unsigned char
#define  uint unsigned int

/*****************Function Prototypes*******************/
extern void Delay_ms(unsigned int ms); // 外部精确毫秒延时
#define delayms Delay_ms

/*****************错误代码定义*******************/
#define ERR_SENSOR_TIMEOUT 0xFF  // 传感器响应超时
#define ERR_SENSOR_RETRY 3       // 传感器重试次数

sbit DS18B20_DQ = P2^7;    // DS18B20数据线连接到P2.7

// 温度数据（在main.c中定义）
extern unsigned char temp_z;    // 温度整数部分
extern unsigned char temp_x;    // 温度小数部分

// 滤波算法参数
#define FILTER_DEPTH 5    // 增加滤波深度获得更好平滑结果
static int temp_buffer[FILTER_DEPTH]; // 温度缓冲区
static uchar buffer_index = 0;  // 缓冲区当前索引
static bit buffer_full = 0;     // 缓冲区是否满标志
static uint last_valid_temp = 0; // 上次有效温度值


/*****************DS18B20错误/状态代码定义*******************/
#define DS18B20_OK    0
#define DS18B20_ERROR 1
#define DS18B20_INVALID_TEMP 0xFFFF // 表示无效温度的返回值

/********************************************************
* 功  能：DS18B20复位时序（满足1-Wire协议时序要求）
* 参  数：无
* 返回值：无
********************************************************/
void DS18B20_Reset(void)
{
    uint i;
    DS18B20_DQ = 0;        // 拉低总线启动复位
    i = 103;               // 480μs复位脉冲（12MHz时钟）
    while(i>0) i--;
    DS18B20_DQ = 1;        // 释放总线等待传感器响应
    i = 4;                 // 70μs响应等待（12MHz时钟）
    while(i>0) i--;
}

/********************************************************
* 功  能：DS18B20 读取一位数据
* 参  数：无
* 返回值：读取的位
********************************************************/
bit DS18B20_ReadBit(void)	
{
    uint i;
    bit dat;
    
    DS18B20_DQ = 0;
    i++;
    DS18B20_DQ = 1;
    i++; i++;
    dat = DS18B20_DQ;
    
    i = 8;
    while(i>0) i--;
    
    return (dat);
}
/********************************************************
* 功  能：DS18B20 读取一个字节
* 参  数：无
* 返回值：读取的字节
********************************************************/
uchar DS18B20_ReadByte(void)	
{
    uchar i, j, dat;
    dat = 0;
    for(i=1; i<=8; i++)
    {
        j = DS18B20_ReadBit();
        dat = (j<<7) | (dat>>1);   // 先读出的放在最高位，向前推
    }
    return(dat);
}

/********************************************************
* 功  能：DS18B20 写入一个字节
* 参  数：要写入的字节
* 返回值：无
********************************************************/
void DS18B20_WriteByte(uchar dat)	
{
    uint i;
    uchar j;
    bit b;
    
    for(j=1; j<=8; j++)
    {
        b = dat & 0x01;
        dat = dat >> 1;
        
        if(b)     // write 1
        {
            DS18B20_DQ = 0;
            i++; i++;
            DS18B20_DQ = 1;
            i = 8;
            while(i>0) i--;
        }
        else      // write 0
        {
            DS18B20_DQ = 0;
            i = 8;
            while(i>0) i--;
            DS18B20_DQ = 1;
            i++; i++;
        }
    }
}

/********************************************************
* 功  能：DS18B20 开始温度转换
* 参  数：无
* 返回值：无
********************************************************/
void DS18B20_StartConvert(void)	
{
    DS18B20_Reset();
    delayms(5);
    DS18B20_WriteByte(0xCC); // 跳过ROM命令
    DS18B20_WriteByte(0x44); // 开始温度转换命令
}

/********************************************************
* 功  能：DS18B20 得到温度值
* 参  数：无
* 返回值：温度值（放大10倍）
********************************************************/
int DS18B20_GetTemp(void)
{
    float t;
    uchar a, b;
    int temp;
    
    DS18B20_Reset();
    delayms(5);
    DS18B20_WriteByte(0xCC); // 跳过ROM命令
    DS18B20_WriteByte(0xBE); // 读取暂存器命令
    a = DS18B20_ReadByte(); // 读取低字节
    b = DS18B20_ReadByte(); // 读取高字节
    
    temp = b;
    temp <<= 8;             // 高字节左移一个字节位置
    temp = temp | a;
    t = temp * 0.0625;      // 转换为实际温度值
    temp = t * 10 ;    // 放大10倍并四舍五入
    
    return temp;
}

/********************************************************
* 功  能：滑动平均滤波算法
* 参  数：新的温度值
* 返回值：滤波后的温度值
********************************************************/
int DS18B20_Filter(int new_temp)
{
    int sum = 0;
    uchar i, count;
    int avg_temp;
    
    // 动态阈值检测（单位：0.1℃）
    int delta = abs(new_temp - last_valid_temp);
    
    if (last_valid_temp != 0) {
        if (delta > 15) { // 超过1.5℃视为无效
            return DS18B20_INVALID_TEMP;
        } else if (delta > 5) { // 0.5℃~1.5℃使用6:4加权
            temp_buffer[buffer_index] = (last_valid_temp * 6 + new_temp * 4) / 10;
        } else { // 小于0.5℃直接更新缓冲区
            temp_buffer[buffer_index] = new_temp;
        }
    } else {
        // 初始化状态：首次测量直接存入缓冲区
        temp_buffer[buffer_index] = new_temp;
    }
    
    // 更新缓冲区索引
    buffer_index++;
    if (buffer_index >= FILTER_DEPTH) {
        buffer_index = 0;
        buffer_full = 1; // 缓冲区已经满了
    }
    
        // 计算缓冲区内的平均值
    for (i = 0; i < count; i++) {
        sum += temp_buffer[i];
    }
    avg_temp = count > 0 ? sum / count : new_temp;
    
    return avg_temp;  // 添加缺失的返回值
}

/********************************************************
* 功  能：初始化DS18B20
* 参  数：无
* 返回值：无
********************************************************/
void DS18B20_Init(void)
{
    // 复位DS18B20
    DS18B20_Reset();
    
    // 复位缓冲区
    buffer_index = 0;
    buffer_full = 0;
    last_valid_temp = 0;
    
    // 设置DS18B20分辨率为12位（默认）
    DS18B20_Reset();
    Delay_ms(5);
    DS18B20_WriteByte(0xCC); // 跳过ROM命令
    DS18B20_WriteByte(0x4E); // 写暂存器命令
    DS18B20_WriteByte(0x00); // TH寄存器 = 0
    DS18B20_WriteByte(0x00); // TL寄存器 = 0
    DS18B20_WriteByte(0x7F); // 配置寄存器 = 0x7F (12位精度)
}

/********************************************************
* 功  能：DS18B20更新并获取温度变量
* 参  数：无
* 返回值：无
********************************************************/
void DS18B20_Convert(void)
{
    int temp;
    uchar retry = 0;
    int filtered_temp;
    
    // 增加错误处理机制，最多重试3次
    while (retry < 3) {
        // 开始温度转换
        DS18B20_StartConvert();
        
        // 等待转换完成 - 增加等待时间确保转换完全完成
        delayms(800);  // 12位精度需要最多750ms，增加余量保证
        
        // 读取温度值
        temp = DS18B20_GetTemp();
        // 检查温度是否有效
        if (temp > 100 && temp < 400) { // 有效温度范围：10度C 到 40度C
            break; // 读取成功，跳出循环
        }
        
        retry++;
        delayms(10); // 短暂延时后重试
    }
    
    // 更新全局温度变量
    if (retry < 3) {
        // 将有效温度进行滤波处理
        filtered_temp = DS18B20_Filter(temp);
        
        // 处理无效温度数据
        if(filtered_temp == DS18B20_INVALID_TEMP) {
            temp_z = 99;
            temp_x = 9;
            last_valid_temp = 0;
        }
        
        // 应用温度补偿到滤波后的结果
        
        // 成功读取温度
        if(filtered_temp < 0) {
            temp_z = 0;
            temp_x = 0;
        } 
        else if(filtered_temp <= 999) {
            temp_z = filtered_temp / 10;
            temp_x = filtered_temp % 10;
        } 
        else {
            temp_z = 99;
            temp_x = 9;
        }
    } else {
        // 读取失败
        temp_z = 88; // 特殊数值表示错误
        temp_x = 8;
    }
}

#endif