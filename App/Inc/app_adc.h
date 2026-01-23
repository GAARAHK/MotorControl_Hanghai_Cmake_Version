#ifndef APP_ADC_H
#define APP_ADC_H

#include "bsp_bldc.h"
#include "main.h"
#include "app_motor.h"



// ADC 通道定义 (对应缓冲区的索引)
#define AD_IDX_CUR    0 // PA2: BAT_CURRENT
#define AD_IDX_NTC    1 // PA3: ADC_NTC
#define AD_IDX_VOL    2 // PA5: BAT_V
#define AD_IDX_POS    3 // PA7: Location_ADC

// 配置结构体
typedef struct {
    float volt_limit_min;
    float volt_limit_max;
    float temp_limit_max;

    // 分离的每个电机的参数限制
    float curr_limit_max[MAX_MOTORS];
    uint8_t protection_enable;
} AdcProtectionConfig_t;

// 全局数据
typedef struct {
    uint16_t raw[4];      // DMA 原始数据缓冲 (注意这里将来可能要扩大)
    
    // 全局数据
    float voltage_V;      // 总电压
    float temperature_C;  // 板载温度
    
    // 分离的每个电机数据
    float current_A[MAX_MOTORS];   // 每个电机的电流
    uint16_t position[MAX_MOTORS]; // 每个电机的位置原始值
    
    uint8_t error_code;   // 0:正常, 1:过压, 2:欠压, 3:过流, 4:过温
} AppAdcData_t;

extern AppAdcData_t g_adc_data;

// API
void App_Adc_Init(void);

// 新配置接口：通用保护
void App_Adc_ConfigProtect_Global(float v_min, float v_max, float t_max);

// 新配置接口：电机独立参数
void App_Adc_ConfigProtect_Motor(uint8_t id, float i_max);

void App_Adc_Process(void); // 在DMA中断中调用
uint16_t App_Adc_GetPos(uint8_t id); // 获取指定电机位置

#endif
