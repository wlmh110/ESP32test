/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "tcp_client.h"
#include "wifistation.h"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// #include "esp_system.h"
// #include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
// #include "protocol_examples_common.h"
// #include "addr_from_stdin.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

static const char *TAG = "tcpclient";
static const char *payload = "Message from ESP32 ";

static void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    // 根据 IPv4 或 IPv6 配置目标地址
#if defined(CONFIG_EXAMPLE_IPV4)
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;
#elif defined(CONFIG_EXAMPLE_IPV6)
    struct sockaddr_in6 dest_addr = {0};
    inet6_aton(host_ip, &dest_addr.sin6_addr);
    dest_addr.sin6_family = AF_INET6;
    dest_addr.sin6_port = htons(PORT);
    dest_addr.sin6_scope_id = esp_netif_get_netif_impl_index(EXAMPLE_INTERFACE);
    addr_family = AF_INET6;
    ip_protocol = IPPROTO_IPV6;
#elif defined(CONFIG_EXAMPLE_SOCKET_IP_INPUT_STDIN)
    struct sockaddr_storage dest_addr = {0};
    ESP_ERROR_CHECK(get_addr_from_stdin(PORT, SOCK_STREAM, &ip_protocol, &addr_family, &dest_addr));
#endif
    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "Successfully connected");

    // 发送音频格式
    char formate[] = "format:samplerate16000channel1bit16";
    ESP_LOGI(TAG, "开始发送数据格式");
    err = send(sock, formate, sizeof(formate), 0);
    if (err < 0)
    {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        vTaskDelete(NULL);
    }

    // 接收服务器的确认
    int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (len < 0)
    {
        ESP_LOGE(TAG, "recv failed: errno %d", errno);
        vTaskDelete(NULL);
    }
    rx_buffer[len] = 0; // Null-terminate whatever we received
    ESP_LOGI(TAG, "Received %d bytes from server: %s", len, rx_buffer);
    if (strcmp(rx_buffer, "OK") != 0)
    {
        ESP_LOGE(TAG, "Server didn't respond with OK after sending format");
        vTaskDelete(NULL);
    }
    char datatag[20];
    snprintf(datatag, sizeof(datatag), "data:");
    // 发送数据标签
    err = send(sock, datatag, strlen(datatag), 0);
    // 开始发送音频数据
    int send_count = 0;
    uint32_t total_data_count = 0;
    while (send_count < batch_count)
    {
        // 等待音频数据准备好
        while (!data_ready)
        {
            vTaskDelay(50 / portTICK_PERIOD_MS); // 等待数据准备完毕
        }

        ESP_LOGI(TAG, "开始发送数据");

        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending data tag: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "开始发送数据2");
        // 发送音频数据
        err = send(sock, audio_data_nvs, audio_data_count, 0);
        if (err < 0)
        {
            ESP_LOGE(TAG, "Error occurred during sending audio data: errno %d", errno);
            break;
        }
        total_data_count = total_data_count + audio_data_count;

        ESP_LOGI(TAG, "数据确认接受");
        data_ready = false;
        send_count++;
    }
    snprintf(datatag, sizeof(datatag), "totalcount:%d", total_data_count);
    // 发送数据标签
    err = send(sock, datatag, strlen(datatag), 0);

    ESP_LOGI(TAG, "等待服务器确认");
    // 接收服务器的确认
    len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (len < 0)
    {
        ESP_LOGE(TAG, "recv failed: errno %d", errno);
           vTaskDelete(NULL);
    }
    rx_buffer[len] = 0; // Null-terminate whatever we received
    ESP_LOGI(TAG, "Received %d bytes from server: %s", len, rx_buffer);
    if (strcmp(rx_buffer, "OK") != 0)
    {
        ESP_LOGE(TAG, "Server didn't respond with OK after sending data");
           vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "所有数据已发送，关闭连接");
    shutdown(sock, 0); // 关闭发送
    close(sock);       // 关闭socket

    vTaskDelete(NULL);
}

void tcpipinit(void)
{
    ESP_LOGI(TAG, "Starting TCPIP");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    wificonnect();

    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
}
