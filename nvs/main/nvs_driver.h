#ifndef _ST7789_DRIVER_H_
#define _ST7789_DRIVER_H_
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t xWriteNvsStr(const char *namespace, const char *key, const char *value);
esp_err_t xWriteNvsBlob(const char *namespace, const char *key, uint8_t *value, size_t len);
size_t xReadNvsStr(const char *namespace, const char *key, char *value, int maxlen);
size_t xReadNvsBlob(const char *namespace, const char *key, uint8_t *value, int maxlen);


#ifdef __cplusplus
}
#endif

#endif
