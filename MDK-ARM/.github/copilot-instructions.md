# Copilot Instructions for STM32F407 Oscilloscope Project

## Project Overview
This is an embedded STM32F407ZGTx-based dual-channel oscilloscope project with LVGL GUI, built using Keil MDK-ARM IDE. The system provides real-time signal acquisition and visualization with custom waveform processing tasks.

## Architecture & Key Components

### Hardware Platform  
- **MCU**: STM32F407ZGTx (Cortex-M4F, 168MHz, 1MB Flash, 192KB RAM)
- **Memory Layout**: IRAM(0x20000000-0x2001BFFF:112KB), IRAM2(0x2001C000-0x2001FFFF:16KB), IROM(0x8000000-0x80FFFFF:1MB)
- **Display**: 320x240 TFT LCD with multi-touch support (FT5206/GT9xxx controllers)
- **Storage**: External SRAM via FSMC interface for display buffers

### Software Stack
- **RTOS**: FreeRTOS with CMSIS-RTOS v2 API
- **GUI**: LVGL 8.x with custom drawing optimizations for embedded displays  
- **HAL**: STM32F4xx HAL Driver with BSP abstraction layer
- **DSP**: ARM CMSIS-DSP library for FFT and signal processing
- **Touch**: Multi-controller support with unified BSP layer

### Directory Structure
```
../Core/         - STM32CubeMX generated code (main.c, peripherals, HAL config)
../APP/          - Application tasks (adc_task.c, waveform_task.c, lvgl_task.c)
../GUI/          - Custom LVGL screens and widgets (oscilloscope.c)
../BSP/          - Board Support Package (LCD, TOUCH, SRAM, IIC, TIMER)
../SYSTEM/       - System utilities (sys.c, delay.c)
../Middlewares/  - Third-party libraries (FreeRTOS, LVGL)
../Drivers/      - STM32 HAL drivers and CMSIS
oscilloscope/    - Build output directory with scatter file (.sct)
```

## Development Patterns

### Task-Based Architecture
- **ADC Task** (`../APP/adc_task.c`): Dual-channel ADC with DMA circular buffer sampling
- **Waveform Task** (`../APP/waveform_task.c`): Signal processing, FFT analysis, parameter calculation
- **LVGL Task** (`../APP/lvgl_task.c`): GUI event handling and display refresh (50ms timer)
- **Main Screen** (`../GUI/oscilloscope.c`): Custom oscilloscope UI with 320x240 grid, measurement table

### Memory Management & Critical Issues
- **Memory Crisis**: Current build fails with L6406E "No space in execution regions" 
- **Heap**: FreeRTOS heap_4.c (0x200 = 512 bytes) - severely constrained
- **Stack**: 0x1000 (4KB) per startup_stm32f407xx.s - may need reduction
- **LVGL Buffers**: Large display buffers consuming significant RAM
- **External SRAM**: FSMC-connected SRAM should be utilized for large buffers

### Peripheral Integration
- **Dual ADC + DMA**: CH1/CH2 with 1024-sample buffers, circular DMA transfer
- **DSP Processing**: ARM Math library for FFT, frequency analysis, voltage calculations
- **Display Pipeline**: Custom LVGL draw callbacks for grid, waveforms, measurements
- **Touch Integration**: BSP layer with unified GT9xxx/FT5206 support

## Build & Debug Workflow

### Critical Memory Issue (Current Blocker)
```bash
# Build currently FAILS with:
# Error: L6406E: No space in execution regions with .ANY selector
# 66 linker errors due to insufficient RAM allocation
```

### Memory Configuration Files
- `oscilloscope/oscilloscope.sct` - Scatter file defining memory regions
- `startup_stm32f407xx.s` - Stack (0x1000) and Heap (0x200) allocation  
- Memory regions: IRAM1(112KB), IRAM2(16KB), but heavily over-allocated

### Keil Project Structure
- `oscilloscope.uvprojx` - Project with 12 groups: Core, BSP, LVGL, APP, GUI
- Major memory consumers: LVGL widgets, FreeRTOS tasks, display buffers
- Build report: `oscilloscope/oscilloscope.htm` shows detailed memory usage

### Essential Debug Commands
```bash
# Build via Keil command line
UV4.exe -b oscilloscope.uvprojx -j0 -t oscilloscope
```

## Code Conventions

### Naming Patterns
- **Files**: lowercase with underscores (`adc_task.c`, `touch_driver.c`)
- **HAL Integration**: Use STM32 HAL naming (`HAL_ADC_Start_DMA`)
- **FreeRTOS**: Standard RTOS prefixes (`xTaskCreate`, `vTaskDelay`)
- **LVGL**: Library prefixes (`lv_obj_create`, `lv_chart_add_series`)

### Interrupt Handling
- Most IRQ handlers defined in `startup_stm32f407xx.s` default to infinite loop
- Override handlers in `stm32f4xx_it.c` for active peripherals
- Use HAL callbacks for peripheral-specific handling

### Configuration Files
- **HAL Config**: `stm32f4xx_hal_conf.h` - Enable/disable HAL modules
- **FreeRTOS Config**: `FreeRTOSConfig.h` - RTOS parameters and features
- **LVGL Config**: LVGL configuration for display and input handling

## Critical Integration Points

### ADC-DMA-Timer Chain
- Timer triggers ADC conversion at precise intervals
- DMA transfers samples to circular buffer without CPU intervention
- ADC task processes completed DMA transfers for waveform display

### LVGL-Touch Integration
- Touch controller data processed through I2C in separate task
- Touch events fed to LVGL input device driver
- GUI updates coordinated with display refresh timing

### Real-time Constraints
- ADC sampling rate determines maximum signal frequency
- DMA buffer size affects memory usage and latency
- FreeRTOS tick rate (typically 1kHz) sets minimum task switching granularity

## URGENT: Memory Crisis Resolution

### Current Build Failures
The project cannot link due to memory exhaustion. Key symptoms:
- L6406E errors for .bss, .data, STACK sections
- LVGL objects consuming excessive RAM
- 66 linker errors blocking all development

### Immediate Actions Required
1. **Reduce LVGL memory footprint**: Disable unused widgets, optimize buffer sizes
2. **Utilize external SRAM**: Move display buffers to FSMC-connected memory
3. **Optimize task stacks**: Reduce from 0x1000 to smaller values where possible
4. **Review scatter file**: Consider CCM RAM utilization for buffers

## Common Development Tasks

When implementing new features:
1. **New ADC channels**: Modify DMA configuration and buffer allocation
2. **GUI screens**: Use LVGL screen management (`lv_scr_load`)
3. **Data processing**: Create dedicated FreeRTOS tasks for signal analysis
4. **Touch gestures**: Extend touch task with gesture recognition
5. **External storage**: Utilize FSMC interface for waveform storage

### Oscilloscope-Specific Patterns
- **Waveform Display**: Use LVGL draw events for real-time plotting at 320x240 resolution
- **Parameter Table**: 4-row measurement table with frequency, voltage, wave type display
- **Signal Processing**: FFT analysis in waveform_task.c with ARM CMSIS-DSP library
- **Data Flow**: ADC → waveform_task → oscilloscope.c → LVGL display pipeline

Remember: This is a real-time system where timing constraints are critical for accurate signal acquisition and smooth user interface responsiveness.
