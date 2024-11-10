#ifndef _KEY_H_
#define _KEY_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#define GPIO_KEY_INPUT   34 // 按键输入 GPIO
#define GPIO_INPUT_PIN_SEL ((1ULL<<GPIO_KEY_INPUT))
#define ESP_INTR_FLAG_DEFAULT 0
#define DEBOUNCE_DELAY_MS  50  // 防抖延迟（毫秒）

// 函数声明
void key_init(void);

#endif