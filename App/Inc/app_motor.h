#ifndef APP_MOTOR_H
#define APP_MOTOR_H
#include "main.h"

// 运动控制模式
typedef enum {
    CTRL_STOP,
    CTRL_RUN_MANUAL, // 手动一直跑
    CTRL_RUN_TIME,   // 跑时间
    CTRL_RUN_POS,    // 跑位置
    CTRL_RUN_ADC_POS // 跑绝对ADC位置
} CtrlMode_t;

void App_Motor_Init(void);
void App_Motor_Process(void); // 周期调用

// 功能接口
void App_Motor_MoveTime(uint8_t id, uint8_t dir, uint16_t speed, uint32_t ms);
void App_Motor_MovePos(uint8_t id, uint8_t dir, uint16_t speed, int32_t pulses);
//void App_Motor_MovePosWithLimit(uint8_t id, uint8_t dir, uint16_t speed, int32_t pulses);

// 新增：移动到指定 ADC 位置
// target_adc: 目标 ADC 值 (0-4095)
// speed: 最大速度
// tolerance: 允许误差范围 (例如 10)
// range_adc: 减速范围 (ADC差值)
void App_Motor_MoveAdcPos(uint8_t id, uint16_t speed, uint16_t target_adc, uint16_t tolerance, uint16_t range_adc);
void App_Motor_MoveAdcPosWithLimit(uint8_t id, uint16_t speed, uint16_t target_adc, uint16_t tolerance, uint16_t range_adc);

// 配置接口：设置预减速参数
void App_Motor_ConfigDecel(uint8_t id, uint32_t decel_pulses, uint16_t min_speed);

// 配置接口：设置限位开关 (cw_pin: 正转限位, ccw_pin: 反转限位, active_level: 触发电平 0或1)
void App_Motor_ConfigLimit(uint8_t id, GPIO_TypeDef* cw_port, uint16_t cw_pin, 
                           GPIO_TypeDef* ccw_port, uint16_t ccw_pin, GPIO_PinState active_level);

// 新增功能：带限位检测的位置移动
void App_Motor_MovePosWithLimit(uint8_t id, uint8_t dir, uint16_t speed, int32_t pulses);

void App_Motor_Stop(uint8_t id);
uint8_t App_Motor_IsBusy(uint8_t id); // 返回1表示正在运行

// 新增：手动模式接口
void App_Motor_MoveManual(uint8_t id, uint8_t dir, uint16_t speed);

#endif
