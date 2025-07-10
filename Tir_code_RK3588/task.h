#ifndef __TASK_H
#define __TASK_H

#include <pthread.h>
#include <stdint.h>

extern volatile int running;
extern pthread_mutex_t print_mutex;

void *motor_a_task(void *arg __attribute__((unused)));
void *motor_b_task(void *arg __attribute__((unused)));
void *motor_c_task(void *arg __attribute__((unused)));
void *motor_d_task(void *arg __attribute__((unused)));
void *print_task(void *arg __attribute__((unused)));

int create_all_tasks(pthread_t *threads, int *thread_ids);
void wait_all_tasks(pthread_t *threads, int thread_count);
void cleanup_tasks(void);

void signal_handler(int sig);
void setup_signal_handlers(void);

int serial_send_data(const unsigned char *data, int len);

int init_pwm(void);
int set_pwm_duty_cycle(int duty_percent);
int set_pwm_frequency(int freq_hz);
int pwm_set_duty(int duty_percent);
int pwm_set_freq(int freq_hz);
void get_pwm_status(void);
void *pwm_task(void *arg);

#endif