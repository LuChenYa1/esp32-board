/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// This example uses SDMMC peripheral to communicate with SD card.

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

/* 
    问题: main.c 头文件报红, 但执行 ESP-IDF: Add VS Code Configuration Folder 没有作用
    解决方法:
    1、按下 Ctrl+Shift+P, 执行 Preferences: Open Workspace Settings (JSON)
    2、在打开的文件中, 确保添加这一行: "idf.espIdfPath": "/home/你的用户名/esp/esp-idf"
    3、同样的, 在 .vscode 的setting.json 中, 重复第2步, 并添加额外配置:
        // 确保CMake Tools能获取到正确的环境变量
        "cmake.autoSelectActiveFolder": false,
        // 防止C/C++扩展自动覆盖配置，是核心设置
        "C_Cpp.autocomplete": "disabled",
        "C_Cpp.errorSquiggles": "disabled",
        "C_Cpp.configurationWarnings": "disabled"
    4、打开 .vscode/c_cpp_properties.json, 于 "configurations": [] 之前, 添加:
        "enableConfigurationSquiggles": false, // 禁用配置警告
        目的: 锁定C/C++配置文件, 阻止其他扩展自动修改智能感知配置文件
        该步骤消除了 c_cpp_properties.json 报错
    5、在命令面板中执行 ESP-IDF: Add VS Code Configuration Folder 或 C/C++: Rescan Workspace
 */

#define EXAMPLE_MAX_CHAR_SIZE    64

static const char *TAG = "example";

#define MOUNT_POINT "/sdcard"   //挂载点名称

/* 向 micro sd 卡写入数据*/
static esp_err_t prvExample_WriteFile(const char *pcPath, char *pcData)
{
    ESP_LOGI(TAG, "Opening file %s", pcPath);
    FILE *f = fopen(pcPath, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, pcData);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

/* 从 micro sd 卡读取数据 */ 
static esp_err_t prvExample_ReadFile(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char cLine[EXAMPLE_MAX_CHAR_SIZE];
    fgets(cLine, sizeof(cLine), f);
    fclose(f);

    /* 在字符串line中查找换行符'\n'的位置，并将其替换为字符串结束符'\0' */ 
    char *pos = strchr(cLine, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", cLine);

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t xRet;
    esp_vfs_fat_sdmmc_mount_config_t xMountConfig = {
        .format_if_mount_failed = true,     //挂载失败是否执行格式化
        .max_files = 5,                     //最大可打开文件数
        .allocation_unit_size = 16 * 1024   //执行格式化时的分配单元大小（分配单元越大，读写越快）
    };
    sdmmc_card_t *xCard;
    const char cMountPoint[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");

    /* 默认配置，速度20MHz,使用卡槽1 */
    sdmmc_host_t xHost = SDMMC_HOST_DEFAULT();

    /* 默认的IO管脚配置 */
    sdmmc_slot_config_t xSlotConfig = SDMMC_SLOT_CONFIG_DEFAULT();
    /* 4位数据 */
    xSlotConfig.width = 4;
    /*
     * 启用已启用引脚的内部上拉电阻。注意内部上拉电阻强度不足，
     * 请确保在总线上连接了10k外部上拉电阻。此函数仅用于调试/示例目的。
     */
    xSlotConfig.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    /* 不适用通过IO矩阵进行映射的管脚，只使用默认支持的SDMMC管脚，可以获得最大性能 */
    #if 0
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
    #endif

    ESP_LOGI(TAG, "Mounting filesystem");// 挂载文件系统
    xRet = esp_vfs_fat_sdmmc_mount(cMountPoint, &xHost, &xSlotConfig, &xMountConfig, &xCard);

    /* 
     * 检查文件系统初始化结果并处理错误情况
     * 
     * 该代码块用于检查SD卡文件系统的初始化状态，根据不同的错误码输出相应的
     * 错误信息到日志系统，帮助开发者快速定位问题原因。
     */
    if (xRet != ESP_OK) {
        if (xRet == ESP_FAIL) {
            /* 文件系统挂载失败，通常是由于文件系统损坏或不兼容导致 */
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
        } else {
            /* SD卡初始化失败，打印错误信息，提示检查硬件连接和上拉电阻配置 */
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                        "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(xRet));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    /* sd 卡已经初始化，打印它的属性信息 */ 
    sdmmc_card_print_info(stdout, xCard);

    /* 使用POSIX和C标准库函数处理文件 */ 

    /* 创建文件并写入数据 */ 
    const char *pcFileHello = MOUNT_POINT"/hello.txt";// "挂载点名称""文件名"
    char cData[EXAMPLE_MAX_CHAR_SIZE];
    snprintf(cData, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", xCard->cid.name);
    xRet = prvExample_WriteFile(pcFileHello, cData);
    if (xRet != ESP_OK) {
        return;
    }

    /* 重命名文件 */
    const char *pcFileNew = MOUNT_POINT"/foo.txt";/* 新名称*/
    struct stat xState;
    /* 检查文件名是否已经存在，如果存在则删除该文件 */
    if (stat(pcFileNew, &xState) == 0) {
        unlink(pcFileNew);
    }
    ESP_LOGI(TAG, "Renaming file %s to %s", pcFileHello, pcFileNew);
    if (rename(pcFileHello, pcFileNew) != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    /* 读取 sd 卡文件数据 */
    xRet = prvExample_ReadFile(pcFileNew);
    if (xRet != ESP_OK) {
        return;
    }

    // Format FATFS
    #if 0
    ret = esp_vfs_fat_sdcard_format(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        return;
    }

    if (stat(file_foo, &st) == 0) {
        ESP_LOGI(TAG, "file still exists");
        return;
    } else {
        ESP_LOGI(TAG, "file doesnt exist, format done");
    }
    #endif

    /* 创建新文件并写入、读取数据 */
    const char *pcFile_NiHao = MOUNT_POINT"/nihao.txt";
    memset(cData, 0, EXAMPLE_MAX_CHAR_SIZE);/* 清空数据 */
    snprintf(cData, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", xCard->cid.name);
    xRet = prvExample_WriteFile(pcFile_NiHao, cData);
    if (xRet != ESP_OK) {
        return;
    }
    xRet = prvExample_ReadFile(pcFile_NiHao);
    if (xRet != ESP_OK) {
        return;
    }

    // All done, unmount partition and disable SDMMC peripheral
    esp_vfs_fat_sdcard_unmount(cMountPoint, xCard);
    ESP_LOGI(TAG, "Card unmounted");
}
