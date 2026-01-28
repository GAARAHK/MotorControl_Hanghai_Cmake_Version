#include "bsp_bldc.h"
#include "bsp_conf.h" // 引用统一配置

BLDC_Handle_t motors[MAX_MOTORS];

void BSP_BLDC_Init(void) {
    // --- 电机 1 配置 (对应图片引脚) ---
    motors[0].config.htim_pwm = &MOTOR_TIM_HANDLE;        // 使用宏替代
    motors[0].config.pwm_channel = TIM_CHANNEL_1;
    motors[0].config.dir_port = MOTOR1_DIR_GPIO_Port;
    motors[0].config.dir_pin = MOTOR1_DIR_Pin;
    motors[0].config.brake_port = MOTOR1_BRK_GPIO_Port;
    motors[0].config.brake_pin = MOTOR1_BRK_Pin;
    motors[0].config.fg_port = MOTOR1_FG_GPIO_Port;
    motors[0].config.fg_pin = MOTOR1_FG_Pin;
    
    // --- 如果有电机 2, 3，在此处继续赋值 motors[1], motors[2] ---

    // 硬件初始化循环
    for(int i = 0; i < MAX_MOTORS; i++) {
        // 开启PWM
        HAL_TIM_PWM_Start(motors[i].config.htim_pwm, motors[i].config.pwm_channel);
        // 初始状态：停止，刹车
        BSP_BLDC_SetSpeed(i, 0);
        BSP_BLDC_Brake(i, 1);
        BSP_BLDC_ResetPulse(i);
    }
}

void BSP_BLDC_SetSpeed(uint8_t id, uint16_t duty) {
    if(id >= MAX_MOTORS) return;
    if(duty > 1000) duty = 1000;
    
    motors[id].state.current_duty = duty;
    __HAL_TIM_SET_COMPARE(motors[id].config.htim_pwm, motors[id].config.pwm_channel, duty);
}

void BSP_BLDC_SetDir(uint8_t id, MotorDir_t dir) {
    if(id >= MAX_MOTORS) return;
    HAL_GPIO_WritePin(motors[id].config.dir_port, motors[id].config.dir_pin, 
                      (dir == MOTOR_DIR_CW) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_BLDC_Brake(uint8_t id, uint8_t enable) {
    if(id >= MAX_MOTORS) return;
    // 假设di电平刹车
    HAL_GPIO_WritePin(motors[id].config.brake_port, motors[id].config.brake_pin, 
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
    motors[id].state.is_braking = enable;
}

int32_t BSP_BLDC_GetPulse(uint8_t id) {
    if(id >= MAX_MOTORS) return 0;
    return motors[id].state.pulse_count;
}

void BSP_BLDC_ResetPulse(uint8_t id) {
    if(id >= MAX_MOTORS) return;
    motors[id].state.pulse_count = 0;
}

// 统一的FG中断处理
void BSP_BLDC_OnFG_Interrupt(uint16_t GPIO_Pin) {
    for(int i = 0; i < MAX_MOTORS; i++) {
        if(GPIO_Pin == motors[i].config.fg_pin) {
            motors[i].state.pulse_count++;
            // 如果需要根据方向加减计数，可在此处读取 DIR 引脚状态判断
        }
    }
}
