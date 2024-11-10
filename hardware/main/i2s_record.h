#ifndef I2S_RECORD_H
#define I2S_RECORD_H

#include <stdint.h>
#include "sdmmc_cmd.h"
#include "esp_err.h"

// Constants
#define BIT_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define SPI_DMA_CHAN SPI_DMA_CH_AUTO
#define SAMPLE_RATE 16000
#define NUM_CHANNELS (1) // For mono recording only!
#define SD_MOUNT_POINT "/sdcard"
#define SAMPLE_SIZE BYTE_RATE // 每次读取和写入NVS的数据大小
#define BYTE_RATE (SAMPLE_RATE * (BIT_SAMPLE / 8)) * NUM_CHANNELS
#define SPI_MOSI_GPIO 14
#define SPI_MISO_GPIO 4
#define SPI_SCLK_GPIO 18
#define SPI_CS_GPIO 19

#define I2S_DATA_GPIO 32
#define I2S_BCK_GPIO 33
#define I2S_WS_GPIO 27
#define I2S_NUM I2S_NUM_0 // I2S设备号
extern bool recording;    // 指示录音任务是否运行中
extern bool data_ready;
extern uint32_t batch_count;
#define AUDIO_NAMESPACE "audio_data" // NVS 存储的命名空间

extern const int WAVE_HEADER_SIZE;

// Functions
void mount_sdcard(void);
void generate_wav_header(char *wav_header, uint32_t wav_size, uint32_t sample_rate);
// void record_wav(uint32_t rec_time);
// void record_wav_start(void);
// void record_wav_stop(void);
void record_wav(uint32_t rec_time);
void record_audio_start(void);
void record_audio_stop(void);
void init_microphone(void);
esp_err_t get_audio_data(void);
esp_err_t clear_nvs_data(const char *namespace);
esp_err_t get_batch_count_from_nvs(uint32_t batch_count);
#endif // I2S_RECORD_H
