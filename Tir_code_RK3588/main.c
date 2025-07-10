#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "motor.h"
#include "task.h"
#include <unistd.h>

#define MAX_THREADS 16 

int main()
{
    pthread_t threads[MAX_THREADS];
    int thread_ids[MAX_THREADS];

    for (int i = 0; i < MAX_THREADS; i++)
    {
        thread_ids[i] = i;
    }

    int thread_count = 0;

    printf("=== Motor Control System Starting ===\n");

    setup_signal_handlers();

    // 初始化电机
    printf("Initializing motor system...\n");
    if (motor_io_init() < 0)
    {
        printf("Failed to initialize motor IO\n");
        return -1;
    }

    Set_Motor_Target(Motor_A, 0);
    Set_Motor_Target(Motor_B, 0);
    Set_Motor_Target(Motor_C, 0);
    Set_Motor_Target(Motor_D, 0);

    thread_count = create_all_tasks(threads, thread_ids);
    if (thread_count < 0)
    {
        printf("Failed to create tasks\n");
        motor_cleanup();
        return -1;
    }

    printf("All %d threads created successfully\n", thread_count);
    printf("System running... Press Ctrl+C to exit\n");
    printf("=====================================\n");

    wait_all_tasks(threads, thread_count);

    cleanup_tasks();
    motor_cleanup();

    printf("=== Motor Control System Stopped ===\n");
    return 0;
}