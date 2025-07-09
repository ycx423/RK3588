#include <unistd.h> 
#include "motor.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdio.h>	
#include <stdlib.h> 

motor motor_data_A;
motor motor_data_B;
motor motor_data_C;
motor motor_data_D; 
float Target_Circle_A = 0;
float Target_Circle_B = 0;
float Target_Circle_C = 0;
float Target_Circle_D = 0; 
int gpio_fd = -1;		  

void delay_us(int us)
{
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = us * 1000;
	nanosleep(&ts, NULL);
}

void delay_ms(int ms)
{
	usleep(ms * 1000);
}

int motor_gpio_init(void)
{
	gpio_fd = open(GPIO_DEVICE, O_RDWR);
	if (gpio_fd < 0)
	{
		printf("Failed to open GPIO device: %s\n", GPIO_DEVICE);
		return -1;
	}
	printf("GPIO device opened successfully\n");
	return 0;
}

void motor_gpio_close(void)
{
	if (gpio_fd >= 0)
	{
		close(gpio_fd);
		gpio_fd = -1;
		printf("GPIO device closed\n");
	}
}

int gpio_write(gpio_index_t gpio_idx, int value)
{
	if (gpio_fd < 0)
	{
		printf("GPIO device not opened\n");
		return -1;
	}

	if (value)
	{
		return ioctl(gpio_fd, SET_GPIO_ON, gpio_idx);
	}
	else
	{
		return ioctl(gpio_fd, SET_GPIO_OFF, gpio_idx);
	}
}

// GPIO切换操作
int gpio_toggle(gpio_index_t gpio_idx)
{
	static int gpio_states[12] = {0}; 

	if (gpio_idx >= 12)
		return -1;

	gpio_states[gpio_idx] = !gpio_states[gpio_idx];
	return gpio_write(gpio_idx, gpio_states[gpio_idx]);
}

// 电机初始化
void Motor_Init(motor *motor_p, gpio_index_t en_gpio, gpio_index_t dir_gpio, gpio_index_t pul_gpio)
{
	motor_p->EN = Disable;
	motor_p->DIR = Forward;
	motor_p->Current_Circle = 0;
	motor_p->Target_Circle = 0;
	motor_p->EN_GPIO = en_gpio;
	motor_p->DIR_GPIO = dir_gpio;
	motor_p->PUL_GPIO = pul_gpio;
	motor_p->State = 0;
	motor_p->Process_Flag = 0;
	motor_p->Pro_flag_printf_once = 0; 

	// 初始化互斥锁
	pthread_mutex_init(&motor_p->mutex, NULL);

	// 设置初始GPIO状态
	gpio_write(motor_p->EN_GPIO, Disable);
	gpio_write(motor_p->DIR_GPIO, Forward);
	gpio_write(motor_p->PUL_GPIO, 0);
}

// 电机IO初始化
int motor_io_init(void)
{
	// 初始化GPIO设备
	if (motor_gpio_init() < 0)
	{
		printf("Failed to initialize GPIO device\n");
		return -1; 
	}

	// 初始化四个电机
	Motor_Init(&motor_data_A, GPIO_IDX_A_EN, GPIO_IDX_A_DIR, GPIO_IDX_A_PUL);
	Motor_Init(&motor_data_B, GPIO_IDX_B_EN, GPIO_IDX_B_DIR, GPIO_IDX_B_PUL);
	Motor_Init(&motor_data_C, GPIO_IDX_C_EN, GPIO_IDX_C_DIR, GPIO_IDX_C_PUL);
	Motor_Init(&motor_data_D, GPIO_IDX_D_EN, GPIO_IDX_D_DIR, GPIO_IDX_D_PUL); 

	printf("Motor IO initialized successfully (A, B, C, D)\n");
	return 0; // 返回成功
}

// 设置使能和方向
void Set_EN_DIR(motor *motor_p)
{
	gpio_write(motor_p->EN_GPIO, motor_p->EN);
	gpio_write(motor_p->DIR_GPIO, motor_p->DIR);
}

// 设置电机目标和方向
void Set_MOTOR_Target_Circle_and_DIR(motor *motor_p, uint16_t target_circle, Control EN)
{
	pthread_mutex_lock(&motor_p->mutex);

	motor_p->EN = EN;
	motor_p->Target_Circle = target_circle;

	if (motor_p->Current_Circle == motor_p->Target_Circle)
	{
		motor_p->EN = Disable;
		motor_p->State = 1;
	}
	else
	{
		motor_p->DIR = (motor_p->Current_Circle < motor_p->Target_Circle) ? Forward : Backward;
		motor_p->EN = Enable;
	}

	pthread_mutex_unlock(&motor_p->mutex);
}

void Motor_Run_Circle(motor *motor_p, uint16_t target_circle, Control EN, const char *name)
{
	int i;
	float RUN_Line = 0;

	Set_MOTOR_Target_Circle_and_DIR(motor_p, target_circle, EN);
	Set_EN_DIR(motor_p);

	if (motor_p->DIR == Forward)
		RUN_Line = motor_p->Target_Circle - motor_p->Current_Circle;
	else if (motor_p->DIR == Backward)
		RUN_Line = -(motor_p->Target_Circle - motor_p->Current_Circle);

	if (motor_p->Process_Flag == 0 && motor_p->Pro_flag_printf_once == 0)
	{
		printf("Motor %s Process Flag is 0, please set it to 1 before running\r\n", name);
		motor_p->Pro_flag_printf_once = 1;
		return;
	}
	else if (motor_p->Process_Flag == 1 && motor_p->State == 0)
	{
		pthread_mutex_lock(&motor_p->mutex);

		motor_p->State = 0;
		int RUN_PLUSE = (int)(RUN_Line / 0.002f);

		for (i = 0; i < RUN_PLUSE; i++)
		{
			gpio_toggle(motor_p->PUL_GPIO);
			usleep(750); 

			if (motor_p->Process_Flag != 1)
			{
				motor_p->Current_Circle = motor_p->DIR == Forward ? motor_p->Current_Circle + (i * 0.002f) : motor_p->Current_Circle - (i * 0.002f);
				pthread_mutex_unlock(&motor_p->mutex);
				return;
			}
		}

		motor_p->Current_Circle = motor_p->Target_Circle;
		motor_p->State = 1;
		motor_p->Process_Flag = 0;
		motor_p->Pro_flag_printf_once = 0; 

		pthread_mutex_unlock(&motor_p->mutex);
	}
}

// 停止电机
void STOP_MOTOR(motor *motor_p)
{
	pthread_mutex_lock(&motor_p->mutex);
	motor_p->Process_Flag = 0;
	motor_p->EN = Disable;
	gpio_write(motor_p->EN_GPIO, Disable);
	pthread_mutex_unlock(&motor_p->mutex);
}

// 开始电机标志
void Begin_Motor_flag(motor *motor_p)
{
	pthread_mutex_lock(&motor_p->mutex);
	motor_p->Process_Flag = 1;
	motor_p->State = 0;
	pthread_mutex_unlock(&motor_p->mutex);
}

// 打印电机状态
void Print_Motor_IO_State(const char *name, motor *motor_p)
{
	pthread_mutex_lock(&motor_p->mutex);
	printf("%s: EN=%d, DIR=%d, Current=%.1f, Target=%.1f, ProFlag=%d\r\n",
		   name, motor_p->EN, motor_p->DIR,
		   motor_p->Current_Circle, motor_p->Target_Circle, motor_p->Process_Flag);
	pthread_mutex_unlock(&motor_p->mutex);
}

// 设置电机目标
void Set_Motor_Target(int motor_index, float target_value)
{
	switch (motor_index)
	{
	case 0:
		Target_Circle_A = (float)target_value;
		pthread_mutex_lock(&motor_data_A.mutex);
		motor_data_A.Target_Circle = (float)target_value;
		motor_data_A.State = 0;		  
		pthread_mutex_unlock(&motor_data_A.mutex);
		printf("Motor A target set to: %.1f\r\n", target_value);
		break;
	case 1:
		Target_Circle_B = (float)target_value;
		pthread_mutex_lock(&motor_data_B.mutex);
		motor_data_B.Target_Circle = (float)target_value;
		motor_data_B.State = 0;		   
		pthread_mutex_unlock(&motor_data_B.mutex);
		printf("Motor B target set to: %.1f\r\n", target_value);
		break;
	case 2:
		Target_Circle_C = (float)target_value;
		pthread_mutex_lock(&motor_data_C.mutex);
		motor_data_C.Target_Circle = (float)target_value;
		motor_data_C.State = 0;		   
		pthread_mutex_unlock(&motor_data_C.mutex);
		printf("Motor C target set to: %.1f\r\n", target_value);
		break;
	case 3: // 新增电机D
		Target_Circle_D = (float)target_value;
		pthread_mutex_lock(&motor_data_D.mutex);
		motor_data_D.Target_Circle = (float)target_value;
		motor_data_D.State = 0;		   
		pthread_mutex_unlock(&motor_data_D.mutex);
		printf("Motor D target set to: %.1f\r\n", target_value);
		break;
	default:
		printf("Invalid motor index: %d (valid range: 0-3)\r\n", motor_index);
		break;
	}
}
void My_Motor_process(void)
{
	static int process_count = 0;
	if (process_count == 0)
	{
		Set_Motor_Target(Motor_A, 0);
		Set_Motor_Target(Motor_B, 0);
		Set_Motor_Target(Motor_C, 5);
		Set_Motor_Target(Motor_D, 0);
		printf("当前电机状态:\n");
		printf("A: Current=%.1f, Target=%.1f\n", motor_data_A.Current_Circle, motor_data_A.Target_Circle);
		printf("B: Current=%.1f, Target=%.1f\n", motor_data_B.Current_Circle, motor_data_B.Target_Circle);

		process_count++;
		printf("Process started, step 1 finished!\n"); // 第一步
		return;
	}
	if (process_count == 1)
	{
		Set_Motor_Target(Motor_A, 8);
		Set_Motor_Target(Motor_B, 9);
		Set_Motor_Target(Motor_C, 10);
		Set_Motor_Target(Motor_D, 5);
		process_count++;
		printf("Process step 2 finished!\n"); // 第二步
		return;
	}
	if (process_count == 2)
	{
		Set_Motor_Target(Motor_A, 5);
		Set_Motor_Target(Motor_B, 10.5);
		Set_Motor_Target(Motor_C, 8);
		Set_Motor_Target(Motor_D, 10);
		process_count++;
		printf("Process step 3 finished!\n"); // 第三步
		return;
	}
	if (process_count == 3)
	{
		Set_Motor_Target(Motor_A, 7);
		Set_Motor_Target(Motor_B, 10);
		Set_Motor_Target(Motor_C, 10);
		Set_Motor_Target(Motor_D, 15);
		process_count++;
		printf("Process step 4 finished!\n"); // 第四步
		return;
	}
	if (process_count == 4)
	{
		Set_Motor_Target(Motor_A, 5);
		Set_Motor_Target(Motor_B, 10.5);
		Set_Motor_Target(Motor_C, 8);
		Set_Motor_Target(Motor_D, 20);
		process_count++;
		printf("Process step 5 finished!\n"); // 第五步
		return;
	}
	if (process_count == 5)
	{
		Set_Motor_Target(Motor_A, 0);
		Set_Motor_Target(Motor_B, 0);
		Set_Motor_Target(Motor_C, 0);
		Set_Motor_Target(Motor_D, 0);
		process_count++;
		printf("Process step 6 finished!\n"); // 第六步
		return;
	}
}
// 清理资源
void motor_cleanup(void)
{
	// 停止所有电机
	STOP_MOTOR(&motor_data_A);
	STOP_MOTOR(&motor_data_B);
	STOP_MOTOR(&motor_data_C);
	STOP_MOTOR(&motor_data_D);

	// 关闭GPIO设备
	if (gpio_fd >= 0)
	{
		close(gpio_fd);
		gpio_fd = -1;
	}

	printf("Motor system cleaned up\n");
}
