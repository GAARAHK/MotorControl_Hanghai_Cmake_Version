#include "app_linkage.h"
#include "app_motor.h"
#include "app_adc.h" // 引用ADC数据



// --- 定义 ---
typedef enum {
    SM_IDLE,
    SM_INIT,    // 初始化/准备步骤
    SM_STEP_1,
    SM_STEP_2,
    SM_STEP_3,
    SM_STEP_4,
	SM_STEP_5,
	SM_STEP_6,
	SM_STEP_7,
	//可增加步骤
    SM_WAIT_FINISH // 等待该轮最后动作��成
} LinkState_t;




// --- 变量 ---
static uint8_t current_mode = 0;
static uint32_t target_loops = 0;     // 目标总次数 (0=无限)
static uint32_t current_loop_cnt = 0; // 当前已执行次数
static LinkState_t sm_state = SM_IDLE;
static uint32_t sm_timer = 0;

// --- 函数声明 ---不同的运行逻辑
static void Logic_Mode_1(void);
static void Logic_Mode_2(void);
static void Logic_Mode_3(void);
static void Logic_Mode_4(void);
static void Logic_Mode_5(void); // Mode 5 declaration
static void Logic_Mode_6(void); // Mode 6 declaration

// --- 接口实现 ---

void App_Linkage_Init(void) {
    current_mode = LINK_MODE_IDLE;
}



void App_Linkage_SetMode(uint8_t mode_id, uint32_t loop_count) {
    current_mode = mode_id;
    target_loops = loop_count;
    current_loop_cnt = 0;
    
    if (mode_id == 0) {
        // 紧急停止所有
        App_Motor_Stop(0); 
        // App_Motor_Stop(1); ...
		
		
        sm_state = SM_IDLE;
    } else {
        // 启动新任务
        sm_state = SM_INIT; 
    }
}

// 统一的状态机调度器
void App_Linkage_Process(void) {
    if (current_mode == 0) return;

    switch (current_mode) {
        case 1: Logic_Mode_1(); break;
        case 2: Logic_Mode_2(); break;
		case 3: Logic_Mode_3(); break;
        case 4: Logic_Mode_4(); break;
        case 5: Logic_Mode_5(); break; // Mode 5 case
        case 6: Logic_Mode_6(); break; // Mode 6 case (Analog Reciprocating)
        // Case 3, 4...
        default:  
            current_mode = 0; // 未知模式，自动停止
            break;
    }
}

// --- 辅助函数：处理循环计数 ---
// 在每一轮逻辑结束时调用此函数
// 返回 1 表示继续下一轮，返回 0 表示全部完成需停止
static uint8_t Check_Loop_Finish(void) {
    current_loop_cnt++;
    
    // 如果 target_loops 为 0，表示无限循环，始终返回 1
    if (target_loops == 0) return 1;
    
    // 检查是否达到目标次数
    if (current_loop_cnt >= target_loops) {
        current_mode = 0; // 停止联动
        sm_state = SM_IDLE;
        // 可选：在这里调用 AT_SendResponse 发送 "+LINK:DONE"
        return 0; 
    }
    
    // 未达到，重置状态继续
    return 1;
}

// ============================================================
// 下面是具体的联动逻辑实现区域
// ============================================================

// 模式1: 简单的往复运动
static void Logic_Mode_1(void) {
    switch (sm_state) {
        case SM_INIT:
            // 可以在这里做一些复位操作
            sm_state = SM_STEP_1;
            break;

        case SM_STEP_1:
            // 动作：电机1 正转 2秒
            if (!App_Motor_IsBusy(0)) {
                App_Motor_MoveTime(0, 0, 800, 2000);
                sm_state = SM_STEP_2;
            }
            break;
            
        case SM_STEP_2:
            // 等待动作完成
            if (!App_Motor_IsBusy(0)) {
                sm_timer = HAL_GetTick(); // 停顿计时开始
                sm_state = SM_STEP_3;
            }
            break;
            
        case SM_STEP_3:
            // 停顿 500ms 后反转
            if (HAL_GetTick() - sm_timer > 500) {
                App_Motor_MoveTime(0, 1, 800, 2000); // 反转 2秒
                sm_state = SM_WAIT_FINISH;
            }
            break;

        case SM_WAIT_FINISH:
            // 等待最后一步完成
            if (!App_Motor_IsBusy(0)) {
                // === 一轮结束，检查循环 ===
                if (Check_Loop_Finish()) {
                    sm_state = SM_STEP_1; // 回到第一步，开始���一轮
                }
            }
            break;
            
        default:
            sm_state = SM_IDLE;
            break;
    }
}


// 模式2: 另一种逻辑 (例如基于位置)
static void Logic_Mode_2(void) {
    switch (sm_state) {
        case SM_INIT:
            sm_state = SM_STEP_1;
            break;

        case SM_STEP_1:
            // 动作：电机1 跑 5000 脉冲
            if (!App_Motor_IsBusy(0)) {
                App_Motor_MovePos(0, 0, 600, 5000);
                sm_state = SM_WAIT_FINISH;
            }
            break;
            
        case SM_WAIT_FINISH:
            if (!App_Motor_IsBusy(0)) {
                // === 一轮结束 ===
                if (Check_Loop_Finish()) {
                    // 模式2可能只需要单向跑，下一轮继续跑
                    sm_state = SM_STEP_1; 
                }
            }
            break;
            
        default:
            sm_state = SM_IDLE;
            break;
    }
}
   
/**
 * @brief 逻辑3: 多段速定位 (位置控制)
 * 流程: M1慢速走到1000脉冲 -> 快速走到5000脉冲 -> 回到0
 */
static void Logic_Mode_3(void) {
    switch (sm_state) {
		
		case SM_INIT:
            sm_state = SM_STEP_1;
            break;
		
        case SM_STEP_1: // 第一段: 慢速
            // ID=0, Dir=CW, Speed=300, Target=1000 pulses
            App_Motor_MovePos(0, 0, 200, 1000);
            sm_state = SM_STEP_2;
            break;
            
        case SM_STEP_2: // 等待
            if (!App_Motor_IsBusy(0)) sm_state = SM_STEP_3;
            break;
            
        case SM_STEP_3: // 第二段: 快速追击
            // ID=0, Dir=CW, Speed=800, Target=4000 pulses (累加)
            App_Motor_MovePos(0, 0, 800, 4000);
            sm_state = SM_STEP_4;
            break;
            
        case SM_STEP_4: // 等待
            if (!App_Motor_IsBusy(0)) {
                sm_timer = HAL_GetTick();
                sm_state = SM_STEP_5;
            }
            break;
            
        case SM_STEP_5: // 停顿500ms后回零
            if (HAL_GetTick() - sm_timer > 1000) {
                // ID=0, Dir=CCW, Speed=1000, Target=5000 pulses (假设回原点)
                App_Motor_MovePos(0, 1, 1000, 5000);
                sm_state = SM_WAIT_FINISH;
            }
            break;
            
        case SM_WAIT_FINISH:
            // 等待最后一步完成
            if (!App_Motor_IsBusy(0)) {
                // === 一轮结束，检查循环 ===
                if (Check_Loop_Finish()) {
                    sm_state = SM_STEP_1; // 回到第一步，开始���一轮
                }
            }
            break;
            
        default:
            sm_state = SM_IDLE;
            break;
    }
}


/**
 * @brief 逻辑4: 带限位保护的多电机顺序动作
 * 场景: M1 正转 -> 触限位/到位 -> M2 正转 -> 触限位/到位 -> 停顿 -> M2 反转 -> M1 反转
 */
static void Logic_Mode_4(void) {
    switch (sm_state) {
        
        case SM_INIT:
            sm_state = SM_STEP_1;
            break;
        
        case SM_STEP_1: // 第1步: 电机1 正转 (带限位保护)
            // ID=0, CW, Speed=500, Max=5000 pulses
            // 只要碰到配置好的限位开关1，或者跑满了5000脉冲，电机就会停
            App_Motor_MovePosWithLimit(0, 0, 500, 5000);
            sm_state = SM_STEP_2;
            break;
            
        case SM_STEP_2: // 等待 M1 停止
            if (!App_Motor_IsBusy(0)) {
                sm_state = SM_STEP_3;
            }
            break;
            
        case SM_STEP_3: // 第2步: 电机2 正转 (带限位保护)
            // 假设我们有第二个电机 (ID=1)。如果是单电机测试，这里可以复用 ID=0 模拟效果
            // App_Motor_MovePosWithLimit(1, 0, 500, 4000);
            
            // 为了演示，这里继续用 ID=0 模拟第二个动作，您可以改成 ID=1
            App_Motor_MovePosWithLimit(0, 0, 600, 4000); 
            sm_state = SM_STEP_4;
            break;
            
        case SM_STEP_4: // 等待 M2 停止 & 开始计时
            if (!App_Motor_IsBusy(0)) { // 检查对应电机是否停止
                sm_timer = HAL_GetTick();
                sm_state = SM_STEP_5;
            }
            break;
            
        case SM_STEP_5: // 第3步: 停顿 1秒
            if (HAL_GetTick() - sm_timer > 1000) {
                sm_state = SM_STEP_6;
            }
            break;
            
        case SM_STEP_6: // 第4步: 电机2 反转退回 (找反向限位)
            // ID=0(模拟M2), CCW, Speed=500, 回退足够远的距离以确保碰到开关
            App_Motor_MovePosWithLimit(0, 1, 500, 10000); 
            sm_state = SM_STEP_7;
            break;
            
        case SM_STEP_7: // 等待 M2 退回完成
            if (!App_Motor_IsBusy(0)) {
                sm_state = SM_WAIT_FINISH; // 这里暂时只演示到这里，可以继续添加 M1 退回逻辑
            }
            break;
            
        // ... 可以继续添加 STEP_8 (M1 反转) ...

        case SM_WAIT_FINISH:
            if (!App_Motor_IsBusy(0)) { // 检查所有相关电机
                if (Check_Loop_Finish()) {
                    sm_state = SM_STEP_1;
                }
            }
            break;
            
        default:
            sm_state = SM_IDLE;
            break;
    }
}

/**
 * @brief 逻辑5: 基于ADC绝对位置的多电机联动 (Mode 5)
 * 流程: M1去位置A -> 到位 -> M1(或M2)去位置B -> 停顿 -> 回到起点
 */
static void Logic_Mode_5(void) {
    switch (sm_state) {
        
        case SM_INIT:
            sm_state = SM_STEP_1;
            break;
            
        case SM_STEP_1: // 第1步: 电机1 移动到 ADC=1000
            // ID=0, Speed=800, Target=1000, Tolerance=10, Range=200
            App_Motor_MoveAdcPosWithLimit(0, 800, 1000, 10, 200);
            sm_state = SM_STEP_2;
            break;
            
        case SM_STEP_2: // 等待 M1 到位
            if (!App_Motor_IsBusy(0)) {
                // 如果是多电机，这里可以启动电机2
                sm_state = SM_STEP_3;
            }
            break;
            
        case SM_STEP_3: // 第2步: 模拟电机2 (这里仍用ID0演示) 移动到 ADC=3000
             // App_Motor_MoveAdcPosWithLimit(1, ...);
            App_Motor_MoveAdcPosWithLimit(0, 600, 3000, 10, 200);
            sm_state = SM_STEP_4;
            break;

        case SM_STEP_4: // 等待 M2 到位 & 启动计时
            if (!App_Motor_IsBusy(0)) {
                sm_timer = HAL_GetTick();
                sm_state = SM_STEP_5;
            }
            break;
            
        case SM_STEP_5: // 第3步: 停顿 2秒
            if (HAL_GetTick() - sm_timer > 2000) {
                sm_state = SM_STEP_6;
            }
            break;
            
        case SM_STEP_6: // 第4步: 返回起点 (ADC=1000)
             // 假设同时或者顺序回 
            App_Motor_MoveAdcPosWithLimit(0, 800, 1000, 10, 200);
            sm_state = SM_WAIT_FINISH;
            break;
            
        case SM_WAIT_FINISH:
            // 检查完成情况
            if (!App_Motor_IsBusy(0)) {
                if (Check_Loop_Finish()) {
                    sm_state = SM_STEP_1; // 继续下一轮
                }
            }
            break;
            
        default:
            sm_state = SM_IDLE;
            break;
    }
}

/**
 * @brief 逻辑6: 模拟开关往复运动 (ADC Position)
 * Logic:
 *  - Default: Start CW
 *  - If ADC >= 3.3V (High Threshold) -> Stop & Switch to CCW
 *  - If ADC <= 0V (Low Threshold) -> Stop & Switch to CW
 */
static void Logic_Mode_6(void) {
    uint16_t adc_val = App_Adc_GetPos(0); 
    // Thresholds: 0-4095 scale. 
    // 0V ~ 100
    // 3.3V ~ 4000
    const uint16_t THRESHOLD_LOW = 100;
    const uint16_t THRESHOLD_HIGH = 4000;
    const uint16_t SPEED = 1000; // Speed 800

    switch (sm_state) {
        case SM_INIT:
            // 默认 CW
            sm_state = SM_STEP_1; 
            break;

        case SM_STEP_1: // Action: CW
            // Start moving CW indefinitely until stopped
             if (!App_Motor_IsBusy(0)) {
                // Manual Move continues until Stop is called
                App_Motor_MoveManual(0, 0, SPEED); // 0=CW
                sm_state = SM_STEP_2;
            }
            break;

        case SM_STEP_2: // Monitor CW
            // Check High Limit (3.3V)
            if (adc_val >= THRESHOLD_HIGH) {
                App_Motor_Stop(0);
                sm_timer = HAL_GetTick(); // Start delay
                sm_state = SM_STEP_3;
            }
            break;

        case SM_STEP_3: // Wait Delay before CCW
            if (HAL_GetTick() - sm_timer > 500) { 
                sm_state = SM_STEP_4;
            }
            break;
            
        case SM_STEP_4: // Action: CCW
             if (!App_Motor_IsBusy(0)) {
                 App_Motor_MoveManual(0, 1, SPEED); // 1=CCW
                 sm_state = SM_STEP_5;
             }
             break;

        case SM_STEP_5: // Monitor CCW
             // Check Low Limit (0V)
             if (adc_val <= THRESHOLD_LOW) {
                 App_Motor_Stop(0);
                 sm_timer = HAL_GetTick();
                 sm_state = SM_STEP_6;
             }
             break;

        case SM_STEP_6: // Wait Delay before CW (Cycle complete)
             if (HAL_GetTick() - sm_timer > 500) {
                 // Check Loop Limit (1 cycle = CW + CCW)
                 if (Check_Loop_Finish()) {
                     sm_state = SM_STEP_1; // Loop back to CW
                 }
             }
             break;

        default:
            sm_state = SM_IDLE;
            break;
    }
}


