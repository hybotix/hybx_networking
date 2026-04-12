/**
 * @file hybx_sntp.c
 * @brief Hybrid RobotiX - SNTP Time Synchronization
 *
 * Configures and starts SNTP, optionally blocking until the system clock
 * is synchronized. Posts HYBX_NET_EVENT_TIME_SYNCED when sync completes.
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#include "hybx_networking.h"

#include "esp_sntp.h"
#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <string.h>

// ── Private ───────────────────────────────────────────────────────────────────

static const char *TAG       = "hybx_sntp";
static bool        s_synced  = false;

static void sntp_sync_cb(struct timeval *tv)
{
    s_synced = true;
    char buf[32];
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "Time synchronized: %s UTC", buf);
    esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_TIME_SYNCED,
                   tv, sizeof(struct timeval), portMAX_DELAY);
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hybx_sntp_init(const hybx_sntp_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Set up to 3 servers
    int server_count = 0;
    for (int i = 0; i < 3; i++) {
        if (cfg->servers[i]) {
            esp_sntp_setservername(i, cfg->servers[i]);
            ESP_LOGI(TAG, "NTP server[%d]: %s", i, cfg->servers[i]);
            server_count++;
        }
    }
    if (server_count == 0) {
        // Sensible defaults
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.google.com");
        esp_sntp_setservername(2, "time.cloudflare.com");
        ESP_LOGI(TAG, "Using default NTP servers");
    }

    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP started");

    if (cfg->wait_for_sync) {
        uint32_t timeout = cfg->sync_timeout_ms ? cfg->sync_timeout_ms : 10000;
        esp_err_t err = hybx_sntp_wait_synced(timeout);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Time sync timed out after %lu ms", (unsigned long)timeout);
            return err;
        }
    }

    return ESP_OK;
}

bool hybx_sntp_is_synced(void)
{
    return s_synced;
}

esp_err_t hybx_sntp_wait_synced(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    const uint32_t interval = 250;

    while (!s_synced) {
        vTaskDelay(pdMS_TO_TICKS(interval));
        elapsed += interval;
        if (timeout_ms && elapsed >= timeout_ms) {
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}
