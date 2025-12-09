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
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // 在字符串line中查找换行符'\n'的位置，并将其替换为字符串结束符'\0'
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

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

    //默认配置，速度20MHz,使用卡槽1
    sdmmc_host_t xHost = SDMMC_HOST_DEFAULT();

    //默认的IO管脚配置
    sdmmc_slot_config_t xSlotConfig = SDMMC_SLOT_CONFIG_DEFAULT();

    //4位数据
    xSlotConfig.width = 4;

    //不适用通过IO矩阵进行映射的管脚，只使用默认支持的SDMMC管脚，可以获得最大性能
    #if 0
    slot_config.clk = CONFIG_EXAMPLE_PIN_CLK;
    slot_config.cmd = CONFIG_EXAMPLE_PIN_CMD;
    slot_config.d0 = CONFIG_EXAMPLE_PIN_D0;
    slot_config.d1 = CONFIG_EXAMPLE_PIN_D1;
    slot_config.d2 = CONFIG_EXAMPLE_PIN_D2;
    slot_config.d3 = CONFIG_EXAMPLE_PIN_D3;
    #endif
    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    //管脚启用内部上拉
    xSlotConfig.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    xRet = esp_vfs_fat_sdmmc_mount(cMountPoint, &xHost, &xSlotConfig, &xMountConfig, &xCard);

    if (xRet != ESP_OK) {
        if (xRet == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. ");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(xRet));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, xCard);

    // Use POSIX and C standard library functions to work with files:

    // First create a file.
    const char *file_hello = MOUNT_POINT"/hello.txt";
    char data[EXAMPLE_MAX_CHAR_SIZE];
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", xCard->cid.name);
    xRet = prvExample_WriteFile(file_hello, data);
    if (xRet != ESP_OK) {
        return;
    }

    const char *file_foo = MOUNT_POINT"/foo.txt";
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        // Delete it if it exists
        unlink(file_foo);
    }

    // Rename original file
    ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return;
    }

    xRet = prvExample_ReadFile(file_foo);
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
    const char *file_nihao = MOUNT_POINT"/nihao.txt";
    memset(data, 0, EXAMPLE_MAX_CHAR_SIZE);
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", xCard->cid.name);
    xRet = prvExample_WriteFile(file_nihao, data);
    if (xRet != ESP_OK) {
        return;
    }

    //Open file for reading
    xRet = prvExample_ReadFile(file_nihao);
    if (xRet != ESP_OK) {
        return;
    }

    // All done, unmount partition and disable SDMMC peripheral
    esp_vfs_fat_sdcard_unmount(cMountPoint, xCard);
    ESP_LOGI(TAG, "Card unmounted");
}
