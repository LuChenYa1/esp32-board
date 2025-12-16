#include "cst816t_driver.h"
#include "driver/i2c.h"
//#include "driver/i2c_master.h"
#include "esp_log.h"

#define TOUCH_I2C_PORT  I2C_NUM_0

#define CST816T_ADDR    0x15

static const char *TAG = "cst816t";

/* 边界值 */ 
static uint16_t xUsLimitX = 0;
static uint16_t xUsLimitY = 0;

static esp_err_t prvI2CRead(uint8_t ucSlaveAddr, uint8_t ucRegisterAddr, uint8_t ucReadLength, uint8_t *pcDataBuffer);

/** CST816T初始化
 * @param pxConfig 配置
 * @return err
*/
esp_err_t xCst816tInit(Cst816tConfig_t* pxConfig)
{
    /* I2C初始化 */
    int iI2CMasterPort = TOUCH_I2C_PORT;
    i2c_config_t xI2CConfig = {
        .mode               = I2C_MODE_MASTER,    // I2C 模式
        .sda_io_num         = pxConfig->xSDA,     // SDA 引脚
        .scl_io_num         = pxConfig->xSCL,     // SCL 引脚
        .sda_pullup_en      = GPIO_PULLUP_ENABLE, // 引脚上拉
        .scl_pullup_en      = GPIO_PULLUP_ENABLE, // 引脚上拉
        .master.clk_speed   = pxConfig->ulFreq,   // 时钟频率
    };
    xUsLimitX = pxConfig->uiXLimit;
    xUsLimitY = pxConfig->uiYLimit;
    ESP_ERROR_CHECK(i2c_param_config(iI2CMasterPort, &xI2CConfig));
    ESP_ERROR_CHECK(i2c_driver_install(iI2CMasterPort, xI2CConfig.mode, 0, 0, 0));
    
    /* 检查 ID */ 
    uint8_t ucDataBuffer;
    prvI2CRead(CST816T_ADDR, 0xA7, 1, &ucDataBuffer);
    ESP_LOGI(TAG, "\tChip ID: 0x%02x", ucDataBuffer);
    /* 检查版本 */ 
    prvI2CRead(CST816T_ADDR, 0xA9, 1, &ucDataBuffer);
    ESP_LOGI(TAG, "\tFirmware version: 0x%02x", ucDataBuffer);
    /* 检查厂商 ID */ 
    prvI2CRead(CST816T_ADDR, 0xAA, 1, &ucDataBuffer);
    ESP_LOGI(TAG, "\tFactory ID: 0x%02x", ucDataBuffer);

    return ESP_OK;
}

/** 读取触摸点坐标值
 * @param  x x坐标
 * @param  y y坐标
 * @param iState 松手状态 0,松手 1按下
 * @return 无
*/
void vCst816tRead(int16_t *x, int16_t *y, int *iState)
{
    uint8_t ucDataX[2];            // 2 bytesX（ 0~65535 ）
    uint8_t ucDataY[2];            // 2 bytesY
    uint8_t ucTouchPointCount = 0; // 检测到的触摸点数量
    static int16_t iLastx = 0;     // 12bit pixel value( 上一次的坐标 )
    static int16_t iLastY = 0;     // 12bit pixel value

    /* 读取触摸点数量, 如果触摸点数量为0或者多个则返回0, 不支持多点触摸 */
    prvI2CRead(CST816T_ADDR, 0x02, 1, &ucTouchPointCount);
    if (ucTouchPointCount != 1) {  // ignore no touch & multi touch
        *x = iLastx;
        *y = iLastY;
        *iState = 0;
        return;
    }

    /* 读取 X 坐标 */
    prvI2CRead(CST816T_ADDR, 0x03, 2, ucDataX);
    /* 读取 Y 坐标 */
    prvI2CRead(CST816T_ADDR, 0x05, 2, ucDataY);
    /* 转换坐标 */
    int16_t iCurrentX = ((ucDataX[0] & 0x0F) << 8) | (ucDataX[1] & 0xFF);
    int16_t iCurrentY = ((ucDataY[0] & 0x0F) << 8) | (ucDataY[1] & 0xFF);

    if(iLastx != iCurrentX || iCurrentY != iLastY)
    {
        iLastx = iCurrentX;
        iLastY = iCurrentY;
        //ESP_LOGI(TAG,"touch x:%d,y:%d",iLastx,iLastY);
    }
    /* 限制坐标 */
    if(iLastx >= xUsLimitX)
        iLastx = xUsLimitX - 1;
    if(iLastY >= xUsLimitY)
        iLastY = xUsLimitY - 1;
    /* 返回坐标 */
    *x = iLastx;
    *y = iLastY;
    *iState = 1;
}

/** 根据寄存器地址读取N字节
 * @param ucSlaveAddr 器件地址
 * @param ucRegisterAddr 寄存器地址
 * @param ucReadLength  要读取的数据长度
 * @param pcDataBuffer 数据
 * @return err
*/
static esp_err_t prvI2CRead(uint8_t ucSlaveAddr, uint8_t ucRegisterAddr, uint8_t ucReadLength, uint8_t *pcDataBuffer) 
{
    i2c_cmd_handle_t xI2CCmd = i2c_cmd_link_create();
    if(!xI2CCmd)
    {
        ESP_LOGE(TAG, "Error xI2CCmd creat fail!");
        return ESP_FAIL;
    }
    i2c_master_start(xI2CCmd);
    i2c_master_write_byte(xI2CCmd, (ucSlaveAddr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(xI2CCmd, ucRegisterAddr, I2C_MASTER_ACK);

    i2c_master_start(xI2CCmd);
    i2c_master_write_byte(xI2CCmd, (ucSlaveAddr << 1) | I2C_MASTER_READ, true);
    for(int i = 0;i < ucReadLength;i++)
    {
        if(i == ucReadLength - 1)
            i2c_master_read_byte(xI2CCmd, &pcDataBuffer[i], I2C_MASTER_NACK);
        else
            i2c_master_read_byte(xI2CCmd, &pcDataBuffer[i], I2C_MASTER_ACK);
    }
    i2c_master_stop(xI2CCmd);

    /* 确保以上步骤执行完毕 */
    esp_err_t xRet = i2c_master_cmd_begin(TOUCH_I2C_PORT, xI2CCmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(xI2CCmd);
    return xRet;
}
