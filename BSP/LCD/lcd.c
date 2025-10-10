/**
 ****************************************************************************************************
 * @file        lcd.c
 * @author      ??(ALIENTEK)
 * @version     V1.1
 * @date        2023-05-29
 * @brief       2.8/3.5/4.3/7 TFTLCD(MCU) 
 *              ?IC??:ILI9341/NT35310/NT35510/SSD1963/ST7789/ST7796/ILI9806
 *
 * @license     Copyright (c) 2020-2032, ????
 ****************************************************************************************************
 * @attention
 *
 * ???:? ? F407
 * ?:www.yuanzige.com
 * ?:www.openedv.com
 * ??:www.alientek.com
 * ?:openedv.taobao.com
 *
 * ??
 * V1.0 20211016
 * ??
 * V1.1 20230529
 * 1ST7796ILI9806 IC?
 * 2??????
 ****************************************************************************************************
 */

#include "./BSP/LCD/lcd.h"
#include "./BSP/LCD/lcdfont.h"
#include "stdlib.h"

/* lcd_ex.cŸLCDIC????,?lcd.c,.c?
 * ???,?lcd.cõ,?include?.(?
 * ??.c?!!?!)
 */
#include "./BSP/LCD/lcd_ex.c"

SRAM_HandleTypeDef g_sram_handle; /* SRAM(?LCD) */

/* LCD???? */
uint32_t g_point_color = 0xF800; /* ? */
uint32_t g_back_color = 0xFFFF;  /* ? */

/* LCD? */
_lcd_dev lcddev;

/**
 * @brief       LCD?
 * @param       data: ??
 * @retval      
 */
void lcd_wr_data(volatile uint16_t data)
{
    data = data; /* ?-O2??,? */
    LCD->LCD_RAM = data;
}

/**
 * @brief       LCD??/?
 * @param       regno: ?/?
 * @retval      
 */
void lcd_wr_regno(volatile uint16_t regno)
{
    regno = regno;        /* ?-O2??,? */
    LCD->LCD_REG = regno; /* ????? */
}

/**
 * @brief       LCD??
 * @param       regno:?/?
 * @param       data:??
 * @retval      
 */
void lcd_write_reg(uint16_t regno, uint16_t data)
{
    LCD->LCD_REG = regno; /* ????? */
    LCD->LCD_RAM = data;  /* ? */
}

/**
 * @brief       LCD?,?mdk -O1????õ??
 * @param       t:??
 * @retval      
 */
static void lcd_opt_delay(uint32_t i)
{
    while (i--)
        ; /* ?AC6????,?while(1) __asm volatile(""); */
}

/**
 * @brief       LCD
 * @param       
 * @retval      ?
 */
static uint16_t lcd_rd_data(void)
{
    volatile uint16_t ram; /* ?? */
    lcd_opt_delay(2);
    ram = LCD->LCD_RAM;
    return ram;
}

/**
 * @brief       ??GRAM
 * @param       
 * @retval      
 */
void lcd_write_ram_prepare(void)
{
    LCD->LCD_REG = lcddev.wramcmd;
}

/**
 * @brief       ????
 * @param       x,y:
 * @retval      ??(32??,LTDC)
 */
uint32_t lcd_read_point(uint16_t x, uint16_t y)
{
    uint16_t r = 0, g = 0, b = 0;

    if (x >= lcddev.width || y >= lcddev.height)
    {
        return 0; /* ??,?? */
    }

    lcd_set_cursor(x, y); /*  */

    if (lcddev.id == 0x5510)
    {
        lcd_wr_regno(0x2E00); /* 5510 ?GRAM? */
    }
    else
    {
        lcd_wr_regno(0x2E); /* 9341/5310/1963/7789/7796/9806 ??GRAM? */
    }

    r = lcd_rd_data(); /* ?(dummy read) */

    if (lcddev.id == 0x1963)
    {
        return r; /* 1963??? */
    }

    r = lcd_rd_data(); /* ?? */

    if (lcddev.id == 0x7796) /* 7796 ????? */
    {
        return r;
    }

    /* 9341/5310/5510/7789/9806?2? */
    b = lcd_rd_data();
    g = r & 0xFF; /* 9341/5310/5510/7789/9806,???RG?,R?,G?,?8? */
    g <<= 8;

    return (((r >> 11) << 11) | ((g >> 10) << 5) |
            (b >> 11)); /* ILI9341/NT35310/NT35510/ST7789/ILI9806???? */
}

/**
 * @brief       LCD?
 * @param       
 * @retval      
 */
void lcd_display_on(void)
{
    if (lcddev.id == 0x5510)
    {
        lcd_wr_regno(0x2900); /* ? */
    }
    else /* 9341/5310/1963/7789/7796/9806 ???? */
    {
        lcd_wr_regno(0x29); /* ? */
    }
}

/**
 * @brief       LCD??
 * @param       
 * @retval      
 */
void lcd_display_off(void)
{
    if (lcddev.id == 0x5510)
    {
        lcd_wr_regno(0x2800); /* ?? */
    }
    else /* 9341/5310/1963/7789/7796/9806 ????? */
    {
        lcd_wr_regno(0x28); /* ?? */
    }
}

/**
 * @brief       ù?(RGB?)
 * @param       x,y: 
 * @retval      
 */
void lcd_set_cursor(uint16_t x, uint16_t y)
{
    if (lcddev.id == 0x1963)
    {
        if (lcddev.dir == 0) /* ??, x?? */
        {
            x = lcddev.width - 1 - x;
            lcd_wr_regno(lcddev.setxcmd);
            lcd_wr_data(0);
            lcd_wr_data(0);
            lcd_wr_data(x >> 8);
            lcd_wr_data(x & 0xFF);
        }
        else /* ?? */
        {
            lcd_wr_regno(lcddev.setxcmd);
            lcd_wr_data(x >> 8);
            lcd_wr_data(x & 0xFF);
            lcd_wr_data((lcddev.width - 1) >> 8);
            lcd_wr_data((lcddev.width - 1) & 0xFF);
        }

        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_data(y & 0xFF);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_data((lcddev.height - 1) & 0xFF);
    }
    else if (lcddev.id == 0x5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(x >> 8);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(x & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(y & 0xFF);
    }
    else /* 9341/5310/7789/7796/9806   */
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(x >> 8);
        lcd_wr_data(x & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_data(y & 0xFF);
    }
}

/**
 * @brief       LCD???(RGB?)
 *   @note
 *              9341/5310/5510/1963/7789/7796/9806IC???
 *              ?:???õ?(9341),
 *              ,??L2R_U2D,????,??.
 *
 * @param       dir:0~7,8(?lcd.h)
 * @retval      
 */
void lcd_scan_dir(uint8_t dir)
{
    uint16_t regval = 0;
    uint16_t dirreg = 0;
    uint16_t temp;

    /* ?1963????1963??(1963?,IC?) */
    if ((lcddev.dir == 1 && lcddev.id != 0x1963) || (lcddev.dir == 0 && lcddev.id == 0x1963))
    {
        switch (dir) /* ? */
        {
        case 0:
            dir = 6;
            break;

        case 1:
            dir = 7;
            break;

        case 2:
            dir = 4;
            break;

        case 3:
            dir = 5;
            break;

        case 4:
            dir = 1;
            break;

        case 5:
            dir = 0;
            break;

        case 6:
            dir = 3;
            break;

        case 7:
            dir = 2;
            break;
        }
    }

    /* ???  0x36/0x3600 ? bit 5,6,7 ?? */
    switch (dir)
    {
    case L2R_U2D: /* ,? */
        regval |= (0 << 7) | (0 << 6) | (0 << 5);
        break;

    case L2R_D2U: /* ,µ */
        regval |= (1 << 7) | (0 << 6) | (0 << 5);
        break;

    case R2L_U2D: /* ?,? */
        regval |= (0 << 7) | (1 << 6) | (0 << 5);
        break;

    case R2L_D2U: /* ?,µ */
        regval |= (1 << 7) | (1 << 6) | (0 << 5);
        break;

    case U2D_L2R: /* ?, */
        regval |= (0 << 7) | (0 << 6) | (1 << 5);
        break;

    case U2D_R2L: /* ?,? */
        regval |= (0 << 7) | (1 << 6) | (1 << 5);
        break;

    case D2U_L2R: /* µ, */
        regval |= (1 << 7) | (0 << 6) | (1 << 5);
        break;

    case D2U_R2L: /* µ,? */
        regval |= (1 << 7) | (1 << 6) | (1 << 5);
        break;
    }

    dirreg = 0x36; /* ???IC, 0x36? */

    if (lcddev.id == 0x5510)
    {
        dirreg = 0x3600; /* 5510, ic??? */
    }

    /* 9341 & 7789 & 7796 ?BGR? */
    if (lcddev.id == 0x9341 || lcddev.id == 0x7789 || lcddev.id == 0x7796)
    {
        regval |= 0x08;
    }

    lcd_write_reg(dirreg, regval);

    if (lcddev.id != 0x1963) /* 1963? */
    {
        if (regval & 0x20)
        {
            if (lcddev.width < lcddev.height) /* X,Y */
            {
                temp = lcddev.width;
                lcddev.width = lcddev.height;
                lcddev.height = temp;
            }
        }
        else
        {
            if (lcddev.width > lcddev.height) /* X,Y */
            {
                temp = lcddev.width;
                lcddev.width = lcddev.height;
                lcddev.height = temp;
            }
        }
    }

    /* ?()? */
    if (lcddev.id == 0x5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setxcmd + 2);
        lcd_wr_data((lcddev.width - 1) >> 8);
        lcd_wr_regno(lcddev.setxcmd + 3);
        lcd_wr_data((lcddev.width - 1) & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setycmd + 2);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_regno(lcddev.setycmd + 3);
        lcd_wr_data((lcddev.height - 1) & 0xFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(0);
        lcd_wr_data(0);
        lcd_wr_data((lcddev.width - 1) >> 8);
        lcd_wr_data((lcddev.width - 1) & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(0);
        lcd_wr_data(0);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_data((lcddev.height - 1) & 0xFF);
    }
}

/**
 * @brief       
 * @param       x,y: 
 * @param       color: ?(32??,LTDC)
 * @retval      
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
    lcd_set_cursor(x, y);    /* ù? */
    lcd_write_ram_prepare(); /* ??GRAM */
    LCD->LCD_RAM = color;
}

/**
 * @brief       SSD1963ú
 * @param       pwm: ?,0~100.??.
 * @retval      
 */
void lcd_ssd_backlight_set(uint8_t pwm)
{
    lcd_wr_regno(0xBE);      /* PWM */
    lcd_wr_data(0x05);       /* 1PWM? */
    lcd_wr_data(pwm * 2.55); /* 2PWM?? */
    lcd_wr_data(0x01);       /* 3C */
    lcd_wr_data(0xFF);       /* 4D */
    lcd_wr_data(0x00);       /* 5E */
    lcd_wr_data(0x00);       /* 6F */
}

/**
 * @brief       LCD?
 * @param       dir:0,; 1,
 * @retval      
 */
void lcd_display_dir(uint8_t dir)
{
    lcddev.dir = dir; /* / */

    if (dir == 0) /*  */
    {
        lcddev.width = 240;
        lcddev.height = 320;

        if (lcddev.id == 0x5510)
        {
            lcddev.wramcmd = 0x2C00;
            lcddev.setxcmd = 0x2A00;
            lcddev.setycmd = 0x2B00;
            lcddev.width = 480;
            lcddev.height = 800;
        }
        else if (lcddev.id == 0x1963)
        {
            lcddev.wramcmd = 0x2C; /* ?GRAM? */
            lcddev.setxcmd = 0x2B; /* ?X? */
            lcddev.setycmd = 0x2A; /* ?Y? */
            lcddev.width = 480;    /* ÿ480 */
            lcddev.height = 800;   /* ø?800 */
        }
        else /* IC, : 9341/5310/7789/7796/9806IC */
        {
            lcddev.wramcmd = 0x2C;
            lcddev.setxcmd = 0x2A;
            lcddev.setycmd = 0x2B;
        }

        if (lcddev.id == 0x5310 || lcddev.id == 0x7796) /* 5310/7796 ? 320*480? */
        {
            lcddev.width = 320;
            lcddev.height = 480;
        }

        if (lcddev.id == 0X9806) /* 9806 ? 480*800 ? */
        {
            lcddev.width = 480;
            lcddev.height = 800;
        }
    }
    else /*  */
    {
        lcddev.width = 320;  /* ?? */
        lcddev.height = 240; /* ??? */

        if (lcddev.id == 0x5510)
        {
            lcddev.wramcmd = 0x2C00;
            lcddev.setxcmd = 0x2A00;
            lcddev.setycmd = 0x2B00;
            lcddev.width = 800;
            lcddev.height = 480;
        }
        else if (lcddev.id == 0x1963 || lcddev.id == 0x9806)
        {
            lcddev.wramcmd = 0x2C; /* ?GRAM? */
            lcddev.setxcmd = 0x2A; /* ?X? */
            lcddev.setycmd = 0x2B; /* ?Y? */
            lcddev.width = 800;    /* ÿ800 */
            lcddev.height = 480;   /* ø?480 */
        }
        else /* IC, :9341/5310/7789/7796IC */
        {
            lcddev.wramcmd = 0x2C;
            lcddev.setxcmd = 0x2A;
            lcddev.setycmd = 0x2B;
        }

        if (lcddev.id == 0x5310 || lcddev.id == 0x7796) /* 5310/7796 ? 320*480? */
        {
            lcddev.width = 480;
            lcddev.height = 320;
        }
    }

    lcd_scan_dir(DFT_SCAN_DIR); /* ??? */
}

/**
 * @brief       ô(RGB?), ?û??(sx,sy).
 * @param       sx,sy:?(?)
 * @param       width,height:????,0!!
 *   @note      ?:width*height.
 *
 * @retval      
 */
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t twidth, theight;
    twidth = sx + width - 1;
    theight = sy + height - 1;

    if (lcddev.id == 0x1963 && lcddev.dir != 1) /* 1963? */
    {
        sx = lcddev.width - width - sx;
        height = sy + height - 1;
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8);
        lcd_wr_data(sx & 0xFF);
        lcd_wr_data((sx + width - 1) >> 8);
        lcd_wr_data((sx + width - 1) & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8);
        lcd_wr_data(sy & 0xFF);
        lcd_wr_data(height >> 8);
        lcd_wr_data(height & 0xFF);
    }
    else if (lcddev.id == 0x5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(sx & 0xFF);
        lcd_wr_regno(lcddev.setxcmd + 2);
        lcd_wr_data(twidth >> 8);
        lcd_wr_regno(lcddev.setxcmd + 3);
        lcd_wr_data(twidth & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(sy & 0xFF);
        lcd_wr_regno(lcddev.setycmd + 2);
        lcd_wr_data(theight >> 8);
        lcd_wr_regno(lcddev.setycmd + 3);
        lcd_wr_data(theight & 0xFF);
    }
    else /* 9341/5310/7789/1963/7796/9806  ô */
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8);
        lcd_wr_data(sx & 0xFF);
        lcd_wr_data(twidth >> 8);
        lcd_wr_data(twidth & 0xFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8);
        lcd_wr_data(sy & 0xFF);
        lcd_wr_data(theight >> 8);
        lcd_wr_data(theight & 0xFF);
    }
}

/**
 * @brief       SRAM?????
 * @note        ??HAL_SRAM_Init(),??
 * @param       hsram:SRAM
 * @retval      
 */
// void HAL_SRAM_MspInit(SRAM_HandleTypeDef *hsram)
// {
//     GPIO_InitTypeDef gpio_init_struct;

//     __HAL_RCC_FSMC_CLK_ENABLE();            /* ?FSMC? */
//     __HAL_RCC_GPIOD_CLK_ENABLE();           /* ?GPIOD? */
//     __HAL_RCC_GPIOE_CLK_ENABLE();           /* ?GPIOE? */

//     /* ?PD0,1, 8,9,10,14,15 */
//     gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 \
//                            | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
//     gpio_init_struct.Mode = GPIO_MODE_AF_PP;            /* ? */
//     gpio_init_struct.Pull = GPIO_PULLUP;                /*  */
//     gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;      /*  */
//     gpio_init_struct.Alternate = GPIO_AF12_FSMC;        /* ?FSMC */

//     HAL_GPIO_Init(GPIOD, &gpio_init_struct);            /* ? */

//     /* ?PE7,8,9,10,11,12,13,14,15 */
//     gpio_init_struct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 \
//                            | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
//     HAL_GPIO_Init(GPIOE, &gpio_init_struct);
// }

/**
 * @brief       ?LCD
 *   @note      ó?????LCD(.c??)
 *
 * @param       
 * @retval      
 */
void lcd_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    FSMC_NORSRAM_TimingTypeDef fsmc_read_handle;
    FSMC_NORSRAM_TimingTypeDef fsmc_write_handle;

    LCD_CS_GPIO_CLK_ENABLE(); /* LCD_CS?? */
    LCD_WR_GPIO_CLK_ENABLE(); /* LCD_WR?? */
    LCD_RD_GPIO_CLK_ENABLE(); /* LCD_RD?? */
    LCD_RS_GPIO_CLK_ENABLE(); /* LCD_RS?? */
    LCD_BL_GPIO_CLK_ENABLE(); /* LCD_BL?? */

    gpio_init_struct.Pin = LCD_CS_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;            /* ? */
    gpio_init_struct.Pull = GPIO_PULLUP;                /*  */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;      /*  */
    gpio_init_struct.Alternate = GPIO_AF12_FSMC;        /* ?FSMC */
    HAL_GPIO_Init(LCD_CS_GPIO_PORT, &gpio_init_struct); /* ?LCD_CS */

    gpio_init_struct.Pin = LCD_WR_GPIO_PIN;
    HAL_GPIO_Init(LCD_WR_GPIO_PORT, &gpio_init_struct); /* ?LCD_WR */

    gpio_init_struct.Pin = LCD_RD_GPIO_PIN;
    HAL_GPIO_Init(LCD_RD_GPIO_PORT, &gpio_init_struct); /* ?LCD_RD */

    gpio_init_struct.Pin = LCD_RS_GPIO_PIN;
    HAL_GPIO_Init(LCD_RS_GPIO_PORT, &gpio_init_struct); /* ?LCD_RS */

    gpio_init_struct.Pin = LCD_BL_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;        /*  */
    HAL_GPIO_Init(LCD_BL_GPIO_PORT, &gpio_init_struct); /* LCD_BL??() */

    g_sram_handle.Instance = FSMC_NORSRAM_DEVICE;
    g_sram_handle.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;

    g_sram_handle.Init.NSBank = FSMC_NORSRAM_BANK4;                     /* ?NE4 */
    g_sram_handle.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;  /* ?/? */
    g_sram_handle.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16; /* 16?? */
    g_sram_handle.Init.BurstAccessMode =
        FSMC_BURST_ACCESS_MODE_DISABLE; /* ???,????,??õ */
    g_sram_handle.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW; /* ????,??? */
    g_sram_handle.Init.WaitSignalActive =
        FSMC_WAIT_TIMING_BEFORE_WS; /* ????????????NWAIT */
    g_sram_handle.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE; /* ??? */
    g_sram_handle.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;        /* ???,??õ */
    g_sram_handle.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;     /* ??ò?? */
    g_sram_handle.Init.AsynchronousWait =
        FSMC_ASYNCHRONOUS_WAIT_DISABLE;                       /* ?????µ???,??õ */
    g_sram_handle.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE; /* ??? */

    /* FSMC??? */
    fsmc_read_handle.AddressSetupTime = 0x0F;         /* ??(ADDSET)?15fsmc_ker_ck(1/168=6)6*15=90ns */
    fsmc_read_handle.AddressHoldTime = 0x00;          /* ??(ADDHLD) ??Aûõ */
    fsmc_read_handle.DataSetupTime = 60;              /* ??(DATAST)?60fsmc_ker_ck=6*60=360ns */
                                                      /* ??IC???,???,??? */
    fsmc_read_handle.AccessMode = FSMC_ACCESS_MODE_A; /* ??A */

    /* FSMC???? */
    fsmc_write_handle.AddressSetupTime = 9;            /* ??(ADDSET)?9fsmc_ker_ck=6*9=54ns */
    fsmc_write_handle.AddressHoldTime = 0x00;          /* ??(ADDHLD) ??Aûõ */
    fsmc_write_handle.DataSetupTime = 9;               /* ??(DATAST)?9fsmc_ker_ck=6*9=54ns */
                                                       /* ?????IC???50ns */
    fsmc_write_handle.AccessMode = FSMC_ACCESS_MODE_A; /* ??A */

    HAL_SRAM_Init(&g_sram_handle, &fsmc_read_handle, &fsmc_write_handle);
    delay_ms(50);

    /* 9341 ID?? */
    lcd_wr_regno(0xD3);
    lcddev.id = lcd_rd_data(); /* dummy read */
    lcddev.id = lcd_rd_data(); /* 0x00 */
    lcddev.id = lcd_rd_data(); /* ?93 */
    lcddev.id <<= 8;
    lcddev.id |= lcd_rd_data(); /* ?41 */

    if (lcddev.id != 0x9341) /*  9341 , ?? ST7789 */
    {
        lcd_wr_regno(0x04);
        lcddev.id = lcd_rd_data(); /* dummy read */
        lcddev.id = lcd_rd_data(); /* 0x85 */
        lcddev.id = lcd_rd_data(); /* ?0x85 */
        lcddev.id <<= 8;
        lcddev.id |= lcd_rd_data(); /* ?0x52 */

        if (lcddev.id == 0x8552) /* 8552ID?7789 */
        {
            lcddev.id = 0x7789;
        }

        if (lcddev.id != 0x7789) /* ?ST7789, ? NT35310 */
        {
            lcd_wr_regno(0xD4);
            lcddev.id = lcd_rd_data(); /* dummy read */
            lcddev.id = lcd_rd_data(); /* 0x01 */
            lcddev.id = lcd_rd_data(); /* 0x53 */
            lcddev.id <<= 8;
            lcddev.id |= lcd_rd_data(); /* 0x10 */

            if (lcddev.id != 0x5310) /* ?NT35310,??ST7796 */
            {
                lcd_wr_regno(0XD3);
                lcddev.id = lcd_rd_data(); /* dummy read */
                lcddev.id = lcd_rd_data(); /* 0X00 */
                lcddev.id = lcd_rd_data(); /* ?0X77 */
                lcddev.id <<= 8;
                lcddev.id |= lcd_rd_data(); /* ?0X96 */

                if (lcddev.id != 0x7796) /* ?ST7796,??NT35510 */
                {
                    /* ?? */
                    lcd_write_reg(0xF000, 0x0055);
                    lcd_write_reg(0xF001, 0x00AA);
                    lcd_write_reg(0xF002, 0x0052);
                    lcd_write_reg(0xF003, 0x0008);
                    lcd_write_reg(0xF004, 0x0001);

                    lcd_wr_regno(0xC500);      /* ?ID?? */
                    lcddev.id = lcd_rd_data(); /* 0x80 */
                    lcddev.id <<= 8;

                    lcd_wr_regno(0xC501);       /* ?ID?? */
                    lcddev.id |= lcd_rd_data(); /* 0x00 */

                    delay_ms(5); /* ?5ms, ?0XC501?1963???, ?5ms1963?? */

                    if (lcddev.id != 0x5510) /* ?NT5510,??ILI9806 */
                    {
                        lcd_wr_regno(0XD3);
                        lcddev.id = lcd_rd_data(); /* dummy read */
                        lcddev.id = lcd_rd_data(); /* 0X00 */
                        lcddev.id = lcd_rd_data(); /* 0X98 */
                        lcddev.id <<= 8;
                        lcddev.id |= lcd_rd_data(); /* 0X06 */

                        if (lcddev.id != 0x9806) /* ?ILI9806,??SSD1963 */
                        {
                            lcd_wr_regno(0xA1);
                            lcddev.id = lcd_rd_data();
                            lcddev.id = lcd_rd_data(); /* 0x57 */
                            lcddev.id <<= 8;
                            lcddev.id |= lcd_rd_data(); /* 0x61 */

                            if (lcddev.id == 0x5761)
                                lcddev.id = 0x1963; /* SSD1963?ID5761H,?,??1963 */
                        }
                    }
                }
            }
        }
    }

    /* ??, main?1?, ?printf
     * (f_putc), , ?1, ?
     *  printf  !!!!!!!
     */
    //    printf("LCD ID:%x\r\n", lcddev.id); /* ?LCD ID */

    if (lcddev.id == 0x7789)
    {
        lcd_ex_st7789_reginit(); /* ?ST7789? */
    }
    else if (lcddev.id == 0x9341)
    {
        lcd_ex_ili9341_reginit(); /* ?ILI9341? */
    }
    else if (lcddev.id == 0x5310)
    {
        lcd_ex_nt35310_reginit(); /* ?NT35310? */
    }
    else if (lcddev.id == 0x7796)
    {
        lcd_ex_st7796_reginit(); /* ?ST7796? */
    }
    else if (lcddev.id == 0x5510)
    {
        lcd_ex_nt35510_reginit(); /* ?NT35510? */
    }
    else if (lcddev.id == 0x9806)
    {
        lcd_ex_ili9806_reginit(); /* ?ILI9806? */
    }
    else if (lcddev.id == 0x1963)
    {
        lcd_ex_ssd1963_reginit();   /* ?SSD1963? */
        lcd_ssd_backlight_set(100); /* ? */
    }

    /* ??, */
    if (lcddev.id == 0x7789 || lcddev.id == 0x9341 || lcddev.id == 0x1963) /* 7789/9341/1963  */
    {
        /* ????? */
        fsmc_write_handle.AddressSetupTime = 3; /* ??(ADDSET)?3fsmc_ker_ck=6*3=18ns */
        fsmc_write_handle.DataSetupTime = 3;    /* ??(DATAST)?3fsmc_ker_ck=6*3=18ns */
        FSMC_NORSRAM_Extended_Timing_Init(g_sram_handle.Extended, &fsmc_write_handle, g_sram_handle.Init.NSBank,
                                          g_sram_handle.Init.ExtendedMode);
    }
    else if (lcddev.id == 0x5310 || lcddev.id == 0x7796 || lcddev.id == 0x5510 ||
             lcddev.id == 0x9806) /* ?IC, WR?? */
    {
        /* ????? */
        fsmc_write_handle.AddressSetupTime = 2; /* ??(ADDSET)?2fsmc_ker_ck=6*2=12ns */
        fsmc_write_handle.DataSetupTime = 2;    /* ??(DATAST)?2fsmc_ker_ck=6*2=12ns */
        FSMC_NORSRAM_Extended_Timing_Init(g_sram_handle.Extended, &fsmc_write_handle, g_sram_handle.Init.NSBank,
                                          g_sram_handle.Init.ExtendedMode);
    }

    lcd_display_dir(0); /* ?? */
    LCD_BL(1);          /*  */
    lcd_clear(WHITE);
}

/**
 * @brief       
 * @param       color: ??
 * @retval      
 */
void lcd_clear(uint16_t color)
{
    uint32_t index = 0;
    uint32_t totalpoint = lcddev.width;

    totalpoint *= lcddev.height;  /* õ? */
    lcd_set_cursor(0x00, 0x0000); /* ù? */
    lcd_write_ram_prepare();      /* ??GRAM */

    for (index = 0; index < totalpoint; index++)
    {
        LCD->LCD_RAM = color;
    }
}

/**
 * @brief       ???
 * @param       (sx,sy),(ex,ey):??,??:(ex - sx + 1) * (ey - sy + 1)
 * @param       color:  ??(32??,LTDC)
 * @retval      
 */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint16_t i, j;
    uint16_t xlen = 0;
    xlen = ex - sx + 1;

    for (i = sy; i <= ey; i++)
    {
        lcd_set_cursor(sx, i);   /* ù? */
        lcd_write_ram_prepare(); /* ??GRAM */

        for (j = 0; j < xlen; j++)
        {
            LCD->LCD_RAM = color; /* ?? */
        }
    }
}

/**
 * @brief       ???
 * @param       (sx,sy),(ex,ey):??,??:(ex - sx + 1) * (ey - sy + 1)
 * @param       color: ????
 * @retval      
 */
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t height, width;
    uint16_t i, j;

    width = ex - sx + 1;  /* õ? */
    height = ey - sy + 1; /* ? */

    for (i = 0; i < height; i++)
    {
        lcd_set_cursor(sx, sy + i); /* ù? */
        lcd_write_ram_prepare();    /* ??GRAM */

        for (j = 0; j < width; j++)
        {
            LCD->LCD_RAM = color[i * width + j]; /* ? */
        }
    }
}

/**
 * @brief       
 * @param       x1,y1: 
 * @param       x2,y2: ?
 * @param       color: ??
 * @retval      
 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1; /*  */
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0)
    {
        incx = 1; /* õ */
    }
    else if (delta_x == 0)
    {
        incx = 0; /* ? */
    }
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0)
    {
        incy = 1;
    }
    else if (delta_y == 0)
    {
        incy = 0; /* ?? */
    }
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if (delta_x > delta_y)
    {
        distance = delta_x; /* ?? */
    }
    else
    {
        distance = delta_y;
    }

    for (t = 0; t <= distance + 1; t++) /*  */
    {
        lcd_draw_point(row, col, color); /*  */
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if (yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }
}

/**
 * @brief       ??
 * @param       x,y   : 
 * @param       len   : ?
 * @param       color : ??
 * @retval      
 */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > lcddev.width) || (y > lcddev.height))
    {
        return;
    }

    lcd_fill(x, y, x + len - 1, y, color);
}

/**
 * @brief       
 * @param       x1,y1: 
 * @param       x2,y2: ?
 * @param       color: ??
 * @retval      
 */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

/**
 * @brief       ?
 * @param       x0,y0 : ?
 * @param       r     : ?
 * @param       color : ??
 * @retval      
 */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a, b;
    int di;

    a = 0;
    b = r;
    di = 3 - (r << 1); /* ?¸?õ?? */

    while (a <= b)
    {
        lcd_draw_point(x0 + a, y0 - b, color); /* 5 */
        lcd_draw_point(x0 + b, y0 - a, color); /* 0 */
        lcd_draw_point(x0 + b, y0 + a, color); /* 4 */
        lcd_draw_point(x0 + a, y0 + b, color); /* 6 */
        lcd_draw_point(x0 - a, y0 + b, color); /* 1 */
        lcd_draw_point(x0 - b, y0 + a, color);
        lcd_draw_point(x0 - a, y0 - b, color); /* 2 */
        lcd_draw_point(x0 - b, y0 - a, color); /* 7 */
        a++;

        /* ?Bresenham?? */
        if (di < 0)
        {
            di += 4 * a + 6;
        }
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

/**
 * @brief       ??
 * @param       x,y  : ?
 * @param       r    : ?
 * @param       color: ??
 * @retval      
 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    uint32_t i;
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
    uint32_t xr = r;

    lcd_draw_hline(x - r, y, 2 * r, color);

    for (i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {
            /* draw lines from outside */
            if (xr > imax)
            {
                lcd_draw_hline(x - i + 1, y + xr, 2 * (i - 1), color);
                lcd_draw_hline(x - i + 1, y - xr, 2 * (i - 1), color);
            }

            xr--;
        }

        /* draw lines from inside (center) */
        lcd_draw_hline(x - xr, y + i, 2 * xr, color);
        lcd_draw_hline(x - xr, y - i, 2 * xr, color);
    }
}

/**
 * @brief       ?????
 * @param       x,y  : 
 * @param       chr  : ???:" "--->"~"
 * @param       size : ? 12/16/24/32
 * @param       mode : ??(1); ???(0);
 * @param       color : ??;
 * @retval      
 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize = 0;
    uint8_t *pfont = 0;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2); /* õ????? */
    chr = chr - ' '; /* õ???ASCII???????-' '????? */

    switch (size)
    {
    case 12:
        pfont = (uint8_t *)asc2_1206[chr]; /* 1206 */
        break;

    case 16:
        pfont = (uint8_t *)asc2_1608[chr]; /* 1608 */
        break;

    case 24:
        pfont = (uint8_t *)asc2_2412[chr]; /* 2412 */
        break;

    case 32:
        pfont = (uint8_t *)asc2_3216[chr]; /* 3216 */
        break;

    default:
        return;
    }

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t]; /* ??? */

        for (t1 = 0; t1 < 8; t1++) /* ??8 */
        {
            if (temp & 0x80) /* ?,?? */
            {
                lcd_draw_point(x, y, color); /* ,?? */
            }
            else if (mode == 0) /* ?,? */
            {
                lcd_draw_point(x, y, g_back_color); /* ?,???(?????) */
            }

            temp <<= 1; /* ?, ?????? */
            y++;

            if (y >= lcddev.height)
                return; /*  */

            if ((y - y0) == size) /* ??? */
            {
                y = y0; /* y?? */
                x++;    /* x */

                if (x >= lcddev.width)
                {
                    return; /* x? */
                }

                break;
            }
        }
    }
}

/**
 * @brief       ?, m^n
 * @param       m: 
 * @param       n: ?
 * @retval      mn?
 */
static uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)
    {
        result *= m;
    }

    return result;
}

/**
 * @brief       ?len
 * @param       x,y : ?
 * @param       num : ?(0 ~ 2^32)
 * @param       len : ???
 * @param       size: ? 12/16/24/32
 * @param       color : ??;
 * @retval      
 */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++) /* ??? */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10; /* ??? */

        if (enshow == 0 && t < (len - 1)) /* û??,???? */
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2) * t, y, ' ', size, 0, color); /* ??,?? */
                continue;                                                  /* ¸?? */
            }
            else
            {
                enshow = 1; /* ?? */
            }
        }

        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, 0, color); /* ?? */
    }
}

/**
 * @brief       ??len(?0??)
 * @param       x,y : ?
 * @param       num : ?(0 ~ 2^32)
 * @param       len : ???
 * @param       size: ? 12/16/24/32
 * @param       mode: ???
 *              [7]:0,;1,0.
 *              [6:1]:
 *              [0]:0,??;1,?.
 * @param       color : ??;
 * @retval      
 */
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++) /* ??? */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10; /* ??? */

        if (enshow == 0 && t < (len - 1)) /* û??,???? */
        {
            if (temp == 0)
            {
                if (mode & 0x80) /* ??0 */
                {
                    lcd_show_char(x + (size / 2) * t, y, '0', size, mode & 0x01, color); /* 0?? */
                }
                else
                {
                    lcd_show_char(x + (size / 2) * t, y, ' ', size, mode & 0x01, color); /* ÿ??? */
                }

                continue;
            }
            else
            {
                enshow = 1; /* ?? */
            }
        }

        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, mode & 0x01, color);
    }
}

/**
 * @brief       ??
 * @param       x,y         : ?
 * @param       width,height: ?
 * @param       size        : ? 12/16/24/32
 * @param       p           : ???
 * @param       color       : ??;
 * @retval      
 */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    uint8_t x0 = x;

    width += x;
    height += y;

    while ((*p <= '~') && (*p >= ' ')) /* ?????! */
    {
        if (x >= width)
        {
            x = x0;
            y += size;
        }

        if (y >= height)
        {
            break; /* ? */
        }

        lcd_show_char(x, y, *p, size, 0, color);
        x += size / 2;
        p++;
    }
}
