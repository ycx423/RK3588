#ifndef __USART_ME_LINUX_H
#define __USART_ME_LINUX_H

#include <stdint.h>

void Parse_Motor_Targets(const char *rx);
void Process_Usart_Lines(char *buf);

#endif