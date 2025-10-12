typedef struct
{
    // 通道1电压数据 (分析层输出 → 显示层输入)
    float ch1_data[WAVEFORM_BUFFER_SIZE]; // 1024个电压值

    // 通道2电压数据
    float ch2_data[WAVEFORM_BUFFER_SIZE];

    // 测量参数 (分析层输出 → 表格显示输入)
    wave_params_t ch1_params; // 包含 max/min/avg/rms/frequency/period/duty_cycle
    wave_params_t ch2_params;

    // 触发信息
    uint16_t trigger_position; // 触发点位置
    uint8_t data_ready;        // 数据就绪标志
} waveform_data_t;

// 全局实例 (在 waveform_task.c 中定义)
extern waveform_data_t g_waveform_data;

/**
 * @brief  获取波形数据 (GUI层调用)
 * @return 波形数据结构指针
 */
waveform_data_t *waveform_get_data(void);

/**
 * @brief  设置触发参数 (GUI控制面板调用)
 * @param  mode: TRIGGER_AUTO/NORMAL/SINGLE
 * @param  level: 触发电平 (0~3.3V)
 * @param  edge: TRIGGER_RISING/FALLING
 */
void waveform_set_trigger(uint8_t mode, float level, uint8_t edge);

/**
 * @brief  启用/禁用通道
 * @param  channel: 1或2
 * @param  enable: 0=禁用, 1=启用
 */
void waveform_enable_channel(uint8_t channel, uint8_t enable);

// 使用示例 (在 oscilloscope.c 中):
waveform_data_t *wave_data = waveform_get_data();
float voltage_at_point_100 = wave_data->ch1_data[100]; // 获取第100个采样点电压
float max_voltage = wave_data->ch1_params.max_voltage; // 获取最大电压

// 使用示例 (在 GUI按钮回调中):
waveform_set_trigger(TRIGGER_NORMAL, 1.65f, TRIGGER_RISING);
waveform_enable_channel(2, 0); // 关闭通道2
