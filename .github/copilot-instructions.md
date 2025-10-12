# STM32F407 Digital Oscilloscope - AI Coding Instructions

## Project Overview
This is a digital oscilloscope implementation on STM32F407ZGT6 using FreeRTOS, LVGL GUI library, and ARM DSP library. The system performs real-time ADC sampling, signal processing, and waveform display with touch interface.

## Architecture & Core Components

### Task-Based Real-Time System
- **FreeRTOS CMSIS-OS2**: 4 main tasks with priority-based scheduling
- **ADCTask** (Realtime): High-speed ADC data acquisition using DMA circular buffer
- **Waveform_Task** (High): Signal processing and measurement calculations using ARM DSP
- **LVGL_Task** (High): GUI rendering and touch input handling  
- **LEDTask** (Normal): System status indication

### Hardware Abstraction Layers
- **BSP/**: Hardware drivers (LCD, Touch, SRAM, IIC, etc.) - use existing patterns
- **Core/**: STM32 HAL peripheral initialization (ADC, DMA, Timers)
- **SYSTEM/**: Custom delay and system utilities

### Data Flow Pipeline
```
ADC1+DMA → Double Buffer → Channel Separation → DSP Processing → GUI Display
          ↓                    ↓                    ↓              ↓
    (adc_task.c)      (waveform_task.c)    (oscilloscope.c)  (LVGL)
```

## Critical Development Patterns

### Memory Management
- **32-byte aligned buffers**: All ADC and processing buffers use `__attribute__((aligned(32)))`
- **Double buffering**: `adc_double_buffer[ADC_BUFFER_SIZE * 2]` for continuous DMA
- **External SRAM**: Available via FSMC for large data storage

### Inter-Task Communication
- **Message Queues**: `ADC_Raw_Data_Queue`, `Waveform_Feature_Queue` for data passing
- **Binary Semaphores**: `adc_data_sem` for buffer ready notifications
- **Volatile pointers**: `volatile uint16_t *filled_buffer_ptr` for ISR communication

### STM32CubeMX Integration
- **USER CODE sections**: Always place custom code within `/* USER CODE BEGIN */` blocks
- **Configuration via .ioc**: Hardware setup in `oscilloscope.ioc` file
- **Regeneration safe**: Code outside USER CODE blocks gets overwritten

## Essential File Relationships

### Core Configuration Chain
1. `oscilloscope.ioc` → STM32CubeMX project configuration
2. `Core/Src/main.c` → System init and task creation entry point
3. `Core/Src/freertos.c` → Task definitions and RTOS setup
4. `Core/Inc/FreeRTOSConfig.h` → RTOS tuning parameters

### Application Layer (`APP/`)
- `adc_task.c`: Timer-triggered ADC sampling with DMA circular buffer management
- `waveform_task.c`: ARM DSP-based signal analysis and measurement extraction  
- `lvgl_task.c`: GUI initialization and LVGL timer handler loop

### GUI Layer (`GUI/`)
- `oscilloscope.c`: Custom LVGL widgets for scope display and measurement tables

## Build & Debug Workflow

### Keil MDK-ARM Project
- **Project file**: `MDK-ARM/oscilloscope.uvprojx`
- **Target**: STM32F407ZGTx with ARM Compiler 5
- **Debug config**: ST-LINK debugger in `DebugConfig/`

### Memory Layout
- **Flash**: 1MB (0x8000000-0x80FFFFF)
- **Internal RAM**: 112KB (0x20000000-0x2001BFFF) + 16KB CCM (0x2001C000-0x2001FFFF)
- **External SRAM**: Connected via FSMC for waveform storage

## Signal Processing Specifics

### ADC Configuration
- **Dual channel**: ADC1 channels 1&2 for differential inputs
- **External trigger**: TIM2_TRGO for precise timing control
- **Sample rate**: Configurable via `ADC_Set_Sample_Rate()` up to 2MHz
- **DMA**: Circular mode with half/full transfer interrupts

### DSP Pipeline (ARM CMSIS-DSP)
```c
// Voltage conversion with DSP scaling
arm_scale_f32(volt_buf, ADC_TO_VOLT_SCALE, volt_buf, len);

// Frequency measurement using zero-crossing detection
static float calculate_frequency(int16_t *wave_buf, uint16_t buf_len);
```

## LVGL Integration Patterns

### Display Port
- Initialize via `lv_port_disp_init()` after `lv_init()`
- Canvas-based drawing for oscilloscope grid and waveforms
- Touch input via `lv_port_indev_init()`

### Custom Widgets
- Scope canvas with background grid drawing
- Real-time waveform overlay using LVGL draw functions
- Measurement table updates via `update_measurement_table()`

## Common Pitfalls & Solutions

### Memory Alignment
- Always use 32-byte alignment for DMA buffers and DSP operations
- FPU requires aligned access for optimal performance

### Task Synchronization  
- Use binary semaphores for buffer ready notifications from ISR to task
- Message queues for structured data passing between processing stages

### STM32CubeMX Regeneration
- Only modify code within USER CODE sections
- Custom initialization goes in `gobal_init()` function
- Hardware config changes require .ioc file updates

### Real-time Performance
- ADC task has REALTIME priority for time-critical sampling
- Use volatile keywords for ISR-shared variables
- Minimize processing in interrupt handlers - defer to tasks