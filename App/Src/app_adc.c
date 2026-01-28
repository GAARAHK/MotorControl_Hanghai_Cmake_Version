#include "bsp_bldc.h" // 获取 MAX_MOTORS
#include "app_adc.h"
#include "bsp_conf.h" // 引用配置宏
#include "app_motor.h"
#include "log.h"
#include <math.h>

// extern ADC_HandleTypeDef hadc1; // 移除直接 extern，使用 ADC_HANDLE

AppAdcData_t g_adc_data;
static AdcProtectionConfig_t prot_conf = {
    .volt_limit_min = 10.0f,
    .volt_limit_max = 28.0f,
    .temp_limit_max = 85.0f,
    .protection_enable = 1
};

// 转换系数 (需根据实际硬件修改)
#define COEFF_VOLT  (3.3f / 4095.0f * 11.0f) // 假设分压比 11 (10k+1k)
#define COEFF_CURR  (3.3f / 4095.0f * 2.0f)  // 假设 2A/V
#define NTC_BETA    3950.0f
#define NTC_R25     10000.0f
#define NTC_PULLUP  10000.0f

static float Calculate_NTC(uint16_t adc_val) {
    if (adc_val == 0 || adc_val == 4095) return 0.0f;
    float volt = (float)adc_val * 3.3f / 4095.0f;
    float r_ntc = volt * NTC_PULLUP / (3.3f - volt);
    // Steinhart-Hart simplified
    float temp = 1.0f / (logf(r_ntc / NTC_R25) / NTC_BETA + 1.0f / 298.15f) - 273.15f;
    return temp;
}

void App_Adc_Init(void) {
    // 初始化默认保护参数
    for(int i=0; i<MAX_MOTORS; i++) {
        prot_conf.curr_limit_max[i] = 5.0f; // 默认 5A
    }

    // 启动 ADC DMA (循环模式)
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc_data.raw, 4) != HAL_OK) {
        // LOG("ADC Start Failed\r\n");
    }
}

void App_Adc_ConfigProtect_Global(float v_min, float v_max, float t_max) {
    prot_conf.volt_limit_min = v_min;
    prot_conf.volt_limit_max = v_max;
    prot_conf.temp_limit_max = t_max;
}

void App_Adc_ConfigProtect_Motor(uint8_t id, float i_max) {
    if (id < MAX_MOTORS) {
        prot_conf.curr_limit_max[id] = i_max;
    }
}

uint16_t App_Adc_GetPos(uint8_t id) {
    if (id >= MAX_MOTORS) return 0;
    return g_adc_data.position[id];
}

// 供 DMA 中断调用的回调函数 (覆盖 HAL 的弱定义)
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        App_Adc_Process();
    }
}

// 现在的 Process 函数运行在中断里，需要尽可能高效
void App_Adc_Process(void) {
    static uint8_t slow_loop_scaler = 0;
    
    // --- 1. 快速通道 (电流、位置) ---
    // 每次 DMA 完成都处理(10kHz)，保证过流保护和位置控制的实时性
    
    // Motor 1 (假设对应 raw[0] 和 raw[3])
    // 如果有多电机，需根据 ADC Scan Rank 映射索引
    // 例如: raw[0]=Cur1, raw[1]=NTC, raw[2]=Volt, raw[3]=Pos1, raw[4]=Cur2...
    
    // 当前映射:
    // Raw[0] -> AD_IDX_CUR -> Current Motor 0
    // Raw[3] -> AD_IDX_POS -> Position Motor 0
    
    g_adc_data.current_A[0] = (float)g_adc_data.raw[AD_IDX_CUR] * COEFF_CURR;
    g_adc_data.position[0]  = g_adc_data.raw[AD_IDX_POS];

    // 快速保护检查：过流
    if (prot_conf.protection_enable) {
        if (g_adc_data.current_A[0] > prot_conf.curr_limit_max[0]) {
             g_adc_data.error_code = 3; // OC
             App_Motor_Stop(0); // 立即停止该电机
        }
    }

    // --- 2. 慢速通道 (电压、温度) ---
    // NTC 计算包含 logf，耗时较长，降频处理 (例如每 10ms 处理一次)
    slow_loop_scaler++;
    if (slow_loop_scaler >= 100) { // 10kHz / 100 = 100Hz (10ms)
        slow_loop_scaler = 0;
        
        g_adc_data.voltage_V = (float)g_adc_data.raw[AD_IDX_VOL] * COEFF_VOLT;
        g_adc_data.temperature_C = Calculate_NTC(g_adc_data.raw[AD_IDX_NTC]);
        
        // 慢速保护检查
        if (prot_conf.protection_enable) {
            uint8_t err = 0;
            if (g_adc_data.voltage_V < prot_conf.volt_limit_min) err = 2; // UV
            else if (g_adc_data.voltage_V > prot_conf.volt_limit_max) err = 1; // OV
            else if (g_adc_data.temperature_C > prot_conf.temp_limit_max) err = 4; // OT
            
            if (err != 0) {
                g_adc_data.error_code = err;
                // 严重系统故障，停止所有
                for(int i=0; i<MAX_MOTORS; i++) App_Motor_Stop(i);
            } else if (g_adc_data.error_code != 3) { 
                // 如果当前没有过流错误，才清除错误码 (避免覆盖OC状态)
                g_adc_data.error_code = 0;
            }
        }
    }
}
