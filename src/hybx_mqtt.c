/**
 * @file hybx_mqtt.c
 * @brief Hybrid RobotiX - MQTT Client
 *
 * Wraps esp-mqtt with automatic reconnection, HYBX_NET_EVENTS forwarding,
 * and a clean publish/subscribe API.
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#include "hybx_networking.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include <string.h>

// ── Private ───────────────────────────────────────────────────────────────────

static const char *TAG = "hybx_mqtt";

static esp_mqtt_client_handle_t s_client    = NULL;
static bool                     s_connected = false;

// ── MQTT Event Handler ────────────────────────────────────────────────────────

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event  = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            ESP_LOGI(TAG, "Connected to broker");
            esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_MQTT_CONNECTED,
                           NULL, 0, portMAX_DELAY);
            break;

        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGW(TAG, "Disconnected from broker");
            esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_MQTT_DISCONNECTED,
                           NULL, 0, portMAX_DELAY);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "Subscribed (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "Unsubscribed (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "Published (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_DATA: {
            ESP_LOGI(TAG, "RECV topic=%.*s len=%d",
                     event->topic_len, event->topic, event->data_len);
            esp_event_post(HYBX_NET_EVENTS, HYBX_NET_EVENT_MQTT_DATA,
                           event_data, sizeof(esp_mqtt_event_t), portMAX_DELAY);
            break;
        }

        case MQTT_EVENT_ERROR:
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP error: esp_tls=%d, errno=%d",
                         event->error_handle->esp_tls_last_esp_err,
                         event->error_handle->esp_transport_sock_errno);
            }
            break;

        default:
            break;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

esp_err_t hybx_mqtt_init(const hybx_mqtt_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    esp_mqtt_client_config_t mqtt_cfg = { 0 };
    mqtt_cfg.broker.address.uri       = cfg->uri;
    mqtt_cfg.credentials.client_id    = cfg->client_id[0] ? cfg->client_id : "hybx_node";
    mqtt_cfg.session.keepalive        = cfg->keepalive_sec ? cfg->keepalive_sec : 120;
    mqtt_cfg.session.disable_clean_session = !cfg->clean_session;

    if (cfg->username[0]) {
        mqtt_cfg.credentials.username           = cfg->username;
        mqtt_cfg.credentials.authentication.password = cfg->password;
    }

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));

    ESP_LOGI(TAG, "MQTT client started -> %s", cfg->uri);
    return ESP_OK;
}

esp_err_t hybx_mqtt_deinit(void)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;
    s_connected = false;
    ESP_ERROR_CHECK(esp_mqtt_client_stop(s_client));
    ESP_ERROR_CHECK(esp_mqtt_client_destroy(s_client));
    s_client = NULL;
    ESP_LOGI(TAG, "MQTT client stopped");
    return ESP_OK;
}

bool hybx_mqtt_is_connected(void)
{
    return s_connected;
}

int hybx_mqtt_publish(const char *topic, const char *data, int len,
                      int qos, bool retain)
{
    if (!s_client || !s_connected) {
        ESP_LOGW(TAG, "Publish skipped: not connected");
        return -1;
    }
    int msg_id = esp_mqtt_client_publish(s_client, topic, data,
                                          len ? len : (int)strlen(data),
                                          qos, (int)retain);
    if (msg_id < 0) ESP_LOGE(TAG, "Publish failed");
    return msg_id;
}

int hybx_mqtt_subscribe(const char *topic, int qos)
{
    if (!s_client) return -1;
    int msg_id = esp_mqtt_client_subscribe(s_client, topic, qos);
    if (msg_id < 0) ESP_LOGE(TAG, "Subscribe failed: %s", topic);
    else ESP_LOGI(TAG, "Subscribed: %s (qos=%d)", topic, qos);
    return msg_id;
}

int hybx_mqtt_unsubscribe(const char *topic)
{
    if (!s_client) return -1;
    return esp_mqtt_client_unsubscribe(s_client, topic);
}
