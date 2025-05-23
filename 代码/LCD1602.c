/*************************************************************
                      LCD1602源文件

实现功能：LCD1602的控制

***************************************************************/
#include <LCD1602.h> // 包含头文件以获取声明和引脚定义
#include <reg52.h>   // 包含寄存器定义
#include <intrins.h> // 包含 _nop_

/*****************LCD1602自定义字符"℃↑↓"*******************/
// 定义 CGRAM_TABLE 数组
unsigned char code CGRAM_TABLE[] = {
    // ℃↑↓符号字模数据
    0x00,0x0E,0x0A,0x0E,0x00,0x00,0x00,0x00, // ℃ (Index 0)
    0x04,0x0A,0x15,0x04,0x04,0x04,0x04,0x00, // ↑ (Index 1)
    0x00,0x04,0x04,0x04,0x04,0x15,0x0A,0x04  // ↓ (Index 2)
};

/********************************************************
函数名称:void delay_n40us(uint n)
函数作用:LCD1602延时函数 (粗略延时)
参数说明:n - 延时参数 (每个循环约40us @ 11.0592MHz)
********************************************************/
void delay_n40us(uint n)
{
    while(n--)
    {
        // 16个 _nop_ 大约是 16 * 12 / 11.0592 = 17.3 us
        // 这里使用32个nop，大约35us，加上循环开销接近40us
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
        _nop_(); _nop_(); _nop_(); _nop_();
    }
}

/********************************************************
函数名称:void Timer0_Delay(uint ms)
函数作用:使用定时器0实现精确延时
参数说明:ms - 延时毫秒数
********************************************************/
void Timer0_Delay(uint ms)
{
    TMOD &= 0xF0; // 清除T0配置位
    TMOD |= 0x01; // 设置定时器0为模式1 (16位定时器)
    TR0 = 0;      // 停止定时器0
    ET0 = 0;      // 禁止定时器0中断
    for(; ms > 0; ms--)
    {
        TH0 = 0xFC; // 1ms定时初值 (@11.0592MHz: 65536 - 11059200 / 12 / 1000 = 64614 -> FC66H)
        TL0 = 0x66; // 使用 FC66H 以获得更精确的1ms
        TF0 = 0;    // 清除溢出标志
        TR0 = 1;    // 启动定时器0
        while(!TF0); // 等待定时器0溢出
        TR0 = 0;    // 停止定时器0
    }
}

/********************************************************
函数名称:void LCD_write_command(uchar command)
函数作用:LCD1602写命令
参数说明:command为指令，参考数据手册
********************************************************/
void LCD_write_command(uchar command)
{
    LCD_RS = 0; // 选择指令寄存器
    LCD_RW = 0; // 选择写模式
    LCD_DB = command; // 发送指令
    delay_n40us(1); // Ts1: 地址建立时间 > 40ns (这里给1个循环约40us，足够)
    LCD_E = 1;  // 拉高使能信号
    // Tpw: E脉冲宽度 > 230ns (delay_n40us(1) ~40us >> 230ns)
    delay_n40us(1);
    LCD_E = 0;  // 拉低使能信号
    delay_n40us(1); // Th1: 地址保持时间 > 10ns (这里给~40us)
    // 指令执行时间不同，最长1.64ms (清屏)，其他大部分<40us
    // 在调用处根据需要加延时，尤其是清屏和归位指令后
}

/********************************************************
函数名称:void LCD_write_data(uchar dat)
函数作用:LCD1602写数据
参数说明:dat - 要写入的数据
********************************************************/
void LCD_write_data(uchar dat)
{
    LCD_RS = 1; // 选择数据寄存器
    LCD_RW = 0; // 选择写模式
    LCD_DB = dat; // 发送数据
    delay_n40us(1); // Ts1 > 40ns
    LCD_E = 1;  // 拉高使能信号
    // Tpw > 230ns
    delay_n40us(1);
    LCD_E = 0;  // 拉低使能信号
    delay_n40us(1); // Th1 > 10ns
}

/********************************************************
函数名称:void LCD_init(void)
函数作用:LCD1602初始化函数
参数说明:无
********************************************************/
void LCD_init(void)
{
    Timer0_Delay(15); // 上电延时 > 15ms
    LCD_write_command(0x38); // 功能设置：8位数据，2行显示，5x7点阵 (无需延时)
    Timer0_Delay(5);  // 延时 > 4.1ms
    LCD_write_command(0x38); // 第二次功能设置
    Timer0_Delay(1);  // 延时 > 100us
    LCD_write_command(0x38); // 第三次功能设置
    Timer0_Delay(1);

    // 从这里开始可以查询忙标志，但为了简化，使用固定延时
    LCD_write_command(0x38); // 确认功能设置: 8位，2行，5x7
    Timer0_Delay(1);
    LCD_write_command(0x08); // 显示关闭
    Timer0_Delay(1);
    LCD_clear(); // 调用清屏函数
    LCD_write_command(0x06); // 进入点设置：光标右移，屏幕不移动
    Timer0_Delay(1);
    LCD_write_command(0x0C); // 显示开，关光标，不闪烁
    Timer0_Delay(1);
}

/********************************************************
函数名称:void LCD_clear(void)
函数作用:清除LCD屏幕
参数说明:无
********************************************************/
void LCD_clear(void)
{
    LCD_write_command(0x01); // 发送清屏指令
    Timer0_Delay(2);         // 清屏指令需要较长延时 > 1.64ms
}

/********************************************************
函数名称:void LCD_disp_char(uchar x, uchar y, uchar dat)
函数作用:LCD1602在指定位置显示一个字符
参数说明:x - 列坐标 (0-15), y - 行坐标 (1-2), dat - 要显示的字符
********************************************************/
void LCD_disp_char(uchar x, uchar y, uchar dat)
{
    uchar address;
    if (y == 1)
    {
        address = 0x80 + x; // 第一行地址
    }
    else if (y == 2)
    {
        address = 0xC0 + x; // 第二行地址
    }
    else
    {
        return; // 无效行号
    }
    LCD_write_command(address); // 设置 DDRAM 地址
    LCD_write_data(dat);        // 写入字符数据
}

/********************************************************
函数名称:void lcd1602_write_character(uchar x, uchar y, uchar *s)
函数作用:LCD1602在指定位置开始显示一个字符串
参数说明:x - 列坐标 (0-15), y - 行坐标 (1-2), *s - 要显示的字符串指针
********************************************************/
void lcd1602_write_character(uchar x, uchar y, uchar *s)
{
    uchar address;
    if (y == 1)
    {
        address = 0x80 + x; // 第一行地址
    }
    else if (y == 2)
    {
        address = 0xC0 + x; // 第二行地址
    }
    else
    {
        return; // 无效行号
    }
    LCD_write_command(address); // 设置起始 DDRAM 地址
    while (*s != '\0')          // 使用正确的 null terminator ' '
    {
        LCD_write_data(*s++);
    }
}

/********************************************************
函数名称:void lcd1602_write_pic(uchar add, uchar *pic_num)
函数作用:LCD1602写入自定义字符数据到CGRAM
参数说明:add - 自定义字符索引 (0-7), *pic_num - 8字节字模数据指针
********************************************************/
void lcd1602_write_pic(uchar add, uchar *pic_num)
{
    unsigned char i;
    if (add > 7) return; // 最多8个自定义字符
    LCD_write_command(CGRAM_ADDR(add)); // 设置 CGRAM 地址
    for(i = 0; i < 8; i++)              // 写入8字节的字模数据
    {
        LCD_write_data(pic_num[i]);     // 注意：这里假设pic_num指向RAM，如果指向code，需要调整
        // 如果 pic_num 指向 code 存储器，应该使用指针访问: LCD_write_data(*(pic_num+i));
        // 但由于 CGRAM_TABLE 是 code 类型，调用时如 lcd1602_write_pic(0, &CGRAM_TABLE[0])
        // 传递的是 code 指针，直接用 pic_num[i] 或 *(pic_num+i) 都可以，编译器能处理。
    }
    LCD_write_command(0x80); // 操作完CGRAM后，将地址指针重新设置到DDRAM，避免后续显示混乱
}
