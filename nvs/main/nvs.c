#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

/** 写入值到 NVS 中（字符数据）
 * @param namespace NVS 命名空间
 * @param key NVS 键值
 * @param value 需要写入的值
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xWriteNvsStr(const char *namespace, const char *key, const char *value)
{
    nvs_handle_t xNvsHandle;
    esp_err_t ret;
    /* 打开命名空间 */
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &xNvsHandle));
    /* 向命名空间写入键值对 */
    ret = nvs_set_str(xNvsHandle, key, value);
    /* 提交数据, 确保数据立刻写入, 否则会延迟写入 */
    nvs_commit(xNvsHandle);
    /* 关闭命名空间 */
    nvs_close(xNvsHandle);
    return ret;
}


 /**
 * @brief 将二进制数据写入 NVS 存储中指定的命名空间和键
 * 
 * @param namespace NVS 命名空间名称
 * @param key 要写入数据的键名
 * @param value 指向要写入的二进制数据的指针
 * @param len 要写入数据的长度
 * @return esp_err_t ESP_OK 表示成功，其他值表示失败
 */
esp_err_t xWriteNvsBlob(const char *namespace, const char *key, uint8_t *value, size_t len)
{
    nvs_handle_t xNvsHandle;
    esp_err_t ret;
    // 打开指定命名空间的 NVS 句柄，以读写模式打开
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &xNvsHandle));
    // 设置 blob 类型的数据到 NVS 中
    ret = nvs_set_blob(xNvsHandle, key, value, len);
    // 提交更改并关闭 NVS 句柄
    nvs_commit(xNvsHandle);
    nvs_close(xNvsHandle);
    return ret;
}


/** 从 NVS 中读取字符值
 * @param namespace NVS 命名空间
 * @param key 要读取的键
 * @param value 读到的值
 * @param maxlen 外部存储数组的最大值
 * @return 读取到的字节数
 */
size_t xReadNvsStr(const char *namespace, const char *key, char *value, int maxlen)
{
    nvs_handle_t xNvsHandle;
    esp_err_t xRetValue = ESP_FAIL;
    size_t xRequiredSize = 0; // 存储读取的字节数
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &xNvsHandle));
    xRetValue = nvs_get_str(xNvsHandle, key, NULL, &xRequiredSize);
    if (xRetValue == ESP_OK && xRequiredSize <= maxlen)
        nvs_get_str(xNvsHandle, key, value, &xRequiredSize);
    else
        xRequiredSize = 0;
    nvs_close(xNvsHandle);
    return xRequiredSize;
}


/** 从 NVS 中读取字节数据（二进制）
 * @param namespace NVS 命名空间
 * @param key 要读取的键值
 * @param value 读到的值
 * @param maxlen 外部存储数组的最大值
 * @return 读取到的字节数
 */
size_t xReadNvsBlob(const char *namespace, const char *key, uint8_t *value, int maxlen)
{
    nvs_handle_t xNvsHandle;
    esp_err_t xRetValue = ESP_FAIL;
    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &xNvsHandle));
    xRetValue = nvs_get_blob(xNvsHandle, key, NULL, &required_size);
    if (xRetValue == ESP_OK && required_size <= maxlen)
        nvs_get_blob(xNvsHandle, key, value, &required_size);
    else
        required_size = 0;
    nvs_close(xNvsHandle);
    return required_size;
}


/** 擦除 NVS 区中某个键
 * @param namespace NVS 命名空间
 * @param key 要读取的键值
 * @return 错误值
 */
esp_err_t xEraseNvsKey(const char *namespace, const char *key)
{
    nvs_handle_t xNvsHandle;
    esp_err_t xRetValue = ESP_FAIL;
    ESP_ERROR_CHECK(nvs_open(namespace, NVS_READWRITE, &xNvsHandle));
    xRetValue = nvs_erase_key(xNvsHandle, key);
    xRetValue = nvs_commit(xNvsHandle);
    nvs_close(xNvsHandle);
    return xRetValue;
}
