#ifndef RGB_LED_CONTROL_H
#define RGB_LED_CONTROL_H

#include <stdint.h>

// RGB LED GPIO定义
#define LEDC_G_GPIO 21
#define LEDC_B_GPIO 22
#define LEDC_R_GPIO 23

// LEDC通道、定时器和频率设置
#define LEDC_R_CHANNEL LEDC_CHANNEL_0
#define LEDC_G_CHANNEL LEDC_CHANNEL_1
#define LEDC_B_CHANNEL LEDC_CHANNEL_2
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_DUTY_MAX 8191  // PWM最大占空比（2^13-1）
#define LEDC_FADE_TIME 1000 // 渐变时间，单位ms
#define LEDC_FREQ_HZ 5000   // PWM频率，单位Hz

// 初始化RGB LED配置
void rgb_led_init(void);

// 设置RGB颜色，red、green、blue取值范围为0~8191
void rgb_led_set_color(uint32_t red, uint32_t green, uint32_t blue, uint32_t fadetime);

#endif // RGB_LED_CONTROL_H
