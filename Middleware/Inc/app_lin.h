#ifndef APP_LIN_H
#define APP_LIN_H

#include "main.h"

// LIN ID 定义 (根据需求自定义)
#define LIN_ID_CMD_RUN      0x30 // 指令: 运行/停止 (Data: Type, ID, Dir, SpeedH, SpeedL...)
#define LIN_ID_CMD_POS      0x31 // 指令: 位置控制 (Data: ID, Dir, SpdH, SpdL, Pos3, Pos2, Pos1, Pos0)
#define LIN_ID_CMD_ADC      0x32 // 指令: ADC位置 (Data: ID, SpdH, SpdL, TgtH, TgtL, Tol, Rng)
#define LIN_ID_QUERY        0x33 // 指令: 查询请求 (Data: ID)

#define LIN_ID_RESP_STATUS  0x34 // 响应: 状态反馈 (Data: ID, Busy, Pulses...)

// LIN 协议状态
typedef enum {
    LIN_STATE_IDLE,
    LIN_STATE_BREAK,
    LIN_STATE_SYNC,
    LIN_STATE_PID,
    LIN_STATE_DATA,
    LIN_STATE_CHECKSUM
} LinState_t;

void App_LIN_Init(void);
void App_LIN_Process(void);
void App_LIN_IRQHandler(void);
void App_LIN_ErrorCallback(UART_HandleTypeDef *huart); // 新增

#endif
