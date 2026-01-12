#include"STC15F2K60S2.H"
#include"intrins.h"

//常量声明
//数码管段码0-F
const unsigned char Segcode[16]={0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90,0x88,0x83,0xc6,0xa1,0x86,0x8e};


//变量声明
volatile unsigned long currenttim=0;//单片机定时器计时时间

//函数声明=====================================================================================
//定时器，延时函数声明
void Timer0Init(void);
void MS_Delay(unsigned int ms);
void US_Delay(unsigned int us);

//I2C驱动函数的声明,MSB
void I2C_Start(void);
void I2C_Stop(void);
void I2C_SendByte(unsigned char Byte);
unsigned char I2C_ReceiveByte(void);
void I2C_SendAck(unsigned char ack);
unsigned char I2C_ReceiveAck(void);

//onewire驱动函数的声明,LSB
unsigned char DS18B20_Init(void);
void DS18B20_SendByte(unsigned char Byte);
unsigned char DS18B20_ReadByte(void);

//DS1302通讯驱动函数,LSB
void DS1302_SendByte(unsigned char addr,unsigned char Byte);
unsigned char DS1302_ReadByte(unsigned char addr);

//数码管驱动函数
void Single_Seg_Display(unsigned char sel,unsigned char Byte,unsigned char dot);//选择，数据，小数点,注意数码只能显示一瞬间，连续显示需要扫描

//矩阵键盘驱动函数
unsigned char Martix_key(void);//单次扫描
unsigned char Get_Key(void);//注这个函数只能判断一次按键是否按下，若不知道按键合适按下会等待按键响应，需要在循环中扫描

//LED驱动函数
void LED_Display(unsigned char Byte);//具有记忆功能，P0不必一直保持

//蜂鸣器继电器控制函数
void Buffer_and_Relay_Control(unsigned char a,unsigned char b);// a继电器 b蜂鸣器

//主函数=======================================================================================
void main(void)
{
	Timer0Init();//必须最先初始化，否则中断函数存在问题
	EA=1;//必须所有有关中断的项目都配置完成才可以打开
	
}

//函数定义=====================================================================================
//单片机定时器配置------------------------------------------------------------------------------
void Timer0Init(void)		//1毫秒@11.0592MHz
{
	AUXR |= 0x80;		//定时器时钟1T模式
	TMOD &= 0xF0;		//设置定时器模式
	TL0 = 0xCD;		//设置定时初值
	TH0 = 0xD4;		//设置定时初值
	TF0 = 0;		//清除TF0标志
	TR0 = 1;		//定时器0开始计时
	ET0=1;//打开定时器0开关
}

void Timer0(void) interrupt 1
{
	currenttim++;
}

void MS_Delay(unsigned int ms)//为阻塞延时
{
	unsigned long prev=currenttim+ms;
	while(prev>=currenttim);
}

void US_Delay(unsigned int us)//为阻塞延时
{
	volatile unsigned int i=0;
	volatile unsigned char j=0;
	for(i=0;i<us;i++)
	{
		for(j=0;j<11;j++)
		{
			_nop_();
		}
	}
}
//I2C------------------------------------------------------------------------------------------
void I2C_Start(void)
{
	//空闲状态
	P21=1;//SDA
	P20=1;//SCK
	US_Delay(3);
	
	P21=0;//拉低SDA，开始通讯
	US_Delay(3);//延时给从机反应时间
	P20=0;//拉低时钟线防止iic误操作
}

void I2C_Stop(void)
{
	P20=0;//先拉低时钟，防止SDA电平不确定误操作
	P21=0;//先保持数据为低
	US_Delay(3);
	P20=1;
	US_Delay(3);//给从机反应时间
	P21=1;
}

void I2C_SendByte(unsigned char Byte)
{
	unsigned char i=0;//循环变量声明
	P20=0;//先拉低SCK，防止误操作
	P21=0;
	US_Delay(2);
	for(i=0;i<8;i++)
	{
		P21=(Byte>>(7-i))&0x01;//准备数据，注意I2C是高位优先
		US_Delay(2);//等待SDA电平稳定
		P20=1;
		US_Delay(3);//给从机反应时间
		P20=0;
	}
	P20=0;//确保I2C_主机不发送接收应答或数据时SCK为低，防止误操作
	P21=1;//释放总线，供从机应答
}

unsigned char I2C_ReceiveByte(void)
{
	unsigned char i=0;//循环变量
	unsigned char temp=0;//函数的返回值
	P20=0;//先拉低SCK，防止I2C误操作
	P21=1;//释放总线，供从机发送数据
	US_Delay(3);//初始化
	for(i=0;i<8;i++)
	{
		temp<<=1;//先左移，接收数据temp只需要移7次，否则最高位会被溢出
		P20=1;//拉高SCK
		US_Delay(3);//等待从机向SDA发送电平稳定
		if(P21)
		{
			temp|=0x01;
		}
		P20=0;//拉低SCK读取结束
		US_Delay(2);
	}
	P20=0;//确保IIC使用中不误触发
	P21=1;//释放总线，等待主机去发送应答
	return(temp);
}

void I2C_SendAck(unsigned char ack)
{
	P20=0;
	P21=0;
	US_Delay(1);//初始状态
	
	P21=!ack;//在SCK为低电平期间准备应答信号，低电平有效
	US_Delay(1);
	P20=1;//拉高SCK
	US_Delay(2);
	P20=0;//拉低SCK
	
	P21=1;//释放SDA,让从机继续发送数据
}

unsigned char I2C_ReceiveAck(void)
{
	unsigned char ack=0;
	P20=0;
	P21=0;
	US_Delay(1);//初始状态
	
	P20=1;//拉高准备读取
	US_Delay(1);//等待SDA电平稳定后读取应答
	ack=P21;//读取应答信号，低电平有效
	ack=!ack;
	P20=0;//拉低表示接收完毕
	
	P21=1;//释放总线，便于主机下一次发送数据
	return(ack);
}
//onewire--------------------------------------------------------------------------------------
unsigned char DS18B20_Init(void)
{
	unsigned char ack=0;
	P14=1;
	US_Delay(10);//延时10us,先保证P14高电平稳定
	P14=0;
	US_Delay(500);//延时500us，让温度传感器稳定读取电平
	P14=1;
	US_Delay(55);//等待温度传感器的拉低电平应答
	ack=P14;
	ack=!ack;  //温度传感器低电平应答有效
	US_Delay(200);//等待温度传感器的低电平应答时间结束，防止与以后的指令冲突
	return(ack);
}

void DS18B20_SendByte(unsigned char Byte)
{
	unsigned char i=0;//循环变量的声明
	P14=1;
	US_Delay(10);//延时10us,先保证P14高电平稳定
	for(i=0;i<8;i++)//低位先行
	{
		if((Byte>>i)&0x01)//1
		{
			P14=0;//拉低提供下降沿时钟
			US_Delay(6);//给传感器检测下降边沿时间
			P14=1;//拉高写1
			US_Delay(55);//传感读取高电平延时时间
		}
		else//0
		{
			P14=0;//拉低提供下降沿时钟
			US_Delay(60);//给传感器检测时间，同时检测低电平
			P14=1;//拉高便于下一次下降延时钟
			US_Delay(5);//高电平保持时间		
		}
	}
	P14=1;//单片机释放总线
}
//DS1302---------------------------------------------------------------------------------------
unsigned char DS18B20_ReadByte(void)
{
	unsigned char i=0;//循环变量的声明
	unsigned char temp=0;//接收变量
	P14=1;
	US_Delay(10);//延时10us,保证P14高电平稳定
	for(i=0;i<8;i++)//低位先行
	{
		temp>>=1;//先左移，接收temp只用左移7次,否则数据为溢出
		P14=0;//拉低P14，提供下降延时钟
		US_Delay(2);//给从机提供反应时间
		P14=1;//主机释放总线，供从机应答
		US_Delay(8);//等待P14电平稳定，但是延时不宜过长，因为温度传感器的电平（低电平）不能一直保持
		if(P14)
		{
			temp|=0x80;
		}
		US_Delay(55);//延时，每一个比特的读时序必须大于60us
	}
	P14=1;//确实P14总线释放
	return(temp);
}

void DS1302_SendByte(unsigned char addr,unsigned char Byte)
{
	unsigned char i=0;//循环变量声明
	P23=0;//数据IO口初始化为低电平
	P17=0;//时钟初始化为低电平
	P13=1;//必须在数据IO口，时钟引脚初始化完成后使能，否则会出现误动作
	US_Delay(10);//延时等待稳定
	
	for(i=0;i<8;i++)//首先发送地址,注意DS1302是LSB
	{
		P23=(addr>>i)&0x01;//数据引脚准备数据
		US_Delay(5);//等待P23引脚电平稳定
		P17=1;//拉高时钟引脚，DS1302写为上升沿有效
		US_Delay(5);//电平保持时间，给从机反应时间
		P17=0;//恢复时钟引脚低电平
		US_Delay(1);//等待引脚电平稳定
	}
	//注意不能让P13引脚失能，否则写入失败
	for(i=0;i<8;i++)
	{
		P23=(Byte>>i)&0x01;//准备数据
		US_Delay(5);//等待数据引脚电平稳定
		P17=1;//P17拉高。上升沿读取P23数据
		US_Delay(5);//电平保持，给从机反应时间
		P17=0;//时钟引脚恢复低电平
		US_Delay(1);//等待时钟引脚电平稳定
	}
	P13=0;//数据发送完成，DS1302不使能，防止误动作
	P23=0;//数据IO口回归低电平，注意操作一定要在P13=0后做，否则误动作
	P17=0;//时钟引脚回归低电平
}

unsigned char DS1302_ReadByte(unsigned char addr)
{
	unsigned char i=0;//循环变量声明
	unsigned char temp=0;//接受变量
	P23=0;//数据IO口初始化为低电平
	P17=0;//时钟初始化为低电平
	P13=1;//必须在数据IO口，时钟引脚初始化完成后使能，否则会出现误动作
	US_Delay(10);//延时等待稳定
	
	for(i=0;i<8;i++)//首先发送地址（虚写），规则同写入
	{
		P23=(addr>>i)&0x01;//数据引脚准备数据
		US_Delay(5);//等待P23引脚电平稳定
		P17=1;//拉高时钟引脚，DS1302写为上升沿有效
		US_Delay(5);//电平保持时间，给从机反应时间
		P17=0;//恢复时钟引脚低电平
		US_Delay(1);//等待引脚电平稳定
	}
	//注意不能让P13引脚失能，否则读取失败
	P23=1;//注意读取要置P23引脚为1，此为单片机的若上拉模式，释放数据总线给DS1302
	US_Delay(1);//延时稳定
	for(i=0;i<8;i++)//读取，下降沿读取，规则LSB
	{
		temp>>=1;//先右移，读取只用移7次，否则最低位会被溢出，数据读取规则LSB
		P17=1;//先把时钟引脚置为高电平
		US_Delay(3);//延时等待电平稳定
		P17=0;//拉低时钟引脚，主机在下降沿读取数据
		US_Delay(1);//短暂延时，等待从机发送数据并且P23电平稳定
		if(P23)
		{
			temp|=0x80;
		}
	}
	P13=0;//数据读取完成，DS1302不使能，防止误动作
	P23=0;//数据口回归低电平，注意这些操作一定在P13=0后做，否则有误动作
	P17=0;//时钟引脚回归低电平
	return(temp);
}
//其它外设和高级函数声明-----------------------------------------------------------------------
void Single_Seg_Display(unsigned char sel,unsigned char Byte,unsigned char dot)
{
	if((sel==0)|(sel>8)|(Byte>15)|(dot>1))//输入非法，直接返回
	{
		return;
	}
	P2=(P2&0x1F)|0x00;//首先选择空（失能），注意P27,P26,P25只能同时赋值，不能分开赋值，否则会导致其它功能误触发
	P0=0x00;//先把P0置0，初始化
	US_Delay(1);//延时，保证电平稳定
	
	P0=0x01<<sel;//独热码,先确定好位选数据
	US_Delay(1);//等待数据稳定
	P2=(P2&0x1F)|0xC0;//11000000,P27=1,P26=1,P25=0，再进行位选，D触发器读取P0准备好的数据
	US_Delay(3);//延时，等待D触发器充分读取数据
	P2=(P2&0x1F)|0x00;//选择空，位选触发器锁存数据
	US_Delay(1);//等待稳定
	
	P0=Segcode[Byte];//段码，先确定好段码，共阳段码1，熄灭，0点亮
	if(dot)//需要显示该位的小数点
	{
		P0|=0xFE;//11111110
	}
	US_Delay(1);//等待稳定
	P2=(P2&0x1F)|0xE0;//11100000,P27=1,P26=1,P25=1,再把数据接到段码区,显示数据
	MS_Delay(1);//延时1毫秒,给定充分点亮时间
	P0=0xFF;//全部熄灭，数码管所有等关闭，防止下一次位选的时候，D触发器保留的数据给下一位造成串扰
	P2=(P2&0x1F)|0x00;//选择空，位选触发器锁存数据（注此时的数据是不让数码管任何部分点亮）
	
	P0=0x00;//准备清除该位的位选，避免对下一位产生干扰
	US_Delay(1);//等待稳定
	P2=(P2&0x1F)|0xC0;//选择位选数据输入，把00000000（0x00）置如到D触发器
	US_Delay(1);//等待稳定
	P2=(P2&0x1F)|0x00;//选择空，位选触发器锁存数据（此时触发器锁存0x00）
}

unsigned char Martix_key(void)//单次扫描
{
	unsigned char key=0;//返回第几个按键被按下
	unsigned int key_value=0;//用于存放采集到的电平情况
	//开始扫描，扫描速度一定要快，否则会导致读取延迟或者按键不触发，扫描低电平，P3口读取是引脚默认高电平（弱上拉）,对低电平敏感
	P44=0;
	P42=1;
	P35=1;
	P34=1;
	key_value=P3&0x0F;//00001111低四位
	P44=1;
	P42=0;
	P35=1;
	P34=1;
	key_value=(key_value<<4)|(P3&0x0F);//0000 0000 0000 0000 16位int每一次收集一列按键的情况
	P44=1;
	P42=1;
	P35=0;
	P34=1;
	key_value=(key_value<<4)|(P3&0x0F);//0000 0000 0000 0000 16位int每一次收集一列按键的情况
	P44=1;
	P42=1;
	P35=1;
	P34=0;
	key_value=(key_value<<4)|(P3&0x0F);//0000 0000 0000 0000 16位int每一次收集一列按键的情况
	//按键按下读到的是低电平
	key_value=~key_value;//按位取反，以独热码形式展现
	switch(key_value)
	{
		case 0x8000://S1
		{
			key=1;//1000 0000 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x4000://S2
		{
			key=2;//0100 0000 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x2000://S3
		{
			key=3;//0010 0000 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x1000://S4
		{
			key=4;//0001 0000 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0800://S5
		{
			key=5;//0000 1000 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0400://S6
		{
			key=6;//0000 0100 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0200://S7
		{
			key=7;//0000 0010 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0100://S8
		{
			key=8;//0000 0001 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0080://S9
		{
			key=9;//0000 0000 1000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0040://S10
		{
			key=10;//0000 0000 0100 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0020://S11
		{
			key=11;//0000 0000 0010 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0010://S12
		{
			key=12;//0000 0000 0001 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0008://S13
		{
			key=13;//0000 0000 0000 1000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0004://S14
		{
			key=14;//0000 0000 0000 0100
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0002://S15
		{
			key=15;//0000 0000 0000 0010
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		case 0x0001://S16
		{
			key=16;//0000 0000 0000 0001
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
		default://未按下，返回0
		{
			key=0;//0000 0000 0000 0000
			break;//S1 S2 S3 S4 S5 S6 S7 S8 S9 S10 S11 S12 S13 S14 S15 S16
		}
	}
	return(key);
}

unsigned char Get_Key(void)//具有消抖，按下延时防止多次触发
{
	unsigned char V1=0;//用于保存函数返回的值
	unsigned char V2=0;//用于保存函数返回的值
	unsigned char V3=0;//用于保存函数返回的值
	unsigned char V4=0;//用于保存函数返回的值
	unsigned char V5=0;//用于保存函数返回的值
	//连续三次读取，只有一个状态出现并至少保持14ms以上才可能被读取
	V1=Martix_key();//第一次读取
	MS_Delay(7);//延时
	V2=Martix_key();//第二次读取
	MS_Delay(7);//延时
	V3=Martix_key();//第三次读取
	//三次结果相同才会被触发
	if((V1==V2)&(V2==V3)&(V1==V3))
	{
		if(V1==0)//稳定读取到没有被按下的状态
		{
			return(0);//返回0
		}
		else//被按下
		{
			while(1)//阻塞循环，在循环中一直扫描按键的值，只用按键被稳定放下才退出并返回值
			{
				V4=Martix_key();//第一次读取
				MS_Delay(5);//延时
				V5=Martix_key();//第二次读取
				if((V4==V5)&(V4==0))//按键被稳定放下，才退出循环
				{
					break;//退出循环
				}
			}
			return(V1);//在按键稳定放下（循环退出后）返回读到的按键值
		}
	}
	return(0);//如果读到的是杂波以及电平不稳定的情况，一律返回0，表示按键未按下
}

void LED_Display(unsigned char Byte)
{
	P2=(P2&0x0F)|0x00;//先失能
	P0=0xFF;//先关闭所有LED灯，防止P0的处置造成LED灯误点亮
	US_Delay(1);//等待稳定
	P0=~Byte;
	US_Delay(1);//等待稳定
	P2=(P2&0x0F)|0x80;//启动LED锁存器的数据使能
	US_Delay(3);//等待LED的D触发器稳定获取显示数据
	P2=(P2&0x0F)|0x00;//后失能
	P0=0x00;//最后P0 回归0x00
}

void Buffer_and_Relay_Control(unsigned char a,unsigned char b)//继电器 蜂鸣器控制函数
{
	unsigned char temp=0;//控制变量声明
	if((a>1)|(b>1))//输入非法直接返回
	{
		return;
	}
	P2=(P2&0x0F)|0x00;//先失能
	P0=0x00;//先不使能任何外设，防止误操作
	US_Delay(1);//等待稳定
	if(a==1)//继电器开关
	{
		temp|=0x10;
	}
	//注：必须使用或，如果单独赋值，那么同时对蜂鸣器和继电器的操作会冲突
	if(b==1)//蜂鸣器开关
	{
		temp|=0x40;
	}
	P0=temp;//准备好数据
	US_Delay(1);//等待稳定
	P2=(P2&0x0F)|0xA0;//选择功率外设控制D触发器，使能读取
	US_Delay(3);//让D触发器读到的电平稳定
	P2=(P2&0x0F)|0x00;//后失能
	P0=0x00;//重新设置P0为0x00;
}