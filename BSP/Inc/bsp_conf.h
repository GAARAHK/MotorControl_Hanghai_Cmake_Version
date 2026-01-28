#ifndef BSP_CONF_H
#define BSP_CONF_H

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "adc.h"
#include "dma.h"

// --- 电机相关 ---
// 电机PWM定时器
#define MOTOR_TIM_HANDLE        htim4
extern TIM_HandleTypeDef        MOTOR_TIM_HANDLE;

// --- 采样相关 ---
// 基础定时器 (10kHz心跳)
#define BASE_TIM_HANDLE         htim3
extern TIM_HandleTypeDef        BASE_TIM_HANDLE;

// ADC & DMA
#define ADC_HANDLE              hadc1
#define DMA_ADC_HANDLE          hdma_adc1
extern ADC_HandleTypeDef        ADC_HANDLE;
extern DMA_HandleTypeDef        DMA_ADC_HANDLE;

// --- 通信相关 ---
// 日志/AT指令串口
#define LOG_UART_HANDLE         huart1
extern UART_HandleTypeDef       LOG_UART_HANDLE;

// LIN 总线串口
#define LIN_UART_HANDLE         huart3
extern UART_HandleTypeDef       LIN_UART_HANDLE;

#endif
