/*
 * SPDX-FileCopyrightText: 2026 ESP32-S3 USB DAC Project
 * SPDX-License-Identifier: MIT
 */

#include "dns_captive.h"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "esp_log.h"

static const char *TAG = "DNS_CAPTIVE";

static int s_sock = -1;
static TaskHandle_t s_dns_task = NULL;
static volatile bool s_running = false;

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

static void dns_server_task(void *pvParameters)
{
    uint8_t rx_buffer[256];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "Unable to create DNS socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);

    int err = bind(s_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "DNS socket unable to bind: errno %d", errno);
        close(s_sock);
        s_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(s_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    ESP_LOGI(TAG, "Captive Portal DNS Server listening on UDP port 53");

    while (s_running) {
        client_addr_len = sizeof(client_addr);
        int len = recvfrom(s_sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < (int)sizeof(dns_header_t)) {
            continue;
        }

        dns_header_t *hdr = (dns_header_t *)rx_buffer;
        // Reply with 192.168.4.1 for all queries
        hdr->flags = htons(0x8180); // Standard response, No error
        hdr->ancount = htons(1);
        hdr->nscount = 0;
        hdr->arcount = 0;

        // Skip question section to append answer
        int q_end = sizeof(dns_header_t);
        while (q_end < len && rx_buffer[q_end] != 0) {
            q_end += 1 + rx_buffer[q_end];
        }
        q_end += 5; // null byte + 2 bytes type + 2 bytes class

        if (q_end + 16 > sizeof(rx_buffer)) continue;

        // Answer Record
        uint8_t *ans = &rx_buffer[q_end];
        ans[0] = 0xC0; ans[1] = 0x0C; // Pointer to domain name in query
        ans[2] = 0x00; ans[3] = 0x01; // Type A
        ans[4] = 0x00; ans[5] = 0x01; // Class IN
        ans[6] = 0x00; ans[7] = 0x00; ans[8] = 0x00; ans[9] = 0x3C; // TTL = 60s
        ans[10] = 0x00; ans[11] = 0x04; // Data length = 4 bytes
        ans[12] = 192; ans[13] = 168; ans[14] = 4; ans[15] = 1; // IP 192.168.4.1

        int total_len = q_end + 16;
        sendto(s_sock, rx_buffer, total_len, 0, (struct sockaddr *)&client_addr, client_addr_len);
    }

    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
    ESP_LOGI(TAG, "DNS Server stopped");
    vTaskDelete(NULL);
}

esp_err_t dns_captive_start(void)
{
    if (s_running) return ESP_OK;
    s_running = true;
    BaseType_t ret = xTaskCreatePinnedToCore(dns_server_task, "dns_server", 4096, NULL, 5, &s_dns_task, 0);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

void dns_captive_stop(void)
{
    s_running = false;
    s_dns_task = NULL;
}
