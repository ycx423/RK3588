#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "motor.h"
#include "usart_me_Recive.h"

extern float Target_Circle_A;
extern float Target_Circle_B;
extern float Target_Circle_C;
extern float Target_Circle_D;
extern motor motor_data_A, motor_data_B, motor_data_C, motor_data_D;

// 解析函数
void Parse_Motor_Targets(const char *rx)
{
    int i = 0;
    float val = 0;
    char id = 0;
    int len = strlen(rx);

    // 只输入了"$"，全部置1并返回
    if (len == 1 && rx[0] == '$') {
        motor_data_A.Process_Flag = 1; motor_data_A.State = 0;
        motor_data_B.Process_Flag = 1; motor_data_B.State = 0;
        motor_data_C.Process_Flag = 1; motor_data_C.State = 0;
        motor_data_D.Process_Flag = 1; motor_data_D.State = 0;
        printf("All Process_Flag set to 1 by $ command\n");
        return;
    }
    while (i < len)
    {
        int has_dollar = 0;
        if (rx[i] == '$') {
            has_dollar = 1;
            i++;
        }

        if ((rx[i] == 'A' || rx[i] == 'B' || rx[i] == 'C' || rx[i] == 'D') && rx[i + 1] == ':')
        {
            id = rx[i];
            i += 2;
            val = 0;
            float frac = 0.1f;
            int point = 0;
            while ((rx[i] >= '0' && rx[i] <= '9') || rx[i] == '.')
            {
                if (rx[i] == '.')
                    point = 1;
                else if (!point)
                    val = val * 10 + (rx[i] - '0');
                else
                {
                    val = val + (rx[i] - '0') * frac;
                    frac *= 0.1f;
                }
                i++;
            }
            if (id == 'A') {
                Target_Circle_A = val;
                if (has_dollar) { motor_data_A.Process_Flag = 1; motor_data_A.State = 0; }
            } else if (id == 'B') {
                Target_Circle_B = val;
                if (has_dollar) { motor_data_B.Process_Flag = 1; motor_data_B.State = 0; }
            } else if (id == 'C') {
                Target_Circle_C = val;
                if (has_dollar) { motor_data_C.Process_Flag = 1; motor_data_C.State = 0; }
            } else if (id == 'D') {
                Target_Circle_D = val;
                if (has_dollar) { motor_data_D.Process_Flag = 1; motor_data_D.State = 0; }
            }
        }
        else
        {
            i++;
        }
    }
}

void Process_Usart_Lines(char *buf)
{
    char *line = strtok(buf, "\r\n");
    while (line != NULL)
    {
        if (strlen(line) > 0)
        {
            printf("Received: %s\n", line);
            Parse_Motor_Targets(line);
            printf("  Target_Circle_A = %.2f, Target_Circle_B = %.2f, Target_Circle_C = %.2f, Target_Circle_D = %.2f\n",
                   Target_Circle_A, Target_Circle_B, Target_Circle_C, Target_Circle_D);
            printf(" Current_Circle_A = %.2f, Current_Circle_B = %.2f, Current_Circle_C = %.2f, Current_Circle_D = %.2f\n",
                   motor_data_A.Current_Circle, motor_data_B.Current_Circle, motor_data_C.Current_Circle, motor_data_D.Current_Circle);
            printf("    Process_Flag_A = %d, Process_Flag_B = %d, Process_Flag_C = %d, Process_Flag_D = %d\n",
                   motor_data_A.Process_Flag, motor_data_B.Process_Flag, motor_data_C.Process_Flag, motor_data_D.Process_Flag);
        }
        line = strtok(NULL, "\r\n");
    }
}