
/*串口参数结构体 */
typedef struct
{
    unsigned long baudrate; /*波特率 600~4000000*/
    unsigned char data_bit; /*数据位5/6/7/8*/
    unsigned char stop_bit; /*停止位 1/2*/
    unsigned char check;    /*校验方式 'N':无校验，'O'：奇校验，'E'：偶校验*/
    unsigned char hardware;/*硬件流控*/
} struct_tty_param;

int func_set_opt(int fd, unsigned long speed, unsigned char bits, unsigned char stop, unsigned char check, unsigned char hardware);
int func_send_frame(int fd, const unsigned char *p_send_buff, const int count);
int func_receive_frame(int fd, unsigned char *p_receive_buff, const int count);
