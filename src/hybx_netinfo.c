/**
 * @file hybx_netinfo.c
 * @brief Hybrid RobotiX - Network Diagnostics & Info
 *
 * Provides IP address, MAC address, RSSI, and a formatted
 * status dump for logging/debugging.
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#include "hybx_networking.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

// ── Private ───────────────────────────────────────────────────────────────────

static const char *TAG = "hybx_netinfo";

// ── Public API ────────────────────────────────────────────────────────────────

void hybx_netinfo_print(void)
{
    char ip[16]  = "?.?.?.?";
    char mac[18] = "??:??:??:??:??:??";

    hybx_netinfo_get_ip(ip, sizeof(ip));
    hybx_netinfo_get_mac(mac, sizeof(mac));
    int8_t rssi = hybx_netinfo_get_rssi();

    wifi_ap_record_t ap = { 0 };
    esp_wifi_sta_get_ap_info(&ap);

    ESP_LOGI(TAG, "┌─── HybX Network Status ──────────────────");
    ESP_LOGI(TAG, "│  IP Address : %s", ip);
    ESP_LOGI(TAG, "│  MAC Address: %s", mac);
    ESP_LOGI(TAG, "│  SSID       : %s", (char *)ap.ssid);
    ESP_LOGI(TAG, "│  RSSI       : %d dBm", rssi);
    ESP_LOGI(TAG, "│  Channel    : %d", ap.primary);
    ESP_LOGI(TAG, "│  AuthMode   : %d", ap.authmode);
    ESP_LOGI(TAG, "└──────────────────────────────────────────");
}

esp_err_t hybx_netinfo_get_ip(char *buf, size_t len)
{
    if (!buf || len < 16) return ESP_ERR_INVALID_ARG;

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        strncpy(buf, "0.0.0.0", len);
        return ESP_ERR_NOT_FOUND;
    }

    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(netif, &ip_info);
    if (err != ESP_OK) {
        strncpy(buf, "0.0.0.0", len);
        return err;
    }

    snprintf(buf, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}

esp_err_t hybx_netinfo_get_mac(char *buf, size_t len)
{
    if (!buf || len < 18) return ESP_ERR_INVALID_ARG;

    uint8_t mac[6];
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        strncpy(buf, "00:00:00:00:00:00", len);
        return err;
    }

    snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ESP_OK;
}

int8_t hybx_netinfo_get_rssi(void)
{
    wifi_ap_record_t ap = { 0 };
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return 0;
    return ap.rssi;
}
