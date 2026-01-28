# 移植与跨平台开发指南

本文档详细说明了如何将 `MotorControl_LIN_Hanghai` 项目移植到不同的硬件平台（如其他 STM32 系列或非 STM32 芯片），以及修改关键硬件配置的步骤。

---

## 1. 目录结构与架构
项目采用分层架构，移植工作主要集中在底层 (Drivers/BSP) 和 配置层 (Config)。

*   **App/**: 纯业务逻辑 (电机控制算法、状态机、AT指令解析)。**无需修改**。
*   **Middleware/**: 协议栈 (LIN, Log)。**基本无需修改** (仅依赖 `BSP_Config.h`)。
*   **BSP/**: 板级支持包 (GPIO初始化,定时器映射)。**移植重点**。
*   **Core/**: STM32CubeMX 生成的底层代码。**移植重点**。

---

## 2. STM32 系列间移植 (例如 F1 -> F4/G4/H7)

得益于 STM32 HAL 库的统一性，系列间移植最为简单。

### 步骤 1: 使用 STM32CubeMX 生成新工程
1.  选择目标芯片 (如 STM32F407)。
2.  配置外设 (保持与原工程一致的功能):
    *   **TIM PWM**: 用于电机驱动 (原 TIM4)。
    *   **TIM Base**: 用于 10kHz 心跳 (原 TIM3)。
    *   **UART**: 用于 AT 指令 (原 USART1) 和 LIN 总线 (原 USART3)。
    *   **ADC + DMA**: 用于电流/电压/位置采样。
3.  生成代码，覆盖 `Core/` 目录。

### 步骤 2: 修改 `BSP/Inc/bsp_conf.h`
这是解耦的核心文件，将新工程的句柄映射到业务层。

```c
// 修改为您新工程中生成的句柄变量名
#define MOTOR_TIM_HANDLE        htim1  // 例如 F4 改用 TIM1
#define BASE_TIM_HANDLE         htim2  // 例如改用 TIM2
#define LOG_UART_HANDLE         huart1
#define LIN_UART_HANDLE         huart2 // 例如改用 USART2
#define ADC_HANDLE              hadc1
#define DMA_ADC_HANDLE          hdma_adc1
```

### 步骤 3: 适配 Flash 存储 (`App/Src/app_storage.c`)
不同系列的 Flash 扇区结构不同。
*   **STM32F1**:按 Page (1KB/2KB) 擦除。
*   **STM32F4**:按 Sector (16KB~128KB) 擦除。

**修改方法**:
在 `app_storage.c` 中，重写 `App_Storage_Save` 函数中的擦除和写入逻辑。
*   F1 使用 `FLASH_TYPEERASE_PAGES`
*   F4 使用 `FLASH_TYPEERASE_SECTORS`

### 步骤 4: 中断移植
将 `Core/Src/stm32f1xx_it.c` 中的自定义逻辑复制到新工程的 `stm32f4xx_it.c` (或其他系列) 中。
*   `USARTx_IRQHandler`: 添加 `AT_UART_IdleCallback` 和 `App_LIN_IRQHandler`。
*   `DMAx_Streamx_IRQHandler`: 确保 DMA 中断正常。

---

## 3. 跨厂商移植 (例如 STM32 -> GD32 / ESP32)

由于脱离了 HAL 库，需要替换底层驱动 API。

### 步骤 1: 替换 BSP 层实现
修改 `BSP/Src/bsp_bldc.c`，替换 HAL 库函数。

*   **PWM 控制**:
    *   原: `__HAL_TIM_SET_COMPARE(&MOTOR_TIM_HANDLE, ...)`
    *   新: `timer_channel_output_pulse_value_config(...)` (GD32 示例) 或 `ledcWrite(...)` (ESP32 示例)。
*   **GPIO 控制**:
    *   原: `HAL_GPIO_WritePin(...)`
    *   新: 目标平台的 GPIO API。

### 步骤 2: 替换系统时钟与延时
*   `App` 层可能使用了 `HAL_Delay()` 或 `HAL_GetTick()`。
*   需要在新平台实现一个返回毫秒级时间戳的函数，并宏定义替换：
    `#define HAL_GetTick()  millis()`

### 步骤 3: 串口驱动适配
`Middleware/Src/at_command.c` 和 `app_lin.c` 依赖串口发送和中断。
*   **发送**: 重写 `HAL_UART_Transmit` 宏或封装层。
*   **接收**: 许多非 STM32 平台没有空闲中断 (IDLE IRQ) + DMA 的组合。可能需要改为 "字节中断 + 缓冲区轮询" 的方式。

---

## 4. 常见的配置修改场景

### 场景 A: 修改电机 PWM 引脚
1.  在 CubeMX 中重新配置 PWM 引脚。
2.  在 `BSP/Inc/bsp_conf.h` 中更新 `MOTOR_TIM_HANDLE` (如果定时器变了)。
3.  在 `BSP/Src/bsp_bldc.c` `BSP_BLDC_Init` 中更新 Channel (如 `TIM_CHANNEL_1` -> `TIM_CHANNEL_2`)。

### 场景 B: 修改 LIN 串口为 UART2
1.  CubeMX 启用 UART2。
2.  `BSP/Inc/bsp_conf.h`: 修改 `#define LIN_UART_HANDLE huart2`。
3.  `Core/Src/stm32fxxxx_it.c`: 将 `App_LIN_IRQHandler()` 从 `USART3_IRQHandler` 移到 `USART2_IRQHandler`。

---

## 5. 编译环境迁移
如果从 GCC (VSCode/CMake) 迁移到 Keil MDK / IAR：
1.  **移除 CMakeLists.txt**。
2.  **添加源文件**: 将 `App`, `BSP`, `Middleware` 文件夹下的 `.c` 文件全部加入 IDE 的工程分组。
3.  **添加头文件路径**: 在 IDE 的 C/C++ 设置中添加 `App/Inc`, `BSP/Inc`, `Middleware/Inc`。
4.  **宏定义**: 确保定义了芯片型号宏 (如 `STM32F103xB`)。

