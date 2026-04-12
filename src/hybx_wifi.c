/**
 * @file hybx_wifi.c
 * @brief Hybrid RobotiX - WiFi Station Manager
 *
 * Handles WiFi init, connect, reconnect, NVS credential persistence,
 * and event-driven state tracking.
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#include "hybx_networking.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

// ── Private ───────────────────────────────────────────────────────────────────

static const char *TAG = "hybx_wifi";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

#define NVS_NAMESPACE       "hybx_wifi"
#define NVS_KEY_SSID        "ssid"
#define NVS_KEY_PASS        "password"

static EventGroupHandle_t   s_wifi_event_group = NULL;
static esp_netif_t         *s_netif_sta        = NULL;
static int                  s_retry_count       = 0;
static uint8_t              s_max_retries       = HYBX_WIFI_RETRY_MAX;
static bool                 s_connected         = false;

// ── Event Handler ─────────────────────────────────────────────────────────────

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA started, connecting...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *disc =
                    (wifi_event_sta_disconnected_t *)event_data;
                s_connected = false;
                ESP_LOGW(TAG, "Disconnected (reason=%d)", disc->reason);
                esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_WIFI_DISCONNECTED,
                               NULL, 0, portMAX_DELAY);
                if (s_retry_count < s_max_retries) {
                    s_retry_count++;
                    ESP_LOGI(TAG, "Retry %d/%d", s_retry_count, s_max_retries);
                    esp_wifi_connect();
                } else {
                    ESP_LOGE(TAG, "Max retries reached");
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
                break;
            }

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Associated to AP");
                s_retry_count = 0;
                esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_WIFI_CONNECTED,
                               NULL, 0, portMAX_DELAY);
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        s_retry_count = 0;
        esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_GOT_IP,
                       &event->ip_info.ip, sizeof(event->ip_info.ip),
                       portMAX_DELAY);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hybx_wifi_init(const hybx_wifi_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    s_max_retries = cfg->max_retries ? cfg->max_retries : HYBX_WIFI_RETRY_MAX;
    s_retry_count = 0;
    s_connected   = false;

    // Create event group
    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
        if (!s_wifi_event_group) {
            ESP_LOGE(TAG, "Failed to create event group");
            return ESP_ERR_NO_MEM;
        }
    }

    // Create default STA netif (idempotent if already done)
    if (!s_netif_sta) {
        s_netif_sta = esp_netif_create_default_wifi_sta();
    }

    // Init WiFi with default config
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Build wifi_config
    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid,     cfg->ssid,     sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, cfg->password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_cfg.sta.pmf_cfg.capable    = true;
    wifi_cfg.sta.pmf_cfg.required   = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init done. Connecting to \"%s\"", cfg->ssid);

    // Optionally persist credentials
    if (cfg->store_in_nvs) {
        hybx_wifi_save_nvs(cfg);
    }

    return ESP_OK;
}

esp_err_t hybx_wifi_deinit(void)
{
    s_connected = false;
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    if (s_netif_sta) {
        esp_netif_destroy_default_wifi(s_netif_sta);
        s_netif_sta = NULL;
    }
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    ESP_LOGI(TAG, "WiFi deinitialized");
    return ESP_OK;
}

bool hybx_wifi_is_connected(void)
{
    return s_connected;
}

esp_err_t hybx_wifi_wait_connected(uint32_t timeout_ms)
{
    if (!s_wifi_event_group) return ESP_ERR_INVALID_STATE;

    TickType_t ticks = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE, ticks);

    if (bits & WIFI_CONNECTED_BIT) return ESP_OK;
    if (bits & WIFI_FAIL_BIT)      return ESP_FAIL;
    return ESP_ERR_TIMEOUT;
}

esp_err_t hybx_wifi_save_nvs(const hybx_wifi_config_t *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, NVS_KEY_SSID, cfg->ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, NVS_KEY_PASS, cfg->password);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    if (err == ESP_OK) ESP_LOGI(TAG, "Credentials saved to NVS");
    return err;
}

esp_err_t hybx_wifi_load_nvs(hybx_wifi_config_t *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t ssid_len = HYBX_SSID_MAX_LEN;
    size_t pass_len = HYBX_PASSWORD_MAX_LEN;

    err = nvs_get_str(handle, NVS_KEY_SSID, cfg->ssid, &ssid_len);
    if (err == ESP_OK) err = nvs_get_str(handle, NVS_KEY_PASS, cfg->password, &pass_len);

    nvs_close(handle);
    if (err == ESP_OK) ESP_LOGI(TAG, "Credentials loaded from NVS (SSID: %s)", cfg->ssid);
    return err;
}
