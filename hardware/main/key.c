#include "key.h"
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "rgb_led_control.h"
#include "i2s_record.h"
#include "esp_timer.h"  // 用于时间戳

static xQueueHandle gpio_evt_queue = NULL;
static uint64_t last_isr_time = 0;  // 记录上次中断的时间戳
static void key_handle(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // 检查按键状态
            if (gpio_get_level(GPIO_KEY_INPUT) == 0) { // 按下
                printf("Button Pressed!\n");
                record_audio_start();
                get_batch_count_from_nvs(batch_count);
                rgb_led_set_color(0, 0, LEDC_DUTY_MAX, 500);
            } else { // 放松
                printf("Button Released!\n");
                record_audio_stop();
                rgb_led_set_color(0, 0, 0, 500);
            }
        }
    }
}

// 中断处理程序，带防抖动
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    uint64_t current_time = esp_timer_get_time() / 1000;  // 当前时间戳，单位为毫秒

    // 防抖动检查
    if (current_time - last_isr_time > DEBOUNCE_DELAY_MS) {
        last_isr_time = current_time;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
}



void key_init(void)
{
    // 初始化 GPIO 配置
    gpio_config_t io_conf = {};
    // 禁用中断
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // 设置为输入模式
    io_conf.mode = GPIO_MODE_INPUT;
    // 设置为按键输入 GPIO
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // 启用上拉电阻
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // 设置中断类型为任意边缘
    gpio_set_intr_type(GPIO_KEY_INPUT, GPIO_INTR_ANYEDGE);

    // 创建一个队列以处理来自 ISR 的 GPIO 事件
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    // 启动 GPIO 任务
    xTaskCreate(key_handle, "key_handle", 2048, NULL, 10, NULL);

    // 安装 GPIO ISR 服务
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // 为特定 GPIO 针脚挂接 ISR 处理程序
    gpio_isr_handler_add(GPIO_KEY_INPUT, gpio_isr_handler, (void*) GPIO_KEY_INPUT);

    printf("Button example initialized.\n");
}
