#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "rgb_led_control.h"



// 初始化RGB LED配置
void rgb_led_init(void) {
    // 配置LEDC定时器
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = LEDC_FREQ_HZ,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    // 配置LEDC通道
    ledc_channel_config_t ledc_channel[3] = {
        {
            .channel = LEDC_R_CHANNEL,
            .duty = 0,
            .gpio_num = LEDC_R_GPIO,
            .speed_mode = LEDC_MODE,
            .hpoint = 0,
            .timer_sel = LEDC_TIMER,
        },
        {
            .channel = LEDC_G_CHANNEL,
            .duty = 0,
            .gpio_num = LEDC_G_GPIO,
            .speed_mode = LEDC_MODE,
            .hpoint = 0,
            .timer_sel = LEDC_TIMER,
        },
        {
            .channel = LEDC_B_CHANNEL,
            .duty = 0,
            .gpio_num = LEDC_B_GPIO,
            .speed_mode = LEDC_MODE,
            .hpoint = 0,
            .timer_sel = LEDC_TIMER,
        }
    };

    // 初始化每个通道
    for (int i = 0; i < 3; i++) {
        ledc_channel_config(&ledc_channel[i]);
    }

    // 安装渐变功能
    ledc_fade_func_install(0);
}

// 设置RGB颜色
void rgb_led_set_color(uint32_t red, uint32_t green, uint32_t blue, uint32_t fadetime) {
    ledc_set_fade_with_time(LEDC_MODE, LEDC_R_CHANNEL, red, fadetime);
    ledc_fade_start(LEDC_MODE, LEDC_R_CHANNEL, LEDC_FADE_NO_WAIT);

    ledc_set_fade_with_time(LEDC_MODE, LEDC_G_CHANNEL, green, fadetime);
    ledc_fade_start(LEDC_MODE, LEDC_G_CHANNEL, LEDC_FADE_NO_WAIT);

    ledc_set_fade_with_time(LEDC_MODE, LEDC_B_CHANNEL, blue, fadetime);
    ledc_fade_start(LEDC_MODE, LEDC_B_CHANNEL, LEDC_FADE_NO_WAIT);
}
