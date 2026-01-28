#include "app_storage.h"
#include <string.h>

// 全局配置实例
AppConfig_t g_Config;

// 默认参数配置
static void SetDefaultConfig(void) {
    g_Config.magic = STORAGE_MAGIC;
    g_Config.device_id = 1;         // 默认ID = 1
    g_Config.baud_rate = 115200;    // 默认波特率
    g_Config.motor_limit_triggered_val = 0;
}

// 初始化: 从Flash加载参数
void App_Storage_Init(void) {
    AppConfig_t *pStoredConfig = (AppConfig_t *)STORAGE_FLASH_PAGE_ADDR;

    // 检查魔数
    if (pStoredConfig->magic == STORAGE_MAGIC) {
        // 有效数据，从Flash拷贝到RAM
        memcpy(&g_Config, pStoredConfig, sizeof(AppConfig_t));
    } else {
        // 第一次使用或Flash损坏，加载默认并保存
        SetDefaultConfig();
        App_Storage_Save();
    }
}

// 保存当前 RAM 中的 g_Config 到 Flash
void App_Storage_Save(void) {
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    // 1. 解锁 Flash
    HAL_FLASH_Unlock();

    // 2. 擦除最后一页
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = STORAGE_FLASH_PAGE_ADDR;
    EraseInitStruct.NbPages     = 1;

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
    if (status != HAL_OK) {
        // 错误处理: 可通过 LOG 打印 Error Code
        HAL_FLASH_Lock();
        return;
    }

    // 3. 编程 (写入)
    // Flash 必须按 Word (32-bit) 或 Half-Word 写入。这里按 Word 写入。
    uint32_t *pSource = (uint32_t *)&g_Config;
    uint32_t DestAddr = STORAGE_FLASH_PAGE_ADDR;
    uint32_t WordsToWrite = (sizeof(AppConfig_t) + 3) / 4; // 向上取整

    for (uint32_t i = 0; i < WordsToWrite; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, DestAddr, *pSource);
        if (status != HAL_OK) {
            // 写入错误
            break;
        }
        DestAddr += 4;
        pSource++;
    }

    // 4. 上锁
    HAL_FLASH_Lock();
}

// 修改参数接口示例: Set ID
void App_Storage_SetDeviceID(uint8_t id) {
    if (g_Config.device_id != id) {
        g_Config.device_id = id;
        App_Storage_Save();
    }
}

// 恢复出厂
void App_Storage_ResetFactory(void) {
    SetDefaultConfig();
    App_Storage_Save();
}
