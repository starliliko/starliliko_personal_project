# 项目结构与数据流

## 核心模块

### 采集层：`APP/adc_task.*`

- 由 TIM2 更新事件触发 ADC 转换。
- ADC 使用 DMA 循环模式写入双缓冲区。
- DMA 半满和全满回调切换已完成缓冲区，并通过信号量通知采集任务。
- 采集任务把交错排列的双通道数据拆分到独立通道缓冲区。

### 分析层：`APP/waveform_task.*`

- 将 12 位 ADC 原始值换算为电压。
- 根据触发模式、触发电平和边沿寻找显示起点。
- 计算最大值、最小值、平均值、有效值、频率、周期和占空比。
- 使用 CMSIS-DSP FFT 辅助频率测量和波形类型识别。
- 通过共享的 `waveform_data_t` 向显示层提供结果。

### 显示层：`APP/lvgl_task.*`、`GUI/oscilloscope.*`

- 初始化 LVGL、LCD 和触摸输入。
- 周期性刷新波形与测量参数。
- 处理通道、时基、垂直档位和触发设置等界面交互。

### 平台层

- `Core/`：CubeMX 生成的时钟、GPIO、ADC、DMA、TIM、FSMC 和 RTOS 配置。
- `BSP/`：开发板 LCD、触摸、SRAM、按键和 I²C 驱动。
- `SYSTEM/`：系统级辅助代码。
- `Drivers/`、`Middlewares/`：厂商驱动及第三方依赖。

## 运行时数据流

```text
TIM2 TRGO
   │
   ▼
ADC1 双通道扫描
   │
   ▼
DMA 循环缓冲区
   │ 半满/全满回调
   ▼
adc_data_sem
   │
   ▼
ADC_acquisition_task
   │ 通道拆分
   ▼
waveform_processing_task
   │ 电压换算 / 触发 / 测量 / FFT
   ▼
g_waveform_data
   │
   ▼
LVGL 界面刷新
```

## 修改指南

- 修改引脚、时钟或外设：编辑 `oscilloscope.ioc`，再由 CubeMX 生成。
- 修改采样率或 DMA 行为：优先检查 `APP/adc_task.*`、`Core/Src/adc.c`、
  `Core/Src/tim.c` 和 `Core/Src/dma.c`。
- 修改测量算法：编辑 `APP/waveform_task.*`，同时核对缓冲区长度和实际
  采样率。
- 修改界面：编辑 `GUI/oscilloscope.*`；LVGL 系统任务位于
  `APP/lvgl_task.*`。
- 修改板卡适配：集中在 `BSP/`，避免把板级细节继续扩散到应用层。

## 仓库维护建议

1. 将源代码、工程配置、必要资源和项目文档纳入版本控制。
2. 不提交编译输出、日志、用户级 Keil 配置或编辑器本地设置。
3. 对 CubeMX 自动生成代码和手写代码分开提交，便于审查差异。
4. 硬件相关变更应在提交说明中记录开发板版本、引脚和测试信号条件。
5. 后续若大幅精简 `Drivers/` 或 `Middlewares/`，先验证 Keil 工程引用，
   避免删除当前工程直接包含的第三方源文件。
