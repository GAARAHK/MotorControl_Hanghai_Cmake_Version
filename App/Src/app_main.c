#include "app_main.h"
#include "bsp_bldc.h"

#include "app_motor.h"
#include "app_linkage.h"
#include "app_adc.h"
#include "app_storage.h"

#include "at_command.h"
#include "log.h"
#include "bsp_conf.h"

// extern UART_HandleTypeDef huart1; // 移除

void App_Init(void) {
    App_Storage_Init(); // 优先初始化存储，获取ID等配置
    BSP_BLDC_Init();
    App_Motor_Init();
    App_Adc_Init(); // 启动ADC采样
    AT_Init(&LOG_UART_HANDLE); // 使用宏
	Log_Init(&LOG_UART_HANDLE); // 使用宏

        // 初始化联动模块并默认启动 Mode 6 (模拟往复)
    App_Linkage_Init(); 
    App_Linkage_SetMode(6, 0); 


}

void App_Loop(void) {
	//1.处理命令
	AT_MainLoopHandler();
    
    // 2. 业务逻辑
    // App_Adc_Process(); // 已移至 DMA 中断中回调
    App_Motor_Process();
    App_Linkage_Process();
}
