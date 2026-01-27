#include "at_command.h"
//#include "cmd_motor.h"
//#include "cmd_parser.h"
#include "log.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h> // for abs if needed

#include "app_motor.h"    // 引用业务层接口
#include "app_linkage.h"  // 引用联动接口
#include "app_adc.h"      // 引用ADC数据接口
#include "bsp_bldc.h"     // 引用底层获取状态

// 接收缓冲区
static uint8_t at_rx_buffer[AT_RX_BUFFER_SIZE];
static char at_cmd_buffer[AT_CMD_MAX_LEN];
static bool at_cmd_ready = false;
static uint16_t at_cmd_len = 0;


// 用于通信的 UART 句柄
static UART_HandleTypeDef *at_uart;


// --- 内部函数声明 ---
static AtCmdStatus_t Process_Run(char *params);
static AtCmdStatus_t Process_Time(char *params);
static AtCmdStatus_t Process_Pos(char *params);
static AtCmdStatus_t Process_Stop(char *params);
static AtCmdStatus_t Process_Query(char *params);
static AtCmdStatus_t Process_Link(char *params);
// 新增指令声明
static AtCmdStatus_t Process_AdcMove(char *params);
static AtCmdStatus_t Process_AdcMoveLim(char *params);
static AtCmdStatus_t Process_CfgDecel(char *params);
static AtCmdStatus_t Process_GetAdc(char *params);


// 初始化 AT 命令处理器
void AT_Init(UART_HandleTypeDef *huart) {
    at_uart = huart;
    at_cmd_ready = false;
    at_cmd_len = 0;
    

    
    // 清空接收缓冲区
    memset(at_rx_buffer, 0, AT_RX_BUFFER_SIZE);
    
    // 使能UART空闲中断
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
    
    // 启动DMA接收
    HAL_UART_Receive_DMA(huart, at_rx_buffer, AT_RX_BUFFER_SIZE);
    
    //LOG_INFO("AT Command processor initialized with DMA\r\n");
}

/**
  * @brief  AT命令主循环处理函数，应在主循环中调用
  * @param  无
  * @retval 无
  */
void AT_MainLoopHandler(void) {
    // 处理命令
    if (at_cmd_ready) {
        at_cmd_ready = false;
        AT_ProcessCommand(at_cmd_buffer);
    }
    
    // 执行其他周期性任务
    //MotorCmd_PeriodicHandler();
}

/**
  * @brief  UART空闲中断回调函数
  * @param  huart UART句柄
  * @retval 无
  */
void AT_UART_IdleCallback(UART_HandleTypeDef *huart) {
    if (huart == at_uart) {
        // 空闲中断表示一帧数据接收完成
        
        // 计算接收到的数据长度
        uint16_t dma_counter = __HAL_DMA_GET_COUNTER(huart->hdmarx);
        at_cmd_len = AT_RX_BUFFER_SIZE - dma_counter;
        
        // 暂停DMA接收
        HAL_StatusTypeDef status = HAL_UART_DMAStop(huart);
        if (status != HAL_OK) {
            LOG_WARN("Failed to stop DMA, status: %d\r\n", status);
        }
        
        // 检查接收的数据是否合法
        if (at_cmd_len > 0 && at_cmd_len < AT_CMD_MAX_LEN) {
            // 复制到命令缓冲区并移除可能的\r\n
            memcpy(at_cmd_buffer, at_rx_buffer, at_cmd_len);
            
            // 查找并移除结尾的\r\n
            if (at_cmd_len >= 2 && 
                at_rx_buffer[at_cmd_len-2] == '\r' && 
                at_rx_buffer[at_cmd_len-1] == '\n') {
                at_cmd_len -= 2;
            }
            
            at_cmd_buffer[at_cmd_len] = '\0';
            
            LOG_DEBUG("Command received: %s\r\n", at_cmd_buffer);
            
            // 设置命令就绪标志
            at_cmd_ready = true;
            
            // 优先处理关键命令
            if (strstr(at_cmd_buffer, "AT+STOP") != NULL ||
                strstr(at_cmd_buffer, "AT+MotorStopCustom") != NULL ||
                strstr(at_cmd_buffer, "AT+MotorStatusUpLoadStop") != NULL) {
                
                // 立即处理停止类命令
                AT_ProcessCommand(at_cmd_buffer);
                at_cmd_ready = false;
                
                LOG_INFO("Priority command processed immediately\r\n");
            }
        }
        
        // 清空缓冲区
        memset(at_rx_buffer, 0, AT_RX_BUFFER_SIZE);
        
       
        
        // 重新启动DMA接收
        status = HAL_UART_Receive_DMA(huart, at_rx_buffer, AT_RX_BUFFER_SIZE);
        if (status != HAL_OK) {
            LOG_ERROR("Failed to restart DMA reception, status: %d\r\n", status);
        }
        

    }
}


/**
  * @brief  UART DMA 接收完成回调
  * @param  huart UART句柄
  * @retval 无
  */
void AT_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == at_uart) {
       
       
    }
}






/**
  * @brief  向上位机发送响应
  * @param  format 格式化字符串
  * @param  ... 可变参数
  * @retval 无
  */
void AT_SendResponse(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len > 0 && len < (int)sizeof(buffer)) {
        // 添加结束符 \r\n
        buffer[len++] = '\r';
        buffer[len++] = '\n';
        
        // 通过 UART 发送
        HAL_UART_Transmit(at_uart, (uint8_t*)buffer, len, 100);
    }
} 

/**
  * @brief  处理完整的AT命令
  * @param  cmd 命令字符串
  * @retval 命令处理状态
  */
AtCmdStatus_t AT_ProcessCommand(char *cmd) {
    //LOG_DEBUG("Processing AT command: %s\r\n", cmd);
    
    // 检查是否是AT命令
    if (strncmp(cmd, "AT+", 3) != 0) {
        LOG_WARN("Invalid AT command format\r\n");
        return AT_ERROR;
    }
    
    // 提取命令名称
    char *cmd_name = cmd + 3;  // 跳过 "AT+"
    
   
    
    // 处理带参数的命令
    char *param_start = strchr(cmd_name, '=');
    if (param_start != NULL) {
        *param_start = '\0';  // 分隔命令名和参数
        param_start++;        // 参数开始位置
        
		// 2. 命令路由
    if (strcmp(cmd_name, "RUN") == 0)      return Process_Run(param_start);
    if (strcmp(cmd_name, "TIME") == 0)     return Process_Time(param_start);
    if (strcmp(cmd_name, "POS") == 0)      return Process_Pos(param_start);
    if (strcmp(cmd_name, "STOP") == 0)     return Process_Stop(param_start);
    if (strcmp(cmd_name, "QUERY") == 0)    return Process_Query(param_start);
    if (strcmp(cmd_name, "LINK") == 0)     return Process_Link(param_start);
    
    // 新增指令路由
    if (strcmp(cmd_name, "ADCMOVE") == 0)    return Process_AdcMove(param_start);
    if (strcmp(cmd_name, "ADCMOVELIM") == 0) return Process_AdcMoveLim(param_start);
    if (strcmp(cmd_name, "CFGDECEL") == 0)   return Process_CfgDecel(param_start);
    if (strcmp(cmd_name, "GETADC") == 0)     return Process_GetAdc(param_start);

        // 处理各种命令...
//        if (strcmp(cmd_name, "MotorRun") == 0) {
//            return MotorCmd_Run(param_start);
//        }
//        else if (strcmp(cmd_name, "MotorStop") == 0) {
//            return MotorCmd_Stop(param_start);
//        }
	
    }
    else {
        // 处理不带参数的命令
//        if (strcmp(cmd_name, "QueryVersion") == 0) {
//            return MotorCmd_QueryVersion();
//        }
//		

    }
    
    LOG_WARN("Unknown AT command: %s\r\n", cmd_name);
    return AT_UNKNOWN_CMD;
}

/**
  * @brief  发送AT错误响应
  * @param  status 错误状态
  * @retval 无
  */
void AT_SendErrorResponse(AtCmdStatus_t status) {
    switch (status) {
        case AT_OK:
            AT_SendResponse("OK");
            break;
        case AT_ERROR:
            AT_SendResponse("ERROR");
            break;
        case AT_PARAM_ERROR:
            AT_SendResponse("ERROR:PARAM");
            break;
        case AT_UNKNOWN_CMD:
            AT_SendResponse("ERROR:UNKNOWN_CMD");
            break;
        case AT_EXECUTION_ERROR:
            AT_SendResponse("ERROR:EXECUTION");
            break;
        default:
            AT_SendResponse("ERROR:UNKNOWN");
            break;
    }
}


// --- 具体命令实现 ---

// AT+RUN=<ID>,<Dir>,<Speed>
static AtCmdStatus_t Process_Run(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id, dir, speed;
    
    // 使用 sscanf 解析参数
    if (sscanf(params, "%d,%d,%d", &id, &dir, &speed) == 3) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        if (speed < 0 || speed > 1000) return AT_PARAM_ERROR;
        
        // 调用业务层
        App_Linkage_SetMode(0,1); // 确保退出联动模式
        App_Motor_MoveManual(id - 1, dir, speed); // ID-1 转为索引
        
        AT_SendResponse("+RUN:OK ID=%d,Dir=%d,Spd=%d", id, dir, speed);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+TIME=<ID>,<Dir>,<Speed>,<Ms>
static AtCmdStatus_t Process_Time(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id, dir, speed, time;
    
    if (sscanf(params, "%d,%d,%d,%d", &id, &dir, &speed, &time) == 4) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        
        App_Linkage_SetMode(0,1);
        App_Motor_MoveTime(id - 1, dir, speed, time);
        
        AT_SendResponse("+TIME:OK ID=%d,Time=%dms", id, time);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+POS=<ID>,<Dir>,<Speed>,<Pulses>
static AtCmdStatus_t Process_Pos(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id, dir, speed, pulses;
    
    if (sscanf(params, "%d,%d,%d,%d", &id, &dir, &speed, &pulses) == 4) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        
        App_Linkage_SetMode(0,1);
        App_Motor_MovePos(id - 1, dir, speed, pulses);
        
        AT_SendResponse("+POS:OK ID=%d,Target=%d", id, pulses);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+STOP=<ID>  (ID=0 全停)
static AtCmdStatus_t Process_Stop(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id;
    
    if (sscanf(params, "%d", &id) == 1) {
        App_Linkage_SetMode(0,1); // 停止联动
        
        if (id == 0) {
            for(int i=0; i<MAX_MOTORS; i++) App_Motor_Stop(i);
            AT_SendResponse("+STOP:OK ALL");
        } else {
            if (id > MAX_MOTORS) return AT_PARAM_ERROR;
            App_Motor_Stop(id - 1);
            AT_SendResponse("+STOP:OK ID=%d", id);
        }
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+QUERY=<ID>
static AtCmdStatus_t Process_Query(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id;
    
    if (sscanf(params, "%d", &id) == 1) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        
        // 从BSP层获取实时数据
        int32_t pulses = BSP_BLDC_GetPulse(id - 1);
        uint8_t is_busy = App_Motor_IsBusy(id - 1);
        
        // 格式: +STATUS:ID=<id>,Busy=<0/1>,Pulses=<val>
        AT_SendResponse("+STATUS:ID=%d,Busy=%d,Pulses=%ld", id, is_busy, pulses);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+LINK=<Mode>
static AtCmdStatus_t Process_Link(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int mode;
	int loop_count = 0; // 默认为0 (无限循环或单次，取决于你的定义，这里建议0表示无限)
    
    // 尝试解析两个参数
    int args_parsed = sscanf(params, "%d,%d", &mode, &loop_count);
    
    if (args_parsed >= 1) {
        // 如果只输入了 AT+LINK=1，则 args_parsed=1，loop_count 保持默认 0 (无限)
        // 或者你可以定义默认值为 1 (只跑一次)
        if (args_parsed == 1) loop_count = 1; 
        
        App_Linkage_SetMode(mode, loop_count);
        
        if (loop_count == 0) {
            AT_SendResponse("+LINK:OK Mode=%d,Loop=Infinite", mode);
        } else {
            AT_SendResponse("+LINK:OK Mode=%d,Loop=%d", mode, loop_count);
        }
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+ADCMOVE=<ID>,<Speed>,<TargetADC>,<Tolerance>,<Range>
static AtCmdStatus_t Process_AdcMove(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id, speed, target, tol, range;
    
    if (sscanf(params, "%d,%d,%d,%d,%d", &id, &speed, &target, &tol, &range) == 5) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        
        App_Linkage_SetMode(0,1);
        App_Motor_MoveAdcPos(id - 1, speed, (uint16_t)target, (uint16_t)tol, (uint16_t)range);
        
        AT_SendResponse("+ADCMOVE:OK ID=%d,Tgt=%d", id, target);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+ADCMOVELIM=<ID>,<Speed>,<TargetADC>,<Tolerance>,<Range>
static AtCmdStatus_t Process_AdcMoveLim(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id, speed, target, tol, range;
    
    if (sscanf(params, "%d,%d,%d,%d,%d", &id, &speed, &target, &tol, &range) == 5) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        
        App_Linkage_SetMode(0,1);
        App_Motor_MoveAdcPosWithLimit(id - 1, speed, (uint16_t)target, (uint16_t)tol, (uint16_t)range);
        
        AT_SendResponse("+ADCMOVELIM:OK ID=%d,Tgt=%d", id, target);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+CFGDECEL=<ID>,<Pulses>,<MinSpd>
static AtCmdStatus_t Process_CfgDecel(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id, pulses, min_spd;
    
    if (sscanf(params, "%d,%d,%d", &id, &pulses, &min_spd) == 3) {
        if (id < 1 || id > MAX_MOTORS) return AT_PARAM_ERROR;
        
        App_Motor_ConfigDecel(id - 1, pulses, min_spd);
        
        AT_SendResponse("+CFGDECEL:OK ID=%d", id);
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}

// AT+GETADC=<ID>  (ID=0 表示获取所有/全局监测数据, ID>0 获取特定电机数据)
static AtCmdStatus_t Process_GetAdc(char *params) {
    if (!params) return AT_PARAM_ERROR;
    int id;
    
    if (sscanf(params, "%d", &id) == 1) {
        if (id == 0) {
            // 返回全局信息: 电池电压, NTC温度, 错误码
             // Volt保留1位小数
             int v_int = (int)(g_adc_data.voltage_V * 10);
             int t_int = (int)(g_adc_data.temperature_C * 10);
             
             AT_SendResponse("+GETADC:Global V=%d.%01dV,T=%d.%01dC,Err=%d", 
                             v_int/10, abs(v_int%10), 
                             t_int/10, abs(t_int%10), 
                             g_adc_data.error_code);
        } else {
             if (id > MAX_MOTORS) return AT_PARAM_ERROR;
             int motor_idx = id - 1;
             
             // 返回电机相关ADC: 电流, 位置
             int i_int = (int)(g_adc_data.current_A[motor_idx] * 100); // 两位小数
             uint16_t pos = g_adc_data.position[motor_idx];
             
             AT_SendResponse("+GETADC:ID=%d,Cur=%d.%02dA,Pos=%d", 
                             id, 
                             i_int/100, abs(i_int%100), 
                             pos);
        }
        return AT_OK;
    }
    return AT_PARAM_ERROR;
}
