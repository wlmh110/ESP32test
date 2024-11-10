/* I2S Digital Microphone Recording Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "i2s_record.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdkconfig.h"

#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "I2SRECORD";

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdmmc_card_t *card;
bool recording = false;  // 定义并初始化变量
bool data_ready = false; // 数据是否准备好
char *audio_data_nvs = NULL;
size_t audio_data_count;
static int16_t i2s_readraw_buff[SAMPLE_SIZE];
size_t bytes_read;
uint32_t batch_count = 0;
const int WAVE_HEADER_SIZE = 44;

static volatile bool save_audio_flag = false;
static uint8_t *buffer_1 = NULL;
static uint8_t *buffer_2 = NULL;
static uint8_t *current_buffer = NULL;
static uint8_t *next_buffer = NULL;
static TaskHandle_t record_task_handle = NULL;
static TaskHandle_t save_task_handle = NULL;

void mount_sdcard(void)
{
    esp_err_t ret;
    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 8 * 1024};
    ESP_LOGI(TAG, "Initializing SD card");

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SPI_MOSI_GPIO,
        .miso_io_num = SPI_MISO_GPIO,
        .sclk_io_num = SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SPI_CS_GPIO;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void generate_wav_header(char *wav_header, uint32_t wav_size, uint32_t sample_rate)
{

    // See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
    uint32_t file_size = wav_size + WAVE_HEADER_SIZE - 8;
    uint32_t byte_rate = BYTE_RATE;

    const char set_wav_header[] = {
        'R', 'I', 'F', 'F',                                                  // ChunkID
        file_size, file_size >> 8, file_size >> 16, file_size >> 24,         // ChunkSize
        'W', 'A', 'V', 'E',                                                  // Format
        'f', 'm', 't', ' ',                                                  // Subchunk1ID
        0x10, 0x00, 0x00, 0x00,                                              // Subchunk1Size (16 for PCM)
        0x01, 0x00,                                                          // AudioFormat (1 for PCM)
        0x01, 0x00,                                                          // NumChannels (1 channel)
        sample_rate, sample_rate >> 8, sample_rate >> 16, sample_rate >> 24, // SampleRate
        byte_rate, byte_rate >> 8, byte_rate >> 16, byte_rate >> 24,         // ByteRate (SampleRate * NumChannels * BitsPerSample/8)
        0x02, 0x00,                                                          // BlockAlign (NumChannels * BitsPerSample/8)
        0x10, 0x00,                                                          // BitsPerSample (16 bits)
        'd', 'a', 't', 'a',                                                  // Subchunk2ID
        wav_size, wav_size >> 8, wav_size >> 16, wav_size >> 24,             // Subchunk2Size (size of audio data)
    };

    memcpy(wav_header, set_wav_header, sizeof(set_wav_header));
}

void record_wav(uint32_t rec_time)
{
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    ESP_LOGI(TAG, "Opening file");

    char wav_header_fmt[WAVE_HEADER_SIZE];

    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    generate_wav_header(wav_header_fmt, flash_rec_time, SAMPLE_RATE);

    // First check if file exists before creating a new file.
    struct stat st;
    if (stat(SD_MOUNT_POINT "/record.wav", &st) == 0)
    {
        // Delete it if it exists
        unlink(SD_MOUNT_POINT "/record.wav");
    }

    // Create new WAV file
    FILE *f = fopen(SD_MOUNT_POINT "/record.wav", "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    fwrite(wav_header_fmt, 1, WAVE_HEADER_SIZE, f);

    // Start recording
    while (flash_wr_size < flash_rec_time)
    {
        // Read the RAW samples from the microphone
        i2s_read(0, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 100);
        // Write the samples to the WAV file
        fwrite(i2s_readraw_buff, 1, bytes_read, f);
        flash_wr_size += bytes_read;
    }

    ESP_LOGI(TAG, "Recording done!");
    fclose(f);
    ESP_LOGI(TAG, "File written on SDCard");

    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "Card unmounted");
    // Deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}

esp_err_t get_batch_count_from_nvs(uint32_t batch_count)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 打开 NVS 命名空间
    err = nvs_open(AUDIO_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // 获取批次数量
    err = nvs_get_u32(my_handle, "batch_count", &batch_count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(my_handle);
        return err;
    }

    // 关闭 NVS
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t set_batch_count_in_nvs(uint32_t batch_count)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 打开 NVS 命名空间
    err = nvs_open(AUDIO_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // 增加批次数量并保存
    err = nvs_set_u32(my_handle, "batch_count", batch_count);
    if (err != ESP_OK)
    {
        nvs_close(my_handle);
        return err;
    }

    // 提交写入
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        nvs_close(my_handle);
        return err;
    }

    // 关闭 NVS
    nvs_close(my_handle);
    return ESP_OK;
}

uint32_t flash_wr_size = 0; // 累计录音数据的字节数
FILE *f = NULL;             // 文件指针

void record_audio_task(void *arg)
{
    esp_err_t err;

    // 分配两个缓冲区
    buffer_1 = malloc(SAMPLE_SIZE);
    buffer_2 = malloc(SAMPLE_SIZE);

    if (buffer_1 == NULL || buffer_2 == NULL)
    {
        ESP_LOGE("Record", "Failed to allocate memory for audio data.");
        vTaskDelete(NULL);
        return;
    }

    // 初始化缓冲区
    current_buffer = buffer_1;
    next_buffer = buffer_2;

    int64_t start_time = esp_timer_get_time();
    int64_t end_time = esp_timer_get_time();

    // 读取 I2S 数据并切换缓冲区
    while (recording)
    {
        start_time = esp_timer_get_time();
        i2s_read(I2S_NUM, (char *)current_buffer, SAMPLE_SIZE, &bytes_read, 100); // 读取 I2S 数据
        end_time = esp_timer_get_time();
        ESP_LOGI("TIME", "代码运行时长：%lld 毫秒", (end_time - start_time) / 1000);
        if (bytes_read > 0)
        {

            // 切换缓冲区
            uint8_t *temp = current_buffer;
            current_buffer = next_buffer;
            next_buffer = temp;

            // 标记数据已经准备好保存
            save_audio_flag = true;
            // 更新批次号
            batch_count++;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS); // 延时以避免 CPU 占用过高
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); // 等待保存完毕
    while (save_audio_flag)
    {
        vTaskDelay(50 / portTICK_PERIOD_MS); // 等待保存完毕
    }
    vTaskDelete(save_task_handle); // 删除任务
    free(buffer_1);
    free(buffer_2);
    record_task_handle = NULL;
    vTaskDelete(NULL); // 删除任务
}

void save_audio_to_nvs_task(void *arg)
{
    esp_err_t err;
    nvs_handle_t my_handle;

    while (1)
    {
        if (save_audio_flag)
        {
            // 保存数据到 NVS
            err = nvs_open(AUDIO_NAMESPACE, NVS_READWRITE, &my_handle);
            if (err != ESP_OK)
            {
                ESP_LOGE("Save", "Failed to open NVS: %s", esp_err_to_name(err));
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
            }

            char key_name[32];
            snprintf(key_name, sizeof(key_name), "audio_data_%d", batch_count - 1);

            uint8_t *audio_data_to_save = next_buffer;

            err = nvs_set_blob(my_handle, key_name, audio_data_to_save, SAMPLE_SIZE);
            if (err != ESP_OK)
            {
                ESP_LOGE("Save", "Failed to save audio data: %s", esp_err_to_name(err));
            }
            else
            {
                err = nvs_commit(my_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE("Save", "Failed to commit NVS: %s", esp_err_to_name(err));
                }
            }

            nvs_close(my_handle);
            save_audio_flag = false; // 清除标志位
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // 适当延时以避免占用过多 CPU
    }
}

// 启动录音
void record_audio_start()
{
    flash_wr_size = 0;
    ESP_LOGI(TAG, "Starting audio recording...");

    recording = true;

    // 创建录音任务
    xTaskCreate(record_audio_task, "record_audio_task", 2048, NULL, 5, &record_task_handle);

    // 创建保存任务
    xTaskCreate(save_audio_to_nvs_task, "save_audio_to_nvs_task", 2048, NULL, 5, &save_task_handle);
}

// 停止录音
void record_audio_stop()
{
    if (recording)
    {
        recording = false; // 停止录音任务

        while (record_task_handle != NULL)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS); // 等待任务完成
        }

        esp_err_t err = set_batch_count_in_nvs(batch_count);
        ESP_LOGI(TAG, "Recording stopped. Total data written: %d bytes", flash_wr_size);
    }
}
esp_err_t get_audio_data(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 打开 NVS 命名空间
    err = nvs_open(AUDIO_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_u32(my_handle, "batch_count", &batch_count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_close(my_handle);
        return err;
    }

    // 如果没有批次数据
    if (batch_count == 0)
    {
        printf("No audio data saved yet!\n");
        nvs_close(my_handle);
        return ESP_OK;
    }

    printf("Batch count: %d\n", batch_count);

    // 遍历所有批次，读取每个批次的数据
    for (uint32_t i = 0; i < batch_count; i++)
    {
        while (data_ready)
        {
            vTaskDelay(50 / portTICK_PERIOD_MS); // 等待数据被发送
        }
        ESP_LOGI(TAG, "开始提取");
        if (audio_data_nvs != NULL)
            free(audio_data_nvs); // 释放分配的内存
        char key_name[32];
        snprintf(key_name, sizeof(key_name), "audio_data_%d", i); // 构建批次的键名

        err = nvs_get_blob(my_handle, key_name, NULL, &audio_data_count);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        {
            printf("Failed to get size for key %s\n", key_name);
            continue; // 跳过这个批次，继续处理下一个
        }

        // 打印当前批次的大小
        printf("Batch %d size: %d bytes\n", i, audio_data_count);

        if (audio_data_count == 0)
        {
            printf("No data found for batch %d\n", i);
            continue;
        }

        // 分配内存并读取数据
        audio_data_nvs = malloc(audio_data_count);
        if (audio_data_nvs == NULL)
        {
            printf("Memory allocation failed for batch %d\n", i);
            continue; // 跳过当前批次，继续处理下一个
        }

        err = nvs_get_blob(my_handle, key_name, audio_data_nvs, &audio_data_count);
        if (err != ESP_OK)
        {
            free(audio_data_nvs);
            printf("Failed to read data for batch %d\n", i);
            continue; // 跳过当前批次，继续处理下一个
        }
        ESP_LOGI(TAG, "提取完毕");
        data_ready = true;
        // 这里你可以根据需求处理或打印数据
        // 打印前 20 个字节的数据（假设是 uint32_t 数据）
        printf("Data for batch %d (first 20 bytes):\n", i);
        for (int j = 0; j < 20 && j < audio_data_count / sizeof(uint32_t); j++) {
            printf("%d: %d\n", j + 1, ((uint32_t*)audio_data_nvs)[j]);
        }
    }
    free(audio_data_nvs); // 释放分配的内存
    // 关闭 NVS
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t clear_nvs_data(const char *namespace)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 打开指定的 NVS 命名空间
    err = nvs_open(namespace, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return err; // 如果打开失败，返回错误
    }

    // 删除所有键
    err = nvs_erase_all(my_handle);
    if (err != ESP_OK)
    {
        nvs_close(my_handle); // 确保关闭句柄
        return err;           // 删除失败，返回错误
    }

    // 提交删除操作
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        nvs_close(my_handle); // 确保关闭句柄
        return err;           // 提交失败，返回错误
    }

    // 关闭 NVS 句柄
    nvs_close(my_handle);
    return ESP_OK; // 删除操作成功
}

void init_microphone(void)
{
    // Set the I2S configuration as PDM and 16bits per sample
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BIT_SAMPLE,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 8,
        .dma_buf_len = 800,
        .use_apll = 0,
    };

    // Set the pinout configuration (set using menuconfig)
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = I2S_BCK_GPIO,
        .ws_io_num = I2S_WS_GPIO,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_GPIO,
    };

    // Call driver installation function before any I2S R/W operation.
    ESP_ERROR_CHECK(i2s_driver_install(0, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(0, &pin_config));
    ESP_ERROR_CHECK(i2s_set_clk(0, SAMPLE_RATE, BIT_SAMPLE, I2S_CHANNEL_MONO));
}
