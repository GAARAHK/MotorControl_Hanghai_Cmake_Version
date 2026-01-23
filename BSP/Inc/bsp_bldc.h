#ifndef BSP_BLDC_H
#define BSP_BLDC_H

#include "main.h"

// 将来换芯片控制3个电机时，只需修改此宏为 3
#define MAX_MOTORS 1 

typedef enum {
    MOTOR_DIR_CW = 0,
    MOTOR_DIR_CCW = 1
} MotorDir_t;

// 1. 硬件配置结构体 (只读配置)
typedef struct {
    TIM_HandleTypeDef *htim_pwm;  // PWM定时器句柄
    uint32_t pwm_channel;         // PWM通道
    
    GPIO_TypeDef *dir_port;       // 方向端口
    uint16_t dir_pin;
    
    GPIO_TypeDef *brake_port;     // 刹车端口
    uint16_t brake_pin;
    
    // FG 反馈引脚 (采用EXTI外部中断计数)
    GPIO_TypeDef *fg_port;
    uint16_t fg_pin;
} BLDC_Config_t;

// 2. 运行时状态结构体 (变量)
typedef struct {
    volatile int32_t pulse_count; // 累计脉冲数
    uint16_t current_duty;        // 当前占空比 (0-1000)
    uint8_t is_braking;           // 刹车状态
} BLDC_State_t;

// 3. 电机对象句柄
typedef struct {
    BLDC_Config_t config;
    BLDC_State_t state;
} BLDC_Handle_t;

// 全局电机数组
extern BLDC_Handle_t motors[MAX_MOTORS];

// API
void BSP_BLDC_Init(void);
void BSP_BLDC_SetSpeed(uint8_t id, uint16_t duty);
void BSP_BLDC_SetDir(uint8_t id, MotorDir_t dir);
void BSP_BLDC_Brake(uint8_t id, uint8_t enable);
int32_t BSP_BLDC_GetPulse(uint8_t id);
void BSP_BLDC_ResetPulse(uint8_t id);

// 在 GPIO EXTI 中断中调用此函数
void BSP_BLDC_OnFG_Interrupt(uint16_t GPIO_Pin);

#endif
