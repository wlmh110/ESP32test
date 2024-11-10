#include <stdio.h>
#include <string.h>
#include "i2s_record.h"
#include "rgb_led_control.h"
#include "key.h"
#include "tcp_client.h"
#include "wifistation.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "driver/i2s.h"

static const char *TAG = "I2S_RECORD_APP";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS 存储区损坏或版本不匹配，重置存储区
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_LOGI(TAG, "PDM microphone recording example start");
    // Mount the SDCard for recording the audio file
    mount_sdcard();
    key_init();
    rgb_led_init();
    clear_nvs_data(AUDIO_NAMESPACE);
    // 设置绿色
    rgb_led_set_color(0, LEDC_DUTY_MAX, 0, 500);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    rgb_led_set_color(0, 0, 0, 100);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Init the PDM digital microphone
    init_microphone();
    ESP_LOGI(TAG, "Starting recording for %d seconds!", 10);
    // record_wav(30);
    // Start recording for 30 seconds
    // record_audio_start();
    // 设置蓝色
    

    // record_audio_stop();

    
    // Stop I2S driver and destroy
    // ESP_ERROR_CHECK(i2s_driver_uninstall(0));
    // tcpipinit();
    
    // get_audio_data();
}
