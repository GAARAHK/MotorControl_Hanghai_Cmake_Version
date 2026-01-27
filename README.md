# NoBrush_Control_LIN (STM32F103)

## 1. 项目简介
本项目基于STM32F103C8T6芯片，实现无刷直流电机（BLDC）的智能控制系统。项目集成了多模式运动控制、ADC高频采样与保护、LIN/串口通信以及多电机联动逻辑功能。采用VS Code + CMake + STM32CubeMX开发流程。

## 2. 核心功能

### 2.1 运动控制 (App/Motor)
*   **PWM 调速**: 基于TIM4 PWM输出，支持0-1000占空比调节。
*   **三种运动模式**:
    *   **时间模式**: 按指定速度运行指定时间。
    *   **相对位置模式 (FG脉冲)**: 运行指定脉冲数，支持梯形预减速。
    *   **绝对位置模式 (ADC)**: 运行到指定传感器(电位器)ADC值，支持动态方向判断与减速。
*   **限位保护**: 支持为每个电机配置正/反转限位开关，硬件直接保护。
*   **联动控制**: 内置状态机，支持多段速、往复运动、多机顺序动作等复杂逻辑。

### 2.2 数据采集与保护 (App/ADC)
*   **高频采样**: 10kHz (100us) 采样率，TIM3触发 + DMA自动搬运。
*   **采集对象**: 母线电压、驱动电流、板载NTC温度、绝对位置传感器。
*   **实时保护**:
    *   **过流保护 (OC)**: 100us级响应，触发即停机。
    *   **过压/欠压/过温 (OV/UV/OT)**: 10ms级响应。

### 2.3 调试与通信
*   **日志系统**: 自定义串口日志打印 (USART1)。
*   **AT指令**: 支持通过串口下发指令控制电机（待扩展）。

## 3. 硬件资源配置

| 功能 | 资源 | 引脚 | 说明 |
| :--- | :--- | :--- | :--- |
| **电机1 PWM** | TIM4_CH1 | PB6 | 驱动板 PWM 输入 |
| **电机1 DIR** | GPIO | PB9 | 方向控制 |
| **电机1 BRK** | GPIO | PB7 | 刹车控制 |
| **电机1 FG** | GPIO/EXTI | PB8 | 速度脉冲反馈 (外部中断) |
| **ADC 电流** | ADC1_IN2 | PA2 | 电流采样 |
| **ADC 温度** | ADC1_IN3 | PA3 | NTC 热敏电阻 |
| **ADC 电压** | ADC1_IN5 | PA5 | 母线电压分压 |
| **ADC 位置** | ADC1_IN7 | PA7 | 角度传感器/电位器 |
| **LED 指示** | GPIO | PB12 | 1Hz 心跳闪烁 |
| **调试串口** | USART1 | PA9/10 | Log 输出 / AT 指令 |
| **LIN 通信** | USART3 | PB10/11 | TX/RX (波特率 19200) |
| **LIN 休眠** | GPIO | PA4 | TJA1021 SLP_N 控制 |

## 4. 快速使用指南

### 4.1 详细初始化流程
为了确保系统功能正常，请按以下顺序进行初始化配置。推荐代码段放在 `main.c` 的 `USER CODE BEGIN 2` 区域。

#### 第一步：基础初始化
```c
// 1. 开启关键中断 (TIM3用于系统Tick和采样)
HAL_TIM_Base_Start_IT(&htim3);

// 2. 调用应用层总入口 (包含电机、ADC、串口、Log的初始化)
App_Init(); 

// 3. 初始化 LIN 总线 (配置收发器与中断)
App_LIN_Init();
```

#### 第二步：运行参数配置
在进入主循环前，建议配置电机运行参数：
```c
// [可选] 配置减速曲线: 电机0, 最后2500脉冲开始减速, 最低PWM=100
App_Motor_ConfigDecel(0, 2500, 100);

// [必须] 配置限位开关 (如果需要限位保护)
// 参1:电机ID, 参2/3:正转限位端口/引脚, 参4/5:反转限位, 参6:触发电平
App_Motor_ConfigLimit(0, GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
```

#### 第三步：保护阈值配置
```c
// [推荐] 设置全局电压/温度保护
// 低压9V, 高压28V, 过温85℃
App_Adc_ConfigProtect_Global(9.0f, 28.0f, 85.0f);

// [推荐] 设置电机过流保护
// 电机0最大电流 5.0A
App_Adc_ConfigProtect_Motor(0, 5.0f);
```

### 4.2 主循环调用
在 `main.c` 的 `while(1)` 循环中，**必须** 保持调用 `App_Loop()` 以维持 AT指令响应、电机状态机切换和联动逻辑。
```c
while (1)
{
    App_Loop();
}
```

### 4.3 控制电机运行
**场景A：绝对位置控制 (例如舵机模式)**
```c
// 电机0, 最大PWM 800, 移动到 ADC值 2048, 误差允许±10, 距离500开始减速
App_Motor_MoveAdcPos(0, 800, 2048, 10, 500);
```

**场景B：相对脉冲控制 (例如开窗器)**
```c
// 1. 配置限位开关 (可选)
App_Motor_ConfigLimit(0, GPIOA, GPIO_PIN_0, GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
// 2. 运行 5000 脉冲，带限位保护
App_Motor_MovePosWithLimit(0, MOTOR_DIR_CW, 1000, 5000);
```

### 4.4 AT指令手册
通过串口 (115200, 8N1) 发送以下ASCII指令。行尾需加 `\r\n`。

| 指令 | 格式 | 示例 | 描述 |
| :--- | :--- | :--- | :--- |
| **电机启停** | `AT+RUN=<ID>,<Dir>,<Spd>` | `AT+RUN=1,1,500` | 手动持续运行 |
| **电机停止** | `AT+STOP=<ID>` | `AT+STOP=0` | 停止指定电机 (0=全停) |
| **时间运行** | `AT+TIME=<ID>,<Dir>,<Spd>,<Ms>` | `AT+TIME=1,1,500,1000` | 运行指定时间 |
| **相对位置** | `AT+POS=<ID>,<Dir>,<Spd>,<Pulses>` | `AT+POS=1,1,800,2000` | 运行指定脉冲 |
| **绝对位置** | `AT+ADCMOVE=<ID>,<Spd>,<Tgt>,<Tol>,<Rng>` | `AT+ADCMOVE=1,800,2048,10,300` | 闭环运行至ADC值 |
| **带限位绝对**| `AT+ADCMOVELIM=<ID>,<Spd>,<Tgt>,<Tol>,<Rng>` | `AT+ADCMOVELIM=1,800,2048,10,300`| 同上，且检测限位开关 |
| **配置减速** | `AT+CFGDECEL=<ID>,<Pulses>,<MinSpd>` | `AT+CFGDECEL=1,100,200` | 配置相对位置模式减速参数 |
| **查询传感器**| `AT+GETADC=<ID>` | `AT+GETADC=0` | ID=0返回电压/温度/异常，ID=n返回电流/位置 |
| **查询状态** | `AT+QUERY=<ID>` | `AT+QUERY=1` | 获取运行状态与脉冲计数 |
| **联动控制** | `AT+LINK=<Mode>,[Loop]` | `AT+LINK=4,1` | 启动联动模式 (Mode=4, Loop=1次) |

*   **ID**: 1~N (电机编号)
*   **Dir**: 0=CCW, 1=CW
*   **Spd**: 0~1000 (PWM占空比)
*   **Ms**: 运行毫秒数

## 5. LIN 通信协议

本项目通过 USART3 (PB10/PB11) + TJA1021 收发器实现 LIN Slave 功能。
*   **波特率**: 19200 bps
*   **协议版本**: LIN 2.x (Enhanced Checksum) / LIN 1.x (Classic Checksum) 兼容

### 5.1 帧结构
标准 LIN 帧包括：**Break** (显性>=13bit) + **Sync** (0x55) + **PID** (ID+Parity) + **Data** (8 Bytes) + **Checksum**。

**PID 计算**: 
*   `P0 = ID0 ^ ID1 ^ ID2 ^ ID4`
*   `P1 = ~(ID1 ^ ID3 ^ ID4 ^ ID5)`
*   `PID = (P1 << 7) | (P0 << 6) | ID`

### 5.2 支持指令表

假设目标电机 ID=1。

| 功能 | ID (Hex) | PID (Hex) | Data 0 | Data 1 | Data 2 | Data 3 | Data 4 | Data 5 | Data 6 | Data 7 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **运行/停止** | **0x30** | **0xF0** | Cmd* | MotorID | Dir | SpdH | SpdL | TimeH | TimeL | Res |
| **位置控制** | **0x31** | **0xB1** | MotorID | Dir | SpdH | SpdL | Pos3 | Pos2 | Pos1 | Pos0 |
| **ADC控制** | **0x32** | **0x32** | MotorID | SpdH | SpdL | AdcH | AdcL | Tol | RngH | RngL |

*   **Cmd**: 1=Run, 2=Stop, 3=Time
*   **Dir**: 0=CCW, 1=CW
*   **Pos**: 4字节脉冲数 (Big Endian)

### 5.3 发送示例 (Hex)
*   **电机1 以1000速度正转**: `55 F0 01 01 01 03 E8 00 00 00 20`
*   **电机1 停止**: `55 F0 02 01 00 00 00 00 00 00 0C`
*   **电机1 走10000脉冲**: `55 B1 01 00 07 D0 00 00 27 10 3E`
*(注: 实际发送前需先发送 Break 信号)*

## 6. 项目扩展指南

### 6.1 增加第2个电机
1.  **CubeMX配置**:
    *   新增PWM通道、DIR/BRK/FG 引脚。
    *   新增ADC通道用于电流和位置采样 (如有)。
2.  **代码修改 (BSP/Inc/bsp_bldc.h)**:
    *   修改 `#define MAX_MOTORS 2`。
3.  **IO配置 (BSP/Src/bsp_bldc.c)**:
    *   在 `BSP_BLDC_Init` 中添加 `motors[1]` 的引脚配置。
4.  **ADC配置 (App/Src/app_adc.c)**:
    *   在 `App_Adc_Process` 中添加新通道的数据读取映射。

### 6.2 链接脚本 (Linker Script) 注意
本项目使用了修复版的链接脚本 `STM32F103C8Tx_FLASH_fixed.ld` 以解决 CubeMX 生成的 GCC 脚本 Bug。
*   **不要删除** 该文件。
*   如果修改了芯片型号或堆栈大小，请手动同步修改该文件。

---
**版本**: 1.0.0
**日期**: 2026/01/23
