#ifndef __MOTOR_H
#define __MOTOR_H

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
// GPIO设备文件路径
#define GPIO_DEVICE "/dev/GPIO_Device"

// GPIO ioctl命令定义
#define GPIO_IOC_MAGIC 'k'
#define SET_GPIO_ON _IO(GPIO_IOC_MAGIC, 0)
#define SET_GPIO_OFF _IO(GPIO_IOC_MAGIC, 1)

#define Motor_A 0
#define Motor_B 1
#define Motor_C 2
#define Motor_D 3
// GPIO索引定义（对应您的GPIO驱动四组配置）
typedef enum
{
	// A组：B5 B4 A2
	GPIO_IDX_A_EN = 0,	// GPIO3_B5 - 电机A使能
	GPIO_IDX_A_DIR = 1, // GPIO3_B4 - 电机A方向
	GPIO_IDX_A_PUL = 2, // GPIO3_A2 - 电机A脉冲

	// B组：B0 C3 D3
	GPIO_IDX_B_EN = 3,	// GPIO3_B0 - 电机B使能
	GPIO_IDX_B_DIR = 4, // GPIO3_C3 - 电机B方向
	GPIO_IDX_B_PUL = 5, // GPIO3_D3 - 电机B脉冲

	// C组：B6 A6 A4
	GPIO_IDX_C_EN = 6,	// GPIO3_B6 - 电机C使能
	GPIO_IDX_C_DIR = 7, // GPIO3_A6 - 电机C方向
	GPIO_IDX_C_PUL = 8, // GPIO3_A4 - 电机C脉冲

	// D组：A5 B1 A7
	GPIO_IDX_D_EN = 9,	 // GPIO3_A5 - 电机D使能
	GPIO_IDX_D_DIR = 10, // GPIO3_B1 - 电机D方向
	GPIO_IDX_D_PUL = 11, // GPIO3_A7 - 电机D脉冲
} gpio_index_t;

typedef enum
{
	Forward = 0,
	Backward = 1,
	Enable = 1,
	Disable = 0
} Control;

typedef struct
{
	Control EN;
	Control DIR;
	float Current_Circle;
	float Target_Circle;
	gpio_index_t EN_GPIO;
	gpio_index_t DIR_GPIO;
	gpio_index_t PUL_GPIO;
	uint8_t State;				  // 0:未完成 1:完成
	uint8_t Process_Flag;		  // 执行进程标志位
	uint8_t Pro_flag_printf_once; // 新增：每个电机独有的打印标志
	pthread_mutex_t mutex;		  // 线程互斥锁
} motor;

#define PLUSE 200

extern motor motor_data_A;
extern motor motor_data_B;
extern motor motor_data_C;
extern motor motor_data_D; // 新增电机D
extern float Target_Circle_A;
extern float Target_Circle_B;
extern float Target_Circle_C;
extern float Target_Circle_D; // 新增电机D目标
extern int gpio_fd;			  // GPIO设备文件描述符

// 函数声明
int motor_gpio_init(void);
void motor_gpio_close(void);
void Motor_Init(motor *motor_p, gpio_index_t en_gpio, gpio_index_t dir_gpio, gpio_index_t pul_gpio);
void Set_EN_DIR(motor *motor_p);
void Set_MOTOR_Target_Circle_and_DIR(motor *motor_p, uint16_t target_circle, Control EN);
void Motor_Run_Circle(motor *motor_p, uint16_t target_circle, Control EN, const char *name);
int motor_io_init(void);
void Print_Motor_IO_State(const char *name, motor *motor_p);
void Begin_Motor_flag(motor *motor_p);
void STOP_MOTOR(motor *motor_p);
void Set_Motor_Target(int motor_index, float target_value);
void motor_cleanup(void);
void My_Motor_process(void);

int gpio_toggle(gpio_index_t gpio_idx);
int gpio_write(gpio_index_t gpio_idx, int value);

#endif
