#ifndef _ST7789_DRIVER_H_
#define _ST7789_DRIVER_H_
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pvLcdFlushDoneCallback)(void *param);

typedef struct
{
    gpio_num_t xMOSI;                      // 数据
    gpio_num_t xClk;                       // 时钟
    gpio_num_t xCS;                        // 片选
    gpio_num_t xDC;                        // 命令
    gpio_num_t xRst;                       // 复位
    gpio_num_t xBL;                        // 背光
    uint32_t ulSPIFreq;                    // spi总线速率
    uint16_t uiWidth;                      // 宽
    uint16_t uiHeight;                     // 长
    uint8_t ucSpin;                        // 旋转角度( 0不旋转，1顺时针旋转90, 2旋转180，3顺时针旋转270 )
    pvLcdFlushDoneCallback pvDoneCallback; // 数据传输完成回调函数
    void *pvCallbackParam;                 // 回调函数参数
} St7789Config_t;

/** st7789初始化
 * @param St7789Config_t  接口参数
 * @return 成功或失败
 */
esp_err_t xSt7789DriverHwInit(St7789Config_t *pxConfig);

/** st7789写入显示数据
 * @param x1,x2,y1,y2:显示区域
 * @return 无
 */
void vSt7789Flush(int x1, int x2, int y1, int y2, void *pvData);

/** 控制背光
 * @param enable 是否使能背光
 * @return 无
 */
void vSt7789LcdBackLight(bool enable);

#ifdef __cplusplus
}
#endif

#endif
