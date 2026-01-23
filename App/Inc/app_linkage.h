#include <main.h>
#ifndef APP_LINKAGE_H
#define APP_LINKAGE_H

// 联动模式定义
typedef enum {
    LINK_MODE_IDLE = 0, // 停止/手动
    LINK_MODE_1    = 1, // 逻辑1: 往复运动 (时间控制)
    LINK_MODE_2    = 2, // 逻辑2: 多段速运动 (位置控制)
    LINK_MODE_3    = 3, // 逻辑3: 模拟双电机协作 (仅供展示扩展性)
	LINK_MODE_4    = 4,
	LINK_MODE_5    = 5,
    // 在此处添加更多...
} LinkageMode_t;

void App_Linkage_Init(void);
/**
 * @brief 设置联动模式
 * @param mode_id 模式ID (1, 2, 3...)，0表示停止
 * @param loop_count 循环次数 (0:无限循环, 1:跑一次, N:跑N次)
 */
void App_Linkage_SetMode(uint8_t mode_id, uint32_t loop_count);
void App_Linkage_Process(void);

#endif