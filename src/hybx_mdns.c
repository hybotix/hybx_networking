/**
 * @file hybx_mdns.c
 * @brief Hybrid RobotiX - mDNS Service Advertisement & Discovery
 *
 * Advertises this node as a HybX service on the local network and
 * provides peer discovery via mDNS queries.
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#include "hybx_networking.h"

#include "mdns.h"
#include "esp_log.h"
#include <string.h>

// ── Private ───────────────────────────────────────────────────────────────────

static const char *TAG = "hybx_mdns";

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hybx_mdns_init(const hybx_mdns_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Set hostname and instance name
    const char *hostname = cfg->hostname[0] ? cfg->hostname : "hybx-node";
    err = mdns_hostname_set(hostname);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set hostname: %s", esp_err_to_name(err));
        return err;
    }

    const char *instance = cfg->instance_name[0] ? cfg->instance_name : "HybX Node";
    err = mdns_instance_name_set(instance);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set instance name: %s", esp_err_to_name(err));
        return err;
    }

    // Advertise service
    const char *svc_type = cfg->service_type[0] ? cfg->service_type : "_hybx";
    const char *proto    = cfg->proto[0]         ? cfg->proto        : "_tcp";
    uint16_t    port     = cfg->port             ? cfg->port         : 1883;

    // TXT records describing this node
    mdns_txt_item_t txt[] = {
        { "version", HYBX_NETWORKING_VERSION_STR },
        { "platform", "esp32"                     },
    };

    err = mdns_service_add(instance, svc_type, proto, port,
                           txt, sizeof(txt) / sizeof(txt[0]));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add mDNS service: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS: %s.local advertising %s.%s port=%d",
             hostname, svc_type, proto, port);

    return ESP_OK;
}

esp_err_t hybx_mdns_deinit(void)
{
    mdns_free();
    ESP_LOGI(TAG, "mDNS stopped");
    return ESP_OK;
}

esp_err_t hybx_mdns_find_nodes(char results[][HYBX_HOSTNAME_MAX_LEN],
                                 size_t max, size_t *found)
{
    if (!results || !found) return ESP_ERR_INVALID_ARG;
    *found = 0;

    mdns_result_t *res = NULL;
    esp_err_t err = mdns_query_ptr("_hybx", "_tcp", 3000, (uint8_t)max, &res);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mDNS query failed: %s", esp_err_to_name(err));
        return err;
    }

    mdns_result_t *r = res;
    while (r && *found < max) {
        if (r->hostname) {
            strncpy(results[*found], r->hostname, HYBX_HOSTNAME_MAX_LEN - 1);
            results[*found][HYBX_HOSTNAME_MAX_LEN - 1] = '\0';
            ESP_LOGI(TAG, "Found HybX node: %s.local", r->hostname);
            (*found)++;
        }
        r = r->next;
    }

    mdns_query_results_free(res);
    ESP_LOGI(TAG, "mDNS discovery: %zu node(s) found", *found);
    return ESP_OK;
}
