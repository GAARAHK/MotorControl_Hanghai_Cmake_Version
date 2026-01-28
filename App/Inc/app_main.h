#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "main.h"

#define SW_VERSION "1.0.1"

// 应用层初始化
// 在 main.c 的 while(1) 之前调用
void App_Init(void);

// 应用层主循环
// 在 main.c 的 while(1) 内部调用
void App_Loop(void);

#endif
