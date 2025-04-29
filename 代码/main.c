/*************************************************************
                         电动自行车胎压监测系统

功能说明：使用PS002模拟信号传感器通过ADC0832CCN模数转换胎压
          使用DS18B20数字温度传感器测量轮胎温度
          使用PWM控制电机转速，实现限速功能
***************************************************************/
#include <reg52.h>
#include <LCD1602.h>
#include <PS002.h>
#include <DS18B20.h>
#include <PWM.h>

// 类型定义
#define uchar unsigned char
#define uint unsigned int

// 常量定义
#define MOTOR_SPEED_MAX 100
#define TEMP_UPDATE_INTERVAL 200
#define PWM_PERIOD 100
#define DEFAULT_TEMP_UP 45
#define DEFAULT_TEMP_DOWN 10
#define DISPLAY_REFRESH_RATE 20  // 显示刷新率(ms)
#define PWM_TIMER_VALUE 0xFC66   // 定时器1的1ms定时值

// ASCII字符表 - 用于数字显示（0-9的ASCII码）
unsigned char code ASCII[] = {
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39 // '0'-'9'的ASCII码
};

// 声明外部 CGRAM_TABLE (在 LCD1602 模块中定义)
extern unsigned char code CGRAM_TABLE[]; 

// 位定义
sbit temp_led = P3^6;    // P3^6 温度超限指示灯
sbit tire_led = P3^7;    // P3^7 胎压超限指示灯
sbit buzz = P2^3;        // P2^3 蜂鸣器
sbit key_set = P1^0;     // P1^0 设置键
sbit key_jia = P1^3;     // P1^3 加键
sbit key_jian = P1^6;    // P1^6 减键
sbit DO_DI_0832 = P2^4;  // P2^4 ADC0832数据输入输出

// 全局变量声明
static unsigned char pwm_count = 0;
static unsigned char temp_count = 0;
static bit temp_update_flag = 0;
static bit alarm = 0;
static unsigned char motor_speed = MOTOR_SPEED_MAX;
static uchar error_count = 0;  // 添加error_count全局变量


// 胎压和温度变量
unsigned char pressure_scaled_x10; // 新变量：压力值 * 10
uchar temp_z; // 温度整数部分
uchar temp_x; // 温度小数部分

// 声明外部变量
extern unsigned int adc_value;    // ADC转换结果，在PS002.c中定义

// 设置变量
uchar set_f = 0;           // 设置选择标记
                          // =0非设置，=1设置胎压上限，=2设置胎压下限
uchar T0_num = 0;         // 计数变量
uchar display_mode = 0;   // 显示模式：0-温度和胎压，1-转速

// 自定义字符变量
unsigned char pic = 0;  // 自定义字符-度数符号索引

// 温度和胎压限值
uchar temp_up = DEFAULT_TEMP_UP;     // 温度上限值
uchar temp_down = DEFAULT_TEMP_DOWN; // 温度下限值
uchar tire_up = 40; // 4.0 Bar * 10
uchar tire_down = 32; // 3.2 Bar * 10

// 函数声明
void fixed_display(void);
void display_tire_info(void);
void scan(void);
void display(void);
void Delay_ms(uint ms);
void PWM_init(void);
void check_alarm(void);
void fixed_display_set(void);

// 定时器1中断服务函数
void Timer1_Interrupt() interrupt 3 using 0
{
    // 重新加载定时器值(1ms周期)
    TH1 = (PWM_TIMER_VALUE >> 8) & 0xFF;
    TL1 = PWM_TIMER_VALUE & 0xFF;
    
    // PWM控制 - 根据占空比控制输出
    PWM_OUT = (++pwm_count < motor_speed) ? 1 : 0;
    if(pwm_count >= PWM_PERIOD) pwm_count = 0;
    
    // 温度更新 - 每200ms(200个周期)触发一次
    if(++temp_count >= TEMP_UPDATE_INTERVAL) {
        temp_count = 0;
        temp_update_flag = 1;
    }
}

/********************************************************
函数名称:void fixed_display(void)
函数功能:LCD1602显示固定的内容 (恢复温度标签)
参数说明:无
********************************************************/
void fixed_display(void)
{
    LCD_clear(); // 使用我们定义的清屏函数

    if(display_mode == 0) // 温度和胎压显示模式
    {
        lcd1602_write_character(0,1,"Tire:");
        lcd1602_write_character(12,1,"Bar");

        // 恢复温度标签
        lcd1602_write_character(0,2,"Temp:");
        LCD_disp_char(12,2,pic);  // 显示自定义字符'°' (使用全局变量 pic)
        LCD_disp_char(13,2,'C');   // 显示'C'字
    }
    else // 转速显示模式
    {
        lcd1602_write_character(0,1,"Speed:");
        lcd1602_write_character(12,1,"%");
        lcd1602_write_character(0,2,"Mode: Auto");
    }
}

/********************************************************
函数名称:void display_tire_info(void)
函数功能:显示轮胎的胎压和温度信息 (使用新变量)
参数说明:
    无
********************************************************/
void display_tire_info(void) {
    uchar pres_int, pres_dec;

    // 第一行：显示胎压 X.Y Bar
    pres_int = pressure_scaled_x10 / 10; // 获取整数部分 (e.g., 33 -> 3)
    pres_dec = pressure_scaled_x10 % 10; // 获取小数部分 (e.g., 33 -> 3)
    LCD_disp_char(7,1, ASCII[pres_int]);     // 显示整数位
    LCD_disp_char(8,1,'.');
    LCD_disp_char(9,1, ASCII[pres_dec]);     // 显示小数位

    // 第二行：显示温度值 XX.Y °C
    LCD_disp_char(7,2, ASCII[temp_z/10]);
    LCD_disp_char(8,2, ASCII[temp_z%10]);
    LCD_disp_char(9,2, '.');
    LCD_disp_char(10,2, ASCII[temp_x]);

}

/********************************************************
函数名称:void fixed_display_set(void)
函数功能:LCD1602显示设置界面的固定内容
参数说明:无
********************************************************/
void fixed_display_set(void)
{
    LCD_clear(); 
    lcd1602_write_character(0,1,"P Up:"); 
    lcd1602_write_character(8,1,"Down:");
    lcd1602_write_character(0,2,"Speed:");
    lcd1602_write_character(9,2,"%");
    // 在 display 函数中显示数值
}

/********************************************************
函数名称:void scan(void)
函数功能:扫描按键，处理正常模式切换和设置模式调整
参数说明:无
********************************************************/
void scan(void)
{
  
    // 阈值限制
    uchar TIRE_MIN = 10; // 1.0 Bar
    uchar TIRE_MAX = 50; // 5.0 Bar
    signed char TEMP_MIN_INT = -55; // -55 C
    uchar TEMP_MAX_INT = 125; // 125 C

    // 1. 处理设置键 (key_set - P1.0)
    if(key_set == 0) {
        Delay_ms(5); // 消抖
        if(key_set == 0) {
            if (set_f == 0) { // 从正常模式进入设置模式
                set_f = 1; // 进入设置胎压上限
                fixed_display_set(); // 显示设置界面固定文本
            } else { // 循环切换设置项或退出
                set_f++;
                if (set_f > 2) { // <-- 修改退出条件为 > 2
                    set_f = 0;
                    // TODO: 此处可以添加保存阈值到 EEPROM 的逻辑
                    fixed_display(); // 恢复正常显示界面
                } else {
                   // 切换到下一个设置项，不需要重绘固定文本，只需 display 函数更新闪烁
                   T0_num = 0; // 重置闪烁计时器，立即显示新选中项
                }
            }
            while(!key_set); // 等待按键释放
        }
    }

    // 2. 处理加键 (key_jia - P1.3) - 仅在设置模式下有效
    if(set_f > 0 && key_jia == 0) {
        Delay_ms(1); // 消抖
        if(key_jia == 0) {
            T0_num = 0; // 重置闪烁计时，立即显示新值
            switch(set_f) {
                case 1: // 设置胎压上限
                    if (tire_up < TIRE_MAX) tire_up++;
                    break;
                case 2: // 设置胎压下限
                    // 确保下限不超过上限
                    if (tire_down < tire_up && tire_down < TIRE_MAX) tire_down++; 
                    break;
            }
            while(!key_jia); // 等待按键释放
        }
    }

    // 3. 处理减键 (key_jian - P1.6) - 仅在设置模式下有效
    if(set_f > 0 && key_jian == 0) {
        Delay_ms(1); // 消抖
        if(key_jian == 0) {
            T0_num = 0; // 重置闪烁计时，立即显示新值
            switch(set_f) {
                case 1: // 设置胎压上限
                    // 确保上限不低于下限
                    if (tire_up > tire_down && tire_up > TIRE_MIN) tire_up--;
                    break;
                case 2: // 设置胎压下限
                    if (tire_down > TIRE_MIN) tire_down--;
                    break;
            }
            while(!key_jian); // 等待按键释放
        }
    }
}

/********************************************************
函数名称:void display(void)
函数功能:根据当前模式显示数据
参数说明:无
********************************************************/
void display(void)
{
    if (set_f == 0) { // 非设置状态
        if (display_mode == 0) { // 温度和胎压显示模式
            display_tire_info();
        } else { // 转速显示模式
            // 显示电机速度
            LCD_disp_char(7, 1, ASCII[motor_speed / 10]); 
            LCD_disp_char(8, 1, ASCII[motor_speed % 10]);
            lcd1602_write_character(7, 2, alarm ? "Limited" : "Normal ");
        }
    } else { // 设置状态
        // 设置模式下的显示
        T0_num++;
        
        // 始终在第二行显示当前速度
        LCD_disp_char(6, 2, ASCII[motor_speed / 10]); // 调整速度显示位置
        LCD_disp_char(7, 2, ASCII[motor_speed % 10]);

        if(T0_num % 2 == 0) {
            // 显示胎压设置值 (第一行)
            LCD_disp_char(5,1,ASCII[tire_up / 10]);
            LCD_disp_char(6,1,'.');
            LCD_disp_char(7,1,ASCII[tire_up % 10]);
            LCD_disp_char(13,1,ASCII[tire_down / 10]);
            LCD_disp_char(14,1,'.');
            LCD_disp_char(15,1,ASCII[tire_down % 10]);
        } else {
            // 闪烁显示当前设置项 (只影响第一行)
            switch(set_f) {
                case 1: lcd1602_write_character(5,1,"   "); break;
                case 2: lcd1602_write_character(13,1,"   "); break;
                default: break; // set_f 为 0 时不闪烁 (虽然理论上不会进入这里)
            }
        }
        // 限制 T0_num 大小防止溢出和长时间不显示
        if (T0_num > 20) T0_num = 0; 
    }
}

/********************************************************
函数名称:void Delay_ms(uint ms)
函数功能:毫秒延时函数
参数说明:ms - 延时毫秒数
********************************************************/ 
// 使用定时器0实现精确延时 - 优化实现
void Delay_ms(uint ms) {
    TMOD = (TMOD & 0xF0) | 0x01; // 设置定时器0为模式1
    TR0 = 0;
    while(ms--) {
        TH0 = 0xFC; // 1ms定时值(FC18H = 64536，延时约1ms)
        TL0 = 0x18;
        TR0 = 1;
        while(!TF0); // 等待溢出
        TR0 = 0;
        TF0 = 0;
    }
}

/********************************************************
函数名称:void PWM_init(void)
函数功能:PWM初始化函数
参数说明:无
********************************************************/ 
// PWM初始化函数 - 配置定时器1生成PWM信号
void PWM_init(void) {
    TMOD = (TMOD & 0x0F) | 0x10; // 设置定时器1为模式1（16位定时器）
    TH1 = 0xFC; // 设置1ms定时值(FC66H = 64614)
    TL1 = 0x66;
    ET1 = 1;    // 使能定时器1中断
    EA = 1;     // 开总中断
    TR1 = 1;    // 启动定时器1
}

/********************************************************
函数名称:void check_alarm(void)
函数功能:检查报警条件,控制指示灯和电机转速
参数说明:无
********************************************************/
void check_alarm(void)
{
    // 声明所有bit类型变量在函数开头
    bit is_pressure_normal, temp_alarm_high;
    
    // 1. 检查压力是否在正常范围内
    is_pressure_normal = (pressure_scaled_x10 >= tire_down) && (pressure_scaled_x10 <= tire_up);

    // 2. 根据压力状态控制指示灯
    if (is_pressure_normal)
    {
        // 压力正常: 绿灯亮, 红灯灭
        tire_led = 0; // P3.7 (Green) ON
        temp_led = 1; // P3.6 (Red) OFF
    }
    else
    {
        // 压力异常: 绿灯灭, 红灯亮
        tire_led = 1; // P3.7 (Green) OFF
        temp_led = 0; // P3.6 (Red) ON
    }

    // 3. 温度报警检测
    temp_alarm_high = (temp_z >= temp_up);
    
    // 4. 设置报警标志
    alarm = !is_pressure_normal || temp_alarm_high; // 包含温度异常
    
    // 5. 根据报警状态控制蜂鸣器
     buzz = 1; // <-- 强制关闭蜂鸣器
   //buzz = alarm ? 0 : 1;  // 报警时蜂鸣器响(0)，正常时关闭(1)

    // 6. 根据压力范围设置电机转速
    if (is_pressure_normal)
    {
        motor_speed = 60; // 正常时固定速度 (或使用之前的 MAX)
    }
    else
    {
        motor_speed = 30; // 异常时低速
    }
}

/********************************************************
函数名称:void main()
函数功能:主函数
参数说明:无
********************************************************/
void main(void)
{
    // 配置中断优先级
    PT0 = 1;       // 定时器0高优先级（用于ADC采样）
    PT1 = 0;       // 定时器1低优先级（用于PWM控制）
    PS = 1;        // 串口中断低优先级

    // 初始化显示模块
    LCD_init();                // 初始化LCD1602显示屏
    Delay_ms(10);              // 等待LCD稳定

    // 写入自定义字符
    lcd1602_write_pic(0,&CGRAM_TABLE[0]);  // 度数符号'°'
    
    // DS18B20传感器初始化 - 确保稳定启动
    DS18B20_Init();            // 初始化DS18B20总线  
    Delay_ms(50);              // 等待稳定时间增加到50ms
    
    // 预热传感器 - 先进行几次测量确保稳定性
    DS18B20_StartConvert();    // 启动初次转换
    Delay_ms(800);             // 等待转换完成
    DS18B20_GetTemp();         // 读取并丢弃初次结果
    
    // 继续原来的初始化
    PS002_init();
    PWM_init();
    
    // 初始化状态
    fixed_display();

    buzz = 1;
    temp_led = 1;
    tire_led = 1;
    motor_speed = 60;

    // 主循环
    while(1)
    {
        // 处理胎压数据
        PS002_Convert(); 
        pressure_scaled_x10 = (adc_value * 37 + 50) / 100;

        // 处理温度数据 - 改进温度更新逻辑
        if(temp_update_flag) {
            DS18B20_Convert();  // 自带重试机制的温度转换函数
            temp_update_flag = 0;
            
            // 如果显示错误温度(88.8)持续超过3次，尝试复位传感器
            if(temp_z == 88 && temp_x == 8) {
                error_count++;
                if(error_count >= 3) {
                    DS18B20_Init();  // 重新初始化传感器
                    Delay_ms(50);    // 给传感器一些恢复时间
                    DS18B20_Convert(); // 再次尝试读取
                    error_count = 0;   // 重置错误计数
                }
            } else {
                error_count = 0;  // 正常温度，重置错误计数
            }
        }

        // 系统状态更新
        check_alarm();
        scan();
        display();

        // 控制刷新频率
        Delay_ms(DISPLAY_REFRESH_RATE);
    } // while(1)循环结束
} // main函数结束
