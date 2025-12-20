#ifndef _DHT11_H_
#define _DHT11_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** DHT11初始化
 * @param xDht11Pin GPIO引脚
 * @return 无
 */
void vDht11Init(uint8_t xDht11Pin);

/** 获取DHT11数据
 * @param piTempX10 温度值X10
 * @param piHumidity 湿度值
 * @return 无
 */
int iDht11StartGet(int *piTempX10, int *piHumidity);

#ifdef __cplusplus
}
#endif

#endif
