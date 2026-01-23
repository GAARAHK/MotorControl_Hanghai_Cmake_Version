#include "app_motor.h"
#include "bsp_bldc.h"
#include "app_adc.h"
#include <stdlib.h> // for abs if needed

typedef struct {
    CtrlMode_t mode;
    uint32_t start_tick;
    uint32_t target_time;
    int32_t target_pulse;
    // ADC 位置控制相关
    uint16_t target_adc;
    uint16_t adc_tolerance;
    uint16_t adc_decel_range;

    // 运行时参数
    uint16_t cruise_speed; // 巡航速度(目标最高速)
    uint8_t  dir;
    
    // 配置参数
    uint32_t decel_range_pulses; // 开始减速的距离(脉冲数)
    uint16_t min_approach_speed; // 最后的蠕动速度

    // 限位开关配置
    uint8_t use_limit;           // 本次运动是否启用限位
    uint8_t limit_configured;    // 是否已配置硬件引脚
    GPIO_TypeDef* port_cw; 
    uint16_t pin_cw;
    GPIO_TypeDef* port_ccw;
    uint16_t pin_ccw;
    GPIO_PinState limit_level;   // 触发电平
} AppMotorCtrl_t;

static AppMotorCtrl_t ctrl_vars[MAX_MOTORS];

void App_Motor_Init(void) {
    for(int i=0; i<MAX_MOTORS; i++) {
        ctrl_vars[i].mode = CTRL_STOP;
        // 默认减速配置
        ctrl_vars[i].decel_range_pulses = 100; // 默认剩下100脉冲开始减速
        ctrl_vars[i].min_approach_speed = 300; // 最低PWM占空比
        ctrl_vars[i].limit_configured = 0;
    }
}

// 供用户配置预减速参数
void App_Motor_ConfigDecel(uint8_t id, uint32_t decel_pulses, uint16_t min_speed) {
    if(id >= MAX_MOTORS) return;
    ctrl_vars[id].decel_range_pulses = decel_pulses;
    ctrl_vars[id].min_approach_speed = min_speed;
}

// 供用户配置限位开关
void App_Motor_ConfigLimit(uint8_t id, GPIO_TypeDef* cw_port, uint16_t cw_pin, 
                           GPIO_TypeDef* ccw_port, uint16_t ccw_pin, GPIO_PinState active_level) 
{
    if(id >= MAX_MOTORS) return;
    ctrl_vars[id].port_cw = cw_port;
    ctrl_vars[id].pin_cw = cw_pin;
    ctrl_vars[id].port_ccw = ccw_port;
    ctrl_vars[id].pin_ccw = ccw_pin;
    ctrl_vars[id].limit_level = active_level;
    ctrl_vars[id].limit_configured = 1;
}

void App_Motor_MoveTime(uint8_t id, uint8_t dir, uint16_t speed, uint32_t ms) {
    if(id >= MAX_MOTORS) return;
    
    ctrl_vars[id].mode = CTRL_RUN_TIME;
    ctrl_vars[id].target_time = ms;
    ctrl_vars[id].start_tick = HAL_GetTick();
    ctrl_vars[id].use_limit = 0; // 默认不启用，如需启用可自行修改或增加接口
    
    BSP_BLDC_Brake(id, 0); // 松刹车
    BSP_BLDC_SetDir(id, (MotorDir_t)dir);
    BSP_BLDC_SetSpeed(id, speed);
}

void App_Motor_MovePos(uint8_t id, uint8_t dir, uint16_t speed, int32_t pulses) {
    if(id >= MAX_MOTORS) return;
    
    ctrl_vars[id].mode = CTRL_RUN_POS;
    ctrl_vars[id].target_pulse = pulses;
    ctrl_vars[id].cruise_speed = speed;
    ctrl_vars[id].dir = dir;
    ctrl_vars[id].use_limit = 0; // 原接口不启用限位
    
    BSP_BLDC_ResetPulse(id); // 归零，相对运动
    
    BSP_BLDC_Brake(id, 0);
    BSP_BLDC_SetDir(id, (MotorDir_t)dir);
    
    // 刚启动时，如果总距离很短，可能直接进入减速区
    if (pulses <= (int32_t)ctrl_vars[id].decel_range_pulses) {
         BSP_BLDC_SetSpeed(id, ctrl_vars[id].min_approach_speed);
    } else {
         BSP_BLDC_SetSpeed(id, speed);
    }
}

void App_Motor_MovePosWithLimit(uint8_t id, uint8_t dir, uint16_t speed, int32_t pulses) {
    if(id >= MAX_MOTORS) return;
    
    // 复用 MovePos 逻辑，仅置位 use_limit
    App_Motor_MovePos(id, dir, speed, pulses);
    
    // 如果配置了引脚，则开启检测限位开关
    if (ctrl_vars[id].limit_configured) {
        ctrl_vars[id].use_limit = 1;
    }
}
void App_Motor_MoveAdcPos(uint8_t id, uint16_t speed, uint16_t target_adc, uint16_t tolerance, uint16_t range_adc) {
    if(id >= MAX_MOTORS) return;

    ctrl_vars[id].mode = CTRL_RUN_ADC_POS;
    ctrl_vars[id].target_adc = target_adc;
    ctrl_vars[id].adc_tolerance = tolerance;
    ctrl_vars[id].adc_decel_range = range_adc;
    ctrl_vars[id].cruise_speed = speed;
    ctrl_vars[id].min_approach_speed = 300; // 默认最小速度
    ctrl_vars[id].use_limit = 0; // 默认不开限位，如需可另加接口

    // 初始判断方向
    uint16_t current_adc = App_Adc_GetPos(id);
    if (abs((int)current_adc - (int)target_adc) <= tolerance) {
        App_Motor_Stop(id);
        return;
    }

    // 简单逻辑：目标大则正转，目标小则反转（需根据实际传感器安装方向确认）
    // 假设：ADC增大 = 正转
    if (target_adc > current_adc) {
        ctrl_vars[id].dir = MOTOR_DIR_CW;
    } else {
        ctrl_vars[id].dir = MOTOR_DIR_CCW;
    }

    BSP_BLDC_Brake(id, 0);
    BSP_BLDC_SetDir(id, (MotorDir_t)ctrl_vars[id].dir);
    BSP_BLDC_SetSpeed(id, speed);
}

void App_Motor_MoveAdcPosWithLimit(uint8_t id, uint16_t speed, uint16_t target_adc, uint16_t tolerance, uint16_t range_adc) {
    if(id >= MAX_MOTORS) return;
    
    // 复用 MoveAdcPos 逻辑
    App_Motor_MoveAdcPos(id, speed, target_adc, tolerance, range_adc);
    
    // 如果配置了引脚，则开启检测限位开关
    if (ctrl_vars[id].limit_configured) {
        ctrl_vars[id].use_limit = 1;
    }
}
void App_Motor_Stop(uint8_t id) {
    if(id >= MAX_MOTORS) return;
    ctrl_vars[id].mode = CTRL_STOP;
    BSP_BLDC_SetSpeed(id, 0);
    BSP_BLDC_Brake(id, 1);
}

void App_Motor_Process(void) {
    for(int i=0; i<MAX_MOTORS; i++) {
        // 限位开关检测
        if (ctrl_vars[i].use_limit && ctrl_vars[i].mode != CTRL_STOP) {
            uint8_t stop_req = 0;
            // 正转检测 CW 开关
            if (ctrl_vars[i].dir == MOTOR_DIR_CW && ctrl_vars[i].port_cw != NULL) {
                if (HAL_GPIO_ReadPin(ctrl_vars[i].port_cw, ctrl_vars[i].pin_cw) == ctrl_vars[i].limit_level) {
                   stop_req = 1;
                }
            }
            // 反转检测 CCW 开关
            else if (ctrl_vars[i].dir == MOTOR_DIR_CCW && ctrl_vars[i].port_ccw != NULL) {
                if (HAL_GPIO_ReadPin(ctrl_vars[i].port_ccw, ctrl_vars[i].pin_ccw) == ctrl_vars[i].limit_level) {
                   stop_req = 1;
                }
            }
            
            if (stop_req) {
                 App_Motor_Stop(i);
                 continue; // 跳过后续逻辑
            }
        }

        if (ctrl_vars[i].mode == CTRL_RUN_TIME) {
            if (HAL_GetTick() - ctrl_vars[i].start_tick >= ctrl_vars[i].target_time) {
                App_Motor_Stop(i);
            }
        }
        else if (ctrl_vars[i].mode == CTRL_RUN_POS) {
            int32_t current_p = abs(BSP_BLDC_GetPulse(i));
            int32_t remain = ctrl_vars[i].target_pulse - current_p;
            
            if (remain <= 0) {
                App_Motor_Stop(i);
            }
            else if (remain <= (int32_t)ctrl_vars[i].decel_range_pulses) {
                // 进入减速区，计算线性减速
                // 目标速度 = Min + (Max - Min) * (Remain / Range)
                uint32_t speed_range = ctrl_vars[i].cruise_speed - ctrl_vars[i].min_approach_speed;
                // 防止 cruise_speed 比 min 还小的情况
                if (ctrl_vars[i].cruise_speed < ctrl_vars[i].min_approach_speed) speed_range = 0;
                
                uint16_t new_speed = ctrl_vars[i].min_approach_speed + 
                                     (speed_range * remain) / ctrl_vars[i].decel_range_pulses;
                
                BSP_BLDC_SetSpeed(i, new_speed);
            }
        }
        else if (ctrl_vars[i].mode == CTRL_RUN_ADC_POS) {
            uint16_t current_adc = App_Adc_GetPos(i); // 获取对应电机位置
            int diff = (int)ctrl_vars[i].target_adc - (int)current_adc;
            int abs_diff = abs(diff);

            // 1. 判断是否到位
            if (abs_diff <= ctrl_vars[i].adc_tolerance) {
                App_Motor_Stop(i);
            }
            else {
                // 2. 动态调整方向 (防止越过目标后无法回头)
                // 假设 CW 增加 ADC
                uint8_t needed_dir = (diff > 0) ? MOTOR_DIR_CW : MOTOR_DIR_CCW;
                if (needed_dir != ctrl_vars[i].dir) {
                    ctrl_vars[i].dir = needed_dir;
                    BSP_BLDC_SetDir(i, (MotorDir_t)needed_dir);
                }

                // 3. 计算速度 (预减速)
                uint16_t set_speed = ctrl_vars[i].cruise_speed;
                if (abs_diff <= ctrl_vars[i].adc_decel_range) {
                    uint32_t speed_range = ctrl_vars[i].cruise_speed - ctrl_vars[i].min_approach_speed;
                    if (ctrl_vars[i].cruise_speed < ctrl_vars[i].min_approach_speed) speed_range = 0;
                    
                    set_speed = ctrl_vars[i].min_approach_speed + 
                                (speed_range * abs_diff) / ctrl_vars[i].adc_decel_range;
                }
                BSP_BLDC_SetSpeed(i, set_speed);
            }
        }
    }
}

uint8_t App_Motor_IsBusy(uint8_t id) {
    if(id >= MAX_MOTORS) return 0;
    return (ctrl_vars[id].mode != CTRL_STOP);
}

void App_Motor_MoveManual(uint8_t id, uint8_t dir, uint16_t speed) {
    if(id >= MAX_MOTORS) return;
    
    // 设置内部状态为手动
    ctrl_vars[id].mode = CTRL_RUN_MANUAL;
    
    // 复位脉冲（可选，看需求）
    BSP_BLDC_ResetPulse(id);
    
    // 直接下发指令
    BSP_BLDC_Brake(id, 0);
    BSP_BLDC_SetDir(id, (MotorDir_t)dir);
    BSP_BLDC_SetSpeed(id, speed);
}

// duplicate App_Motor_Process removed; use the primary implementation above.
