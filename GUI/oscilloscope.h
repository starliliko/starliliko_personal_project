#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H


/* 核心配置：适配320x240小屏幕 */
#define SCOPE_WIDTH     320     // 屏幕宽度
#define SCOPE_HEIGHT    240     // 屏幕高度
#define TABLE_HEIGHT    75      // 表格总高度（4行×16px + 边框）
#define WAVE_HEIGHT     (SCOPE_HEIGHT - TABLE_HEIGHT)  // 波形区高度

/* 其他参数 */
#define GRID_X_COUNT    10      
#define GRID_Y_COUNT    8       
#define BUFFER_SIZE     1024     
#define TIME_DIV        0.01     // 时基：0.01ms/格
   
//显示频率10-30K
/* 电压参数 - 明确每格0.5V */
#define V_PER_DIV       0.5f    // 每格电压：0.5V（核心参数，用于Y轴标识）
#define V_RANGE         4.0f    // 总电压范围：4V（-2V ~ +2V）
#define PIXELS_PER_VOLT (WAVE_HEIGHT / V_RANGE)  // 像素/伏换算


void create_oscilloscope_ui(void);

#endif // OSCILLOSCOPE_H