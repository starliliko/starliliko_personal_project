# STM32F407 双通道简易示波器

这是一个基于 STM32F407 的嵌入式双通道示波器课程项目。工程使用
STM32CubeMX/HAL 完成底层配置，以 FreeRTOS 组织采集、分析和显示任务，
通过 LVGL 绘制波形，并使用 CMSIS-DSP 完成信号参数计算和频谱分析。

> 项目为课程设计原型，适合用于学习 ADC 定时采样、DMA 双缓冲、
> RTOS 任务协作和嵌入式图形界面，不应作为经过校准的测量仪器使用。

## 主要功能

- 双通道 ADC 采样
- TIM2 触发采样，最高目标采样率 2 MHz
- DMA 循环双缓冲采集
- 波形触发与稳定显示
- 最大值、最小值、平均值、有效值、频率、周期和占空比计算
- 基于 CMSIS-DSP FFT 的频率分析和波形类型识别
- LVGL 触控界面

## 硬件与软件

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F407 |
| 工程生成 | STM32CubeMX（`oscilloscope.ioc`） |
| IDE/工具链 | Keil MDK-ARM |
| 操作系统 | FreeRTOS / CMSIS-RTOS2 |
| 图形库 | LVGL |
| 数学库 | CMSIS-DSP |
| 外设 | ADC、DMA、TIM2、FSMC、LCD、触摸屏、外部 SRAM |

## 工作流程

```text
TIM2 定时触发
      ↓
双通道 ADC + DMA 循环双缓冲
      ↓
ADC 采集任务（通道拆分）
      ↓
波形任务（电压换算、触发、测量、FFT）
      ↓
LVGL 任务（波形与参数显示）
```

## 快速开始

1. 安装 Keil MDK-ARM，并确保已安装 STM32F4 Device Pack。
2. 使用 Keil 打开 `MDK-ARM/oscilloscope.uvprojx`。
3. 检查目标芯片、调试器和下载算法是否与实际开发板一致。
4. 编译并下载到开发板。
5. 如需修改引脚、时钟或外设配置，使用 STM32CubeMX 打开
   `oscilloscope.ioc`；重新生成代码前请先提交或备份自定义代码。

## 目录说明

| 路径 | 内容 |
| --- | --- |
| `APP/` | ADC、波形处理和 LVGL 的 RTOS 任务 |
| `GUI/` | 示波器界面与交互逻辑 |
| `BSP/` | LCD、触摸、SRAM、I²C、按键等板级驱动 |
| `Core/` | CubeMX 生成的初始化代码和中断入口 |
| `SYSTEM/` | 延时与系统辅助代码 |
| `Drivers/` | STM32 HAL 和 CMSIS |
| `Middlewares/` | FreeRTOS、LVGL、CMSIS-DSP 及内存管理 |
| `MDK-ARM/` | Keil 工程文件 |
| `docs/` | 项目文档和课程报告整理稿 |
| `readme/` | 参考教程 |

更详细的模块边界见 [`docs/PROJECT_STRUCTURE.md`](docs/PROJECT_STRUCTURE.md)。

## 已知限制

- 测量结果依赖输入调理电路、参考电压和校准参数。
- 当前频率与波形识别算法包含面向实验环境的经验参数，换用信号源或前端
  电路后需要重新验证。
- 源码中的部分中文注释存在历史编码问题，不影响编译，但建议后续统一为
  UTF-8。

## 文档

- [`docs/contest_report/06_combined_report.md`](docs/contest_report/06_combined_report.md)：
  已移除个人身份信息的课程设计报告 Markdown 整理稿
- [`docs/contest_report/`](docs/contest_report/)：摘要、正文、测试记录与答辩提纲
- `readme/安富莱_STM32-V6开发板_二代示波器设计教程（V1.0）.pdf`：
  参考资料

## 许可说明

仓库目前未声明统一的开源许可证。STM32 HAL、CMSIS、FreeRTOS 和 LVGL
等第三方代码分别遵循其目录内附带的许可证；原创代码在补充许可证前默认
保留所有权利。
