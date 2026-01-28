#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include "main.h"

// STM32F103C8T6 Flash: 64KB
// Page Size: 1KB (Medium Density)
// Last Page Address: 0x0800FC00 (Starting address of the last 1KB page)
#define STORAGE_FLASH_PAGE_ADDR  0x0800FC00 
#define STORAGE_MAGIC            0xA5A55A5A 

// 数据结构定义 (必须4字节对齐)
typedef struct {
    uint32_t magic;         // 魔数，用于检测是否已初始化
    uint32_t device_id;     // 设备ID
    uint32_t baud_rate;     // 波特率 (预留)
    uint32_t motor_limit_triggered_val; // 上次限位触发值 (示例扩展)
    // 在此处添加更多字段...
    
    // 自动填充至 1KB 边界 (可选)
} AppConfig_t;

extern AppConfig_t g_Config;

// API
void App_Storage_Init(void);
void App_Storage_Save(void);
void App_Storage_SetDeviceID(uint8_t id);
void App_Storage_ResetFactory(void);

#endif
