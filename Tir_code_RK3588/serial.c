#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h> /*PPSIX 终端控制定义*/

/*
 * @description     : 串口参数设置
 * @param - speed   : 波特率
 * @param - bits    : 数据位
 * @param - stop    : 停止位
 * @param - check   : 校验方式
 * @param - hardware: 硬件流控
 * @return		    : 执行结果
 */
int func_set_opt(int fd, unsigned long speed, unsigned char bits, unsigned char stop, unsigned char check, unsigned char hardware)
{
    struct termios newtio, oldtio;
	/*保存原先串口设置*/
    if (tcgetattr(fd, &oldtio) != 0)	//通过此函数可以我们可以把termios保存在结构变量oldtio；函数原型：int tcgetattr(int fd,struct termois & termios_p);
    {
        perror("tcgetattr");
        return -1;
    }
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag |= CLOCAL | CREAD; /*用于本地连接和接收使能*/
    newtio.c_cflag &= ~CSIZE;	/*用数据掩码清空数据位设置*/

    switch (bits)
    {
    case 5:
        newtio.c_cflag |= CS5;
        break;
    case 6:
        newtio.c_cflag |= CS6;
        break;
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    default:
        newtio.c_cflag |= CS8;
        break;
    }

    switch (check)
    {
    case 'O': //奇校验
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= INPCK;
        break;
    case 'E': //偶校验
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        newtio.c_iflag |= INPCK;
        break;
    case 'M': //MARK校验
        newtio.c_cflag |= PARODD;
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= CMSPAR;
        newtio.c_iflag &= ~INPCK;
        break;
    case 'S': //SPACE校验
        newtio.c_cflag &= ~PARODD;
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= CMSPAR;
        newtio.c_iflag &= ~INPCK;
        break;
    case 'N': //无校验
        newtio.c_cflag &= ~PARENB;
        break;
    default:
        newtio.c_cflag &= ~PARENB;
        break;
    }

    switch (speed)	//设置波特率
    {
    case 600:
        cfsetispeed(&newtio, B600);
        cfsetospeed(&newtio, B600);
        break;
    case 1200:
        cfsetispeed(&newtio, B1200);
        cfsetospeed(&newtio, B1200);
        break;
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 19200:
        cfsetispeed(&newtio, B19200);
        cfsetospeed(&newtio, B19200);
        break;
    case 38400:
        cfsetispeed(&newtio, B38400);
        cfsetospeed(&newtio, B38400);
        break;
    case 57600:
        cfsetispeed(&newtio, B57600);
        cfsetospeed(&newtio, B57600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    case 230400:
        cfsetispeed(&newtio, B230400);
        cfsetospeed(&newtio, B230400);
        break;
    case 460800:
        cfsetispeed(&newtio, B460800);
        cfsetospeed(&newtio, B460800);
        break;
    case 500000:
        cfsetispeed(&newtio, B500000);
        cfsetospeed(&newtio, B500000);
        break;
    case 576000:
        cfsetispeed(&newtio, B576000);
        cfsetospeed(&newtio, B576000);
        break;
    case 921600:
        cfsetispeed(&newtio, B921600);
        cfsetospeed(&newtio, B921600);
        break;
    case 1000000:
        cfsetispeed(&newtio, B1000000);
        cfsetospeed(&newtio, B1000000);
        break;
    case 1152000:
        cfsetispeed(&newtio, B1152000);
        cfsetospeed(&newtio, B1152000);
        break;
    default:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    }
    if (stop == 1)
    {
        newtio.c_cflag &= ~CSTOPB;	//将停止位设置成一个bit
    }
    else if (stop == 2)
    {
        newtio.c_cflag |= CSTOPB;	//将停止位设置成两个bit
    }
    if (hardware == 1)
    {
        newtio.c_cflag |= CRTSCTS;
    }
    newtio.c_cc[VTIME] = 0;	//设置等待时间
    newtio.c_cc[VMIN] = 0;	//设置最少字符
    tcflush(fd, TCIFLUSH);	
    if ((tcsetattr(fd, TCSANOW, &newtio)) != 0)
    {
        perror("com set error");
        return -1;
    }
    printf("set done!\n");
    return 0;
}

/*
 * @description  : 串口发送函数
 * @param - fd   : 文件描述符
 * @param - *p_send_buff: 要发送数据缓冲区首地址
 * @param - count: 要发送数据长度
 * @return		 : 执行结果
 */
int func_send_frame(int fd, const unsigned char *p_send_buff, const int count)
{
    int Result = 0;

    Result = write(fd, p_send_buff, count);
    if (Result == -1)
    {
        perror("write");
        return 0;
    }
    return Result;
}

/*
 * @description  : 串口接收函数
 * @param - fd   : 文件描述符
 * @param - *p_receive_buff: 接收数据缓冲区首地址
 * @param - count: 最大接收数据长度
 * @return		 : 执行结果
 */
int func_receive_frame(int fd, unsigned char *p_receive_buff, const int count)
{
    // 阻塞用法
    int nread = 0; 	//存储读取字节数的变量，初始化为0
    fd_set rd;		//监视串口文件描述符是否有数据可读
    int retval = 0;	//用于存储retval函数的返回值
    struct timeval timeout = {0, 500};	//设置select函数的超时时间

    FD_ZERO(&rd);		//用于将rd文件描述符集合清零，方便后续设置监视对象
    FD_SET(fd, &rd);	//将串口文件描述符fd添加到文件描述符集合rd中，以便select监视这个文件描述符是否有可读数据
    memset(p_receive_buff, 0x0, count);		//初始化接收缓冲区
    retval = select(fd + 1, &rd, NULL, NULL, &timeout);		
    switch (retval)
    {
    case 0:
        nread = 0;		//无数据可读
        break;
    case -1:						
        printf("select%s\n", strerror(errno));		//发生错误
        nread = -1;
        break;
    default:
        nread = read(fd, p_receive_buff, count); //读串口		
        break;
    }

    return nread;
}
