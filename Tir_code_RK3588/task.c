#include <unistd.h>
#include "task.h"
#include "motor.h"
#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sched.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

// ============================================================================
// 全局变量定义
// ============================================================================
volatile int running = 1;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int Printf_Flag = 1;
volatile int Process_continue_flag = 0;

// 串口相关全局变量
static int serial_fd = -1;
static pthread_mutex_t serial_mutex = PTHREAD_MUTEX_INITIALIZER;

// PWM 相关全局变量
static pthread_mutex_t pwm_mutex = PTHREAD_MUTEX_INITIALIZER;
static int current_period_ns = 1000000; // 当前周期（ns），默认1kHz
static int current_duty_percent = 0;    // 当前占空比百分比

// ============================================================================
// 信号处理函数
// ============================================================================
void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        printf("\nReceived signal %d, shutting down...\n", sig);
        running = 0;
    }
}

void setup_signal_handlers(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

// ============================================================================
// 串口相关函数
// ============================================================================
int init_serial_port(const char *device, unsigned long baudrate)
{
    serial_fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd == -1)
    {
        perror("Failed to open serial port");
        return -1;
    }

    if (func_set_opt(serial_fd, baudrate, 8, 1, 'N', 0) < 0)
    {
        close(serial_fd);
        serial_fd = -1;
        return -1;
    }

    printf("Serial port %s initialized successfully at %lu baud\n", device, baudrate);
    return 0;
}

int serial_send_data(const unsigned char *data, int len)
{
    if (serial_fd == -1)
    {
        printf("Serial port not initialized\n");
        return -1;
    }

    pthread_mutex_lock(&serial_mutex);
    int result = func_send_frame(serial_fd, data, len);
    pthread_mutex_unlock(&serial_mutex);

    return result;
}

// ============================================================================
// PWM 相关函数
// ============================================================================
// 初始化函数
int init_pwm(void)
{

    if (system("echo 0 > /sys/class/pwm/pwmchip0/export") != 0)
    {
        printf("Warning: PWM export may have failed (possibly already exported)\n");
    }
    usleep(100000);
    // 设置 PWM 周期为1000000 ns (1kHz)
    current_period_ns = 1000000;
    if (system("echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/period") != 0)
    {
        printf("Failed to set PWM period\n");
        return -1;
    }

    // 设置 PWM 占空比为0% (0 ns)
    current_duty_percent = 0;
    if (system("echo 0 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle") != 0)
    {
        printf("Failed to set PWM duty cycle\n");
        return -1;
    }

    if (system("echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable") != 0)
    {
        printf("Failed to enable PWM\n");
        return -1;
    }

    printf("PWM2 initialized successfully: 1kHz, 0%% duty cycle\n");
    return 0;
}

int set_pwm_duty_cycle(int duty_percent)
{
    char command[256];

    if (duty_percent < 0)
        duty_percent = 0;
    if (duty_percent > 100)
        duty_percent = 100;

    int duty_ns = (current_period_ns * duty_percent) / 100;

    if (duty_ns > current_period_ns)
    {
        duty_ns = current_period_ns;
        duty_percent = 100;
        printf("Warning: Duty cycle clamped to 100%%\n");
    }

    snprintf(command, sizeof(command), "echo %d > /sys/class/pwm/pwmchip0/pwm0/duty_cycle", duty_ns);
    if (system(command) == 0)
    {
        current_duty_percent = duty_percent;
        printf("PWM duty cycle set to: %d%% (%d ns)\n", duty_percent, duty_ns);
    }
    else
    {
        printf("Failed to set PWM duty cycle\n");
        return -1;
    }

    return 0;
}

int set_pwm_frequency(int freq_hz)
{
    char command[256];

    if (freq_hz < 1)
        freq_hz = 1;
    if (freq_hz > 100000)
        freq_hz = 100000;

    int new_period_ns = 1000000000 / freq_hz;

    system("echo 0 > /sys/class/pwm/pwmchip0/pwm0/enable");

    // 设置新周期
    snprintf(command, sizeof(command), "echo %d > /sys/class/pwm/pwmchip0/pwm0/period", new_period_ns);
    if (system(command) != 0)
    {
        printf("Failed to set PWM period\n");
        // 恢复原状态
        system("echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable");
        return -1;
    }
    // 更新周期
    current_period_ns = new_period_ns;

    // 重新计算并设置占空比（保持当前百分比）
    int duty_ns = (current_period_ns * current_duty_percent) / 100;

    if (duty_ns > current_period_ns)
    {
        duty_ns = current_period_ns;
        current_duty_percent = 100;
        printf("Warning: Duty cycle adjusted to 100%% for new frequency\n");
    }

    snprintf(command, sizeof(command), "echo %d > /sys/class/pwm/pwmchip0/pwm0/duty_cycle", duty_ns);
    if (system(command) != 0)
    {
        printf("Failed to set PWM duty cycle after frequency change\n");
        return -1;
    }
    system("echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable");

    printf("PWM frequency set to: %d Hz (period: %d ns, duty: %d%% = %d ns)\n",
           freq_hz, new_period_ns, current_duty_percent, duty_ns);
    return 0;
}

// PWM 控制函数
int pwm_set_duty(int duty_percent)
{
    pthread_mutex_lock(&pwm_mutex);
    int result = set_pwm_duty_cycle(duty_percent);
    pthread_mutex_unlock(&pwm_mutex);
    return result;
}

int pwm_set_freq(int freq_hz)
{
    pthread_mutex_lock(&pwm_mutex);
    int result = set_pwm_frequency(freq_hz);
    pthread_mutex_unlock(&pwm_mutex);
    return result;
}

// 获取当前 PWM 状态信息
void get_pwm_status(void)
{
    int duty_ns = (current_period_ns * current_duty_percent) / 100;
    int freq_hz = 1000000000 / current_period_ns;

    printf("PWM Status: Freq=%d Hz, Period=%d ns, Duty=%d%% (%d ns)\n",
           freq_hz, current_period_ns, current_duty_percent, duty_ns);
}

// ============================================================================
// 任务函数
// ============================================================================
void *motor_a_task(void *arg __attribute__((unused)))
{
    printf("Motor A task started\n");

    while (running)
    {
        Motor_Run_Circle(&motor_data_A, Target_Circle_A, Enable, "A");
        usleep(1000);
    }

    printf("Motor A task stopped\n");
    return NULL;
}

void *motor_b_task(void *arg __attribute__((unused)))
{
    printf("Motor B task started\n");

    while (running)
    {
        Motor_Run_Circle(&motor_data_B, Target_Circle_B, Enable, "B");
        usleep(1000);
    }

    printf("Motor B task stopped\n");
    return NULL;
}

void *motor_c_task(void *arg __attribute__((unused)))
{
    printf("Motor C task started\n");

    while (running)
    {
        Motor_Run_Circle(&motor_data_C, Target_Circle_C, Enable, "C");
        usleep(1000);
    }

    printf("Motor C task stopped\n");
    return NULL;
}

void *motor_d_task(void *arg __attribute__((unused)))
{
    printf("Motor D task started\n");

    while (running)
    {
        Motor_Run_Circle(&motor_data_D, Target_Circle_D, Enable, "D");
        usleep(1000);
    }

    printf("Motor D task stopped\n");
    return NULL;
}

void *print_task(void *arg __attribute__((unused)))
{
    printf("Print task started\n");

    while (running)
    {
        if (Printf_Flag == 0)
        {
            usleep(500000);
            continue;
        }
        pthread_mutex_lock(&print_mutex);

        printf("------------------------\n");
        Print_Motor_IO_State("A", &motor_data_A);
        Print_Motor_IO_State("B", &motor_data_B);
        Print_Motor_IO_State("C", &motor_data_C);
        Print_Motor_IO_State("D", &motor_data_D);
        printf("------------------------\n");
        Printf_Flag = 0;
        pthread_mutex_unlock(&print_mutex);

        usleep(100000);
    }

    printf("Print task stopped\n");
    return NULL;
}

void *serial_task(void *arg __attribute__((unused)))
{
    printf("Serial task started\n");

    if (init_serial_port("/dev/ttyS9", 115200) < 0)
    {
        printf("Failed to initialize serial port /dev/ttyS9, serial task will exit\n");
        return NULL;
    }

    unsigned char recv_buffer[256];
    int recv_len = 0;

    while (running)
    {
        pthread_mutex_lock(&serial_mutex);
        recv_len = func_receive_frame(serial_fd, recv_buffer, sizeof(recv_buffer) - 1);

        if (recv_len > 0)
        {
            if (recv_len == 4)
            {
                float value;
                memcpy(&value, recv_buffer, 4);
                printf("PH is: %.1f\n", value);
                if (value < 7.0)
                    printf("→ 当前为酸性环境。\n");
                else if (value > 7.0)
                    printf("→ 当前为碱性环境。\n");
                else
                    printf("→ 当前为中性环境。\n");

                printf("常见物质PH值参考:\n");
                printf("  - 柠檬汁: 2.0\n");
                printf("  - 可乐: 2.5\n");
                printf("  - 雨水: 5.5\n");
                printf("  - 纯净水: 7.0\n");
                printf("  - 海水: 8.0\n");
                printf("  - 肥皂水: 10.0\n");
                printf("  - 漂白水: 12.5\n");
            }
            const char *response = "OK\r\n";
            func_send_frame(serial_fd, (const unsigned char *)response, strlen(response));
        }
        else if (recv_len < 0)
        {
            printf("Serial receive error\n");
        }

        pthread_mutex_unlock(&serial_mutex);
        usleep(10000);
    }

    if (serial_fd != -1)
    {
        close(serial_fd);
        serial_fd = -1;
    }

    printf("Serial task stopped\n");
    return NULL;
}

void *pwm_task(void *arg __attribute__((unused)))
{
    printf("PWM task started\n");

    if (init_pwm() < 0)
    {
        printf("Failed to initialize PWM, PWM task will exit\n");
        return NULL;
    }

    int duty_cycle = 0;
    int direction = 1;

    while (running)
    {
        pthread_mutex_lock(&pwm_mutex);
        if (direction == 1)
        {
            duty_cycle = 0;
            set_pwm_duty_cycle(duty_cycle);
            direction = 0;
        }
        pthread_mutex_unlock(&pwm_mutex);
        usleep(100000);
    }

    system("echo 0 > /sys/class/pwm/pwmchip0/pwm0/enable");
    system("echo 0 > /sys/class/pwm/pwmchip0/unexport");

    printf("PWM task stopped\n");
    return NULL;
}

void *console_task(void *arg __attribute__((unused)))
{
    char input[128];
    printf("控制台任务已启动，输入如 A:10 或 $A:10 修改目标圈数和使能\n");
    printf("PWM控制: P:50 设置占空比50%%, F:1000 设置频率1000Hz, PWM 查看状态\n");
    printf(">> ");
    fflush(stdout);

    while (running)
    {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int select_result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

        if (select_result < 0)
        {
            if (!running)
                break;
            continue;
        }
        else if (select_result == 0)
        {
            if (!running)
                break;
            continue;
        }

        if (!fgets(input, sizeof(input), stdin))
        {
            if (!running)
                break;
            continue;
        }

        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "@") == 0)
        {
            Printf_Flag = 1;
            printf(">> ");
            fflush(stdout);
            continue;
        }

        // PWM 控制命令
        if (strncmp(input, "P:", 2) == 0)
        {
            int duty = atoi(input + 2);
            pwm_set_duty(duty);
            printf(">> ");
            fflush(stdout);
            continue;
        }

        if (strncmp(input, "F:", 2) == 0)
        {
            int freq = atoi(input + 2);
            pwm_set_freq(freq);
            printf(">> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(input, "PWM") == 0)
        {
            get_pwm_status();
            printf(">> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(input, "$") == 0)
        {
            pthread_mutex_lock(&motor_data_A.mutex);
            motor_data_A.Process_Flag = 1;
            motor_data_A.State = 0;
            pthread_mutex_unlock(&motor_data_A.mutex);

            pthread_mutex_lock(&motor_data_B.mutex);
            motor_data_B.Process_Flag = 1;
            motor_data_B.State = 0;
            pthread_mutex_unlock(&motor_data_B.mutex);

            pthread_mutex_lock(&motor_data_C.mutex);
            motor_data_C.Process_Flag = 1;
            motor_data_C.State = 0;
            pthread_mutex_unlock(&motor_data_C.mutex);

            pthread_mutex_lock(&motor_data_D.mutex);
            motor_data_D.Process_Flag = 1;
            motor_data_D.State = 0;
            pthread_mutex_unlock(&motor_data_D.mutex);

            Printf_Flag = 1;
            Process_continue_flag = 1;
            printf("所有电机EN已置为1\n");
            printf(">> ");
            fflush(stdout);
            continue;
        }

        char *token = strtok(input, ",");
        while (token != NULL)
        {
            while (*token == ' ')
                token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ')
                *end-- = '\0';

            int enable = 0;
            char motor = 0;
            float value = 0;

            if (token[0] == '$')
            {
                enable = 1;
                motor = token[1];
                if (sscanf(token + 2, ":%f", &value) != 1)
                {
                    printf("格式错误，应为 $A:10\n");
                    token = strtok(NULL, ",");
                    continue;
                }
            }
            else
            {
                motor = token[0];
                if (sscanf(token + 1, ":%f", &value) != 1)
                {
                    printf("格式错误，应为 A:10\n");
                    token = strtok(NULL, ",");
                    continue;
                }
            }

            switch (motor)
            {
            case 'A':
                if (value > 10.5f)
                {
                    value = 10.5f;
                    printf("A电机最大值为10.5\n");
                }
                Set_Motor_Target(Motor_A, value);
                pthread_mutex_lock(&motor_data_A.mutex);
                if (enable)
                {
                    motor_data_A.Process_Flag = 1;
                    motor_data_A.State = 0;
                }
                pthread_mutex_unlock(&motor_data_A.mutex);
                break;
            case 'B':
                if (value > 9.0f)
                {
                    value = 9.0f;
                    printf("B电机最大值为9.0\n");
                }
                Set_Motor_Target(Motor_B, value);
                pthread_mutex_lock(&motor_data_B.mutex);
                if (enable)
                {
                    motor_data_B.Process_Flag = 1;
                    motor_data_B.State = 0;
                }
                pthread_mutex_unlock(&motor_data_B.mutex);
                break;
            case 'C':
                if (value > 18.0f)
                {
                    value = 18.0f;
                    printf("C电机最大值为18.0\n");
                }
                Set_Motor_Target(Motor_C, value);
                pthread_mutex_lock(&motor_data_C.mutex);
                if (enable)
                {
                    motor_data_C.Process_Flag = 1;
                    motor_data_C.State = 0;
                }
                pthread_mutex_unlock(&motor_data_C.mutex);
                break;
            case 'D':
                Set_Motor_Target(Motor_D, value);
                pthread_mutex_lock(&motor_data_D.mutex);
                if (enable)
                {
                    motor_data_D.Process_Flag = 1;
                    motor_data_D.State = 0;
                }
                pthread_mutex_unlock(&motor_data_D.mutex);
                break;
            default:
                printf("未知电机: %c\n", motor);
                token = strtok(NULL, ",");
                continue;
            }
            printf("已设置 %c: %.2f\n", motor, value);
            token = strtok(NULL, ",");
        }
        Printf_Flag = 1;
        printf(">> ");
        fflush(stdout);
    }
    printf("控制台任务退出\n");
    return NULL;
}
int Finish_flag = 1; // 用于标记处理任务是否完成
void *process_task(void *arg __attribute__((unused)))
{
    printf("Process task started\n");

    while (running)
    {
        if (Process_continue_flag == 1 && Finish_flag == 1)
        {
            Finish_flag = 0;
            motor_data_A.EN = Enable;
            motor_data_B.EN = Enable;
            motor_data_C.EN = Enable;
            motor_data_A.Process_Flag = 1;
            motor_data_B.Process_Flag = 1;
            motor_data_C.Process_Flag = 1;
            My_Motor_process();
            // Process_continue_flag = 0;
        }
        if (motor_data_A.State == 1 && motor_data_B.State == 1 &&
            motor_data_C.State == 1)
        {
            Finish_flag = 1; // 所有电机完成后，设置标志位
            usleep(500000);
        }
        usleep(1000000);
    }

    printf("Process task stopped\n");
    return NULL;
}

// ============================================================================
// 线程管理函数
// ============================================================================
int get_task_priority(int task_index)
{
    switch (task_index)
    {
    case 0:
        return 80; // Motor A
    case 1:
        return 80; // Motor B
    case 2:
        return 80; // Motor C
    case 3:
        return 80; // Motor D
    case 4:
        return 10; // Print task
    case 5:
        return 50; // Console task
    case 6:
        return 10; // Process task
    case 7:
        return 60; // Serial task
    case 8:
        return 40; // PWM task
    default:
        return 30;
    }
}

void set_thread_priority(pthread_t thread, int priority, const char *name)
{
    struct sched_param param;
    param.sched_priority = priority;

    int result = pthread_setschedparam(thread, SCHED_FIFO, &param);
    if (result != 0)
    {
        param.sched_priority = 0;
        result = pthread_setschedparam(thread, SCHED_OTHER, &param);
        if (result == 0)
        {
            printf("%s 设置为普通优先级 (root权限设置实时优先级)\n", name);
        }
    }
    else
    {
        printf("%s 设置为实时优先级: %d\n", name, priority);
    }
}

int create_all_tasks(pthread_t *threads, int *thread_ids)
{
    void *(*task_functions[])(void *) = {
        motor_a_task, motor_b_task, motor_c_task, motor_d_task,
        print_task, console_task, process_task, serial_task, pwm_task};

    const char *task_names[] = {
        "motor A", "motor B", "motor C", "motor D",
        "print", "console", "process", "serial", "pwm"};

    int task_count = sizeof(task_functions) / sizeof(task_functions[0]);
    printf("Creating %d threads...\n", task_count);

    for (int i = 0; i < task_count; i++)
    {
        if (pthread_create(&threads[i], NULL, task_functions[i], &thread_ids[i]) != 0)
        {
            printf("Failed to create %s thread\n", task_names[i]);
            running = 0;
            for (int j = 0; j < i; j++)
            {
                pthread_join(threads[j], NULL);
            }
            return -1;
        }
        int priority = get_task_priority(i);
        set_thread_priority(threads[i], priority, task_names[i]);
        printf("%s thread created successfully\n", task_names[i]);
    }

    return task_count;
}

void wait_all_tasks(pthread_t *threads, int thread_count)
{
    printf("Waiting for all threads to finish...\n");
    for (int i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }
    printf("All threads finished\n");
}

void cleanup_tasks(void)
{
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&serial_mutex);
    pthread_mutex_destroy(&pwm_mutex);

    if (serial_fd != -1)
    {
        close(serial_fd);
        serial_fd = -1;
    }

    system("echo 0 > /sys/class/pwm/pwmchip0/pwm0/enable");
    system("echo 0 > /sys/class/pwm/pwmchip0/unexport");

    printf("Task resources cleaned up\n");
}