/**
 * @file main.c
 * @brief Hybrid RobotiX - hybx_networking
 *
 * Entry point / master init for the HybX networking stack.
 * Demonstrates a full bring-up: WiFi → MQTT → mDNS → SNTP.
 *
 * For integration into your own project, call hybx_net_init() from
 * your app_main() after populating a hybx_net_config_t.
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#include "hybx_networking.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// ── Event Base Definition ─────────────────────────────────────────────────────

ESP_EVENT_DEFINE_BASE(HYBX_NET_EVENTS);

// ── Private ───────────────────────────────────────────────────────────────────

static const char *TAG = "hybx_net";

// ── Master Init ───────────────────────────────────────────────────────────────

esp_err_t hybx_net_init(const hybx_net_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    esp_err_t err;

    // ── NVS ──────────────────────────────────────────────────────────────────
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition issue — erasing and reinitializing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // ── esp-netif & event loop ────────────────────────────────────────────────
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // ── WiFi ─────────────────────────────────────────────────────────────────
    err = hybx_wifi_init(&cfg->wifi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = hybx_wifi_wait_connected(15000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi did not connect within timeout");
        return err;
    }

    hybx_netinfo_print();

    // ── mDNS ─────────────────────────────────────────────────────────────────
    if (cfg->enable_mdns) {
        err = hybx_mdns_init(&cfg->mdns);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(err));
            // Non-fatal — continue
        }
    }

    // ── SNTP ─────────────────────────────────────────────────────────────────
    if (cfg->enable_sntp) {
        err = hybx_sntp_init(&cfg->sntp);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SNTP init failed: %s", esp_err_to_name(err));
            // Non-fatal — continue
        }
    }

    // ── MQTT ─────────────────────────────────────────────────────────────────
    if (cfg->enable_mqtt) {
        err = hybx_mqtt_init(&cfg->mqtt);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "MQTT init failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    ESP_LOGI(TAG, "hybx_networking v%s ready", HYBX_NETWORKING_VERSION_STR);
    return ESP_OK;
}

esp_err_t hybx_net_deinit(void)
{
    hybx_mqtt_deinit();
    hybx_mdns_deinit();
    hybx_wifi_deinit();
    ESP_LOGI(TAG, "hybx_networking stack stopped");
    return ESP_OK;
}

// ── Example app_main ─────────────────────────────────────────────────────────
// Remove or replace this with your own app_main() when integrating.

void app_main(void)
{
    hybx_net_config_t cfg = {
        .wifi = {
            .ssid         = CONFIG_HYBX_WIFI_SSID,
            .password     = CONFIG_HYBX_WIFI_PASSWORD,
            .store_in_nvs = true,
            .max_retries  = 5,
        },
        .mqtt = {
            .uri          = CONFIG_HYBX_MQTT_URI,
            .client_id    = CONFIG_HYBX_MQTT_CLIENT_ID,
            .clean_session = true,
            .keepalive_sec = 60,
            .qos           = 1,
        },
        .mdns = {
            .hostname      = CONFIG_HYBX_MDNS_HOSTNAME,
            .instance_name = "HybX ESP32 Node",
            .service_type  = "_hybx",
            .proto         = "_tcp",
            .port          = 1883,
        },
        .sntp = {
            .servers       = { "pool.ntp.org", "time.google.com", NULL },
            .wait_for_sync = true,
            .sync_timeout_ms = 10000,
        },
        .enable_mqtt = true,
        .enable_mdns = true,
        .enable_sntp = true,
    };

    esp_err_t err = hybx_net_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE("app", "Network init failed: %s", esp_err_to_name(err));
        return;
    }

    // Example: subscribe to a command topic
    hybx_mqtt_subscribe("hybx/" CONFIG_HYBX_MQTT_CLIENT_ID "/cmd", 1);

    // Example: publish a hello message
    hybx_mqtt_publish("hybx/" CONFIG_HYBX_MQTT_CLIENT_ID "/status",
                      "{\"status\":\"online\",\"version\":\"" HYBX_NETWORKING_VERSION_STR "\"}",
                      0, 1, true);

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        hybx_netinfo_print();
    }
}
