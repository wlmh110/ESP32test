#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdint.h>
#include "esp_netif.h"

#define HOST_IP_ADDR "192.168.1.10"  // Can be redefined in the main application
#define CONFIG_EXAMPLE_IPV4

#define PORT 31333

extern bool data_ready;
extern char *audio_data_nvs;

extern size_t audio_data_count;
extern uint32_t batch_count;
static void tcp_client_task(void *pvParameters);

void tcpipinit(void);

#endif