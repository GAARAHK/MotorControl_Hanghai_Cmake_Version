#include "app_lin.h"
#include "usart.h"
#include "app_motor.h"
#include "app_linkage.h"
#include "bsp_bldc.h"
#include "app_adc.h"
#include "log.h"
#include <string.h>

extern UART_HandleTypeDef huart3;

// LIN 状态机变量
static volatile LinState_t lin_state = LIN_STATE_IDLE;
static volatile uint8_t lin_rx_buffer[9]; // 8 Bytes Data + 1 Checksum
static volatile uint8_t lin_data_idx = 0;
static volatile uint8_t lin_current_id = 0;
static volatile uint8_t lin_data_len = 0;

// 从 ID 获取数据长度 (简单的固定长度 8 字节，或查表)
static uint8_t GetLenFromID(uint8_t id) {
    // 简单起见，假设我们定义的 ID 都是 8 字节数据
    // 标准 LIN 需根据 ID[5:4] 判断，这里简化
    return 8;
}

// 校验和计算 (Classic or Enhanced)
static uint8_t CalcChecksum(uint8_t id, const uint8_t *data, uint8_t len) {
    uint16_t sum = 0;
    // Enhanced Checksum includes PID (ID)
    // Classic only Data. Let's use Classic for simplicity or Enhanced if standard.
    // 许多工具默认 Enhanced.
    sum += id; 
    
    for (int i = 0; i < len; i++) {
        sum += data[i];
        if (sum > 0xFF) sum -= 0xFF;
    }
    return (uint8_t)(~sum);
}

// 解析指令
static void Execute_LIN_Command(uint8_t id, uint8_t *data) {
    uint8_t motor_id, dir, cmd_type;
    uint16_t speed, target_adc, tolerance, range_adc;
    int32_t pulses;

    motor_id = data[1]; // 通用格式: Byte 1 是 MotorID (1-based)

    switch (id) {
        case LIN_ID_CMD_RUN: // [CMD(1=Run,2=Stop,3=Time), ID, Dir, SpdH, SpdL, TimeH, TimeL, Res]
            cmd_type = data[0];
            if (motor_id < 1 || motor_id > MAX_MOTORS) return;
            dir = data[2];
            speed = (data[3] << 8) | data[4];
            uint16_t time_ms = (data[5] << 8) | data[6];

            App_Linkage_SetMode(0, 1); // 停止联动

            if (cmd_type == 1) { // Manual Run
                App_Motor_MoveManual(motor_id - 1, dir, speed);
            } else if (cmd_type == 2) { // Stop
                App_Motor_Stop(motor_id - 1);
            } else if (cmd_type == 3) { // Time
                App_Motor_MoveTime(motor_id - 1, dir, speed, time_ms);
            }
            break;

        case LIN_ID_CMD_POS: // [CMD(Dummy), ID, Dir, SpdH, SpdL, Pos3, Pos2, Pos1, Pos0]
            if (motor_id < 1 || motor_id > MAX_MOTORS) return;
            dir = data[2];
            speed = (data[3] << 8) | data[4];
            pulses = (data[5] << 24) | (data[6] << 16) | (data[7] << 8) | data[8]; // Oops, index overflow?
            // Frame is 8 bytes. data[0]..data[7].
            // Re-map: [ID, Dir, SpdH, SpdL, P3, P2, P1, P0]
            motor_id = data[0];
            if (motor_id < 1 || motor_id > MAX_MOTORS) return;
            dir = data[1];
            speed = (data[2] << 8) | data[3];
            pulses = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
            
            App_Linkage_SetMode(0, 1);
            // 默认带限位
            App_Motor_MovePosWithLimit(motor_id - 1, dir, speed, pulses);
            break;

        case LIN_ID_CMD_ADC: // [ID, SpdH, SpdL, AdcH, AdcL, Tol, RngH, RngL]
            motor_id = data[0];
            if (motor_id < 1 || motor_id > MAX_MOTORS) return;
            speed = (data[1] << 8) | data[2];
            target_adc = (data[3] << 8) | data[4];
            tolerance = data[5];
            range_adc = (data[6] << 8) | data[7];

            App_Linkage_SetMode(0, 1);
            App_Motor_MoveAdcPosWithLimit(motor_id - 1, speed, target_adc, tolerance, range_adc);
            break;
            
        case LIN_ID_QUERY: // [ID, ...]
             // 这里通常需要作为Slave发送响应。
             // STM32 HAL LIN Slave 发送比较复杂，需要预先填充数据等到 Master 发送 Header。
             // 暂不实现 Slave Response，仅实现接收控制。
             break;
    }
}


void App_LIN_Init(void) {
    // 1. 使能 PHY (TJA1021 SLP_N -> High)
    HAL_GPIO_WritePin(LIN_SLEEP_GPIO_Port, LIN_SLEEP_Pin, GPIO_PIN_SET);
    
    // 2. 开启断帧检测中断 (LBDIE) 和 接收非空中断 (RXNEIE)
    // 注意: HAL_LIN_Init 默认只配置了基本参数
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_LBD);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
    
    // LOG("LIN Init OK\r\n");
}

void App_LIN_Process(void) {
    // 轮询或处理复杂逻辑，当前逻辑全在中断中完成解析，这里留空即可
}

// 放入 stm32f1xx_it.c 的 USART3_IRQHandler 中
void App_LIN_IRQHandler(void) {
    uint32_t isrflags = READ_REG(huart3.Instance->SR);
    uint32_t cr1its   = READ_REG(huart3.Instance->CR1);
    uint32_t cr2its   = READ_REG(huart3.Instance->CR2);
    
    // 1. 检测到 Break (LBD)
    if ((isrflags & USART_SR_LBD) && (cr2its & USART_CR2_LBDIE)) {
        __HAL_UART_CLEAR_FLAG(&huart3, UART_FLAG_LBD);
        lin_state = LIN_STATE_BREAK;
        // 等待 Sync Field (0x55)
    }
    
    // 2. 接收数据 (RXNE)
    if ((isrflags & USART_SR_RXNE) && (cr1its & USART_CR1_RXNEIE)) {
        uint8_t data = (uint8_t)(huart3.Instance->DR & 0xFF);
        
        switch (lin_state) {
            case LIN_STATE_BREAK:
                if (data == 0x55) {
                    lin_state = LIN_STATE_SYNC;
                } else {
                    lin_state = LIN_STATE_IDLE; // Sync Error
                }
                break;
                
            case LIN_STATE_SYNC:
                lin_current_id = data; // PID (Frame ID + Parity)
                // 这里应该校验 PID Parity，简化跳过
                lin_data_len = GetLenFromID(lin_current_id & 0x3F);
                lin_data_idx = 0;
                lin_state = LIN_STATE_DATA;
                break;
                
            case LIN_STATE_DATA:
                lin_rx_buffer[lin_data_idx++] = data;
                if (lin_data_idx >= lin_data_len) {
                    lin_state = LIN_STATE_CHECKSUM;
                }
                break;
                
            case LIN_STATE_CHECKSUM:
                // 校验 Checksum
                 uint8_t expected = CalcChecksum(lin_current_id, (uint8_t*)lin_rx_buffer, lin_data_len);
                 if (data == expected) {
                    // 校验过，执行命令
                    Execute_LIN_Command(lin_current_id & 0x3F, (uint8_t*)lin_rx_buffer);
                 }
                lin_state = LIN_STATE_IDLE;
                break;
                
            default:
                // IDLE 状态收到数据，通常忽略
                break;
        }
    }
}
