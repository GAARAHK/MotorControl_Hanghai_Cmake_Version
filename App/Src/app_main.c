#include "app_main.h"
#include "bsp_bldc.h"

#include "app_motor.h"
#include "app_linkage.h"
#include "app_adc.h"

#include "at_command.h"
#include "log.h"

extern UART_HandleTypeDef huart1; // 引用 CubeMX 生成的句柄

void App_Init(void) {
    BSP_BLDC_Init();
    App_Motor_Init();
    App_Adc_Init(); // 启动ADC采样
    AT_Init(&huart1);
	Log_Init(&huart1);
}

void App_Loop(void) {
	//1.处理命令
	AT_MainLoopHandler();
    
    // 2. 业务逻辑
    // App_Adc_Process(); // 已移至 DMA 中断中回调
    App_Motor_Process();
    App_Linkage_Process();
}
