#ifndef _CST816T_DRIVER_H_
#define _CST816T_DRIVER_H_

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CST816T 触摸IC驱动 */

typedef struct
{
    gpio_num_t xSCL;   // SCL管脚
    gpio_num_t xSDA;   // SDA管脚
    uint32_t ulFreq;   // I2C速率
    uint16_t uiXLimit; // X方向触摸边界
    uint16_t uiYLimit; // y方向触摸边界
} Cst816tConfig_t;

/** CST816T初始化
 * @param cfg 配置
 * @return err
 */
esp_err_t xCst816tInit(Cst816tConfig_t *cfg);

/** 读取坐标值
 * @param  x x坐标
 * @param  y y坐标
 * @param state 松手状态 0,松手 1按下
 * @return 无
 */
void vCst816tRead(int16_t *x, int16_t *y, int *state);

#ifdef __cplusplus
}
#endif

#endif
