/**
 * @file hybx_networking.h
 * @brief Hybrid RobotiX Networking Library - Public API
 *
 * Unified networking stack for HybX ESP32 nodes:
 *   - WiFi station management with NVS credential storage
 *   - MQTT client with automatic reconnection
 *   - mDNS service advertisement and discovery
 *   - SNTP time synchronization
 *   - Network diagnostic / info utilities
 *
 * Copyright (c) 2026 Dale Weber - Hybrid RobotiX
 * MIT License
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_event.h"

// ── Version ───────────────────────────────────────────────────────────────────

#define HYBX_NETWORKING_VERSION_MAJOR  0
#define HYBX_NETWORKING_VERSION_MINOR  1
#define HYBX_NETWORKING_VERSION_PATCH  0
#define HYBX_NETWORKING_VERSION_STR    "0.1.0"

// ── Limits ────────────────────────────────────────────────────────────────────

#define HYBX_SSID_MAX_LEN       32
#define HYBX_PASSWORD_MAX_LEN   64
#define HYBX_HOSTNAME_MAX_LEN   64
#define HYBX_MQTT_URI_MAX_LEN   128
#define HYBX_MQTT_TOPIC_MAX_LEN 128
#define HYBX_WIFI_RETRY_MAX     5

// ── Event Base ────────────────────────────────────────────────────────────────

ESP_EVENT_DECLARE_BASE(HYBX_NET_EVENTS);

typedef enum {
    HYBX_NET_EVENT_WIFI_CONNECTED,
    HYBX_NET_EVENT_WIFI_DISCONNECTED,
    HYBX_NET_EVENT_GOT_IP,
    HYBX_NET_EVENT_MQTT_CONNECTED,
    HYBX_NET_EVENT_MQTT_DISCONNECTED,
    HYBX_NET_EVENT_MQTT_DATA,
    HYBX_NET_EVENT_TIME_SYNCED,
} hybx_net_event_id_t;

// ── Config Structures ─────────────────────────────────────────────────────────

/**
 * @brief WiFi station configuration
 */
typedef struct {
    char ssid[HYBX_SSID_MAX_LEN];
    char password[HYBX_PASSWORD_MAX_LEN];
    bool store_in_nvs;          /**< Persist credentials to NVS */
    uint8_t max_retries;        /**< 0 = use HYBX_WIFI_RETRY_MAX */
} hybx_wifi_config_t;

/**
 * @brief MQTT client configuration
 */
typedef struct {
    char uri[HYBX_MQTT_URI_MAX_LEN];    /**< e.g. "mqtt://broker.local:1883" */
    char client_id[64];                  /**< Unique client ID */
    char username[64];                   /**< Optional */
    char password[64];                   /**< Optional */
    bool clean_session;
    uint16_t keepalive_sec;             /**< 0 = default (120s) */
    uint8_t qos;                        /**< Default QoS for publish (0/1/2) */
} hybx_mqtt_config_t;

/**
 * @brief mDNS service configuration
 */
typedef struct {
    char hostname[HYBX_HOSTNAME_MAX_LEN];   /**< mDNS hostname (no .local) */
    char instance_name[64];                  /**< Human-readable instance name */
    char service_type[32];                   /**< e.g. "_hybx" */
    char proto[8];                           /**< "_tcp" or "_udp" */
    uint16_t port;
} hybx_mdns_config_t;

/**
 * @brief SNTP configuration
 */
typedef struct {
    const char *servers[3];     /**< NTP server hostnames, NULL-terminated */
    bool wait_for_sync;         /**< Block init until time is synced */
    uint32_t sync_timeout_ms;   /**< Max wait if wait_for_sync=true */
} hybx_sntp_config_t;

/**
 * @brief Master networking configuration
 */
typedef struct {
    hybx_wifi_config_t  wifi;
    hybx_mqtt_config_t  mqtt;
    hybx_mdns_config_t  mdns;
    hybx_sntp_config_t  sntp;
    bool enable_mqtt;
    bool enable_mdns;
    bool enable_sntp;
} hybx_net_config_t;

// ── WiFi API ─────────────────────────────────────────────────────────────────

/**
 * @brief Initialize and connect WiFi station.
 *        Must be called after nvs_flash_init() and esp_netif_init().
 */
esp_err_t hybx_wifi_init(const hybx_wifi_config_t *cfg);

/** @brief Disconnect and deinitialize WiFi */
esp_err_t hybx_wifi_deinit(void);

/** @brief Returns true if station has an IP address */
bool hybx_wifi_is_connected(void);

/** @brief Block until connected or timeout (ms). 0 = wait forever. */
esp_err_t hybx_wifi_wait_connected(uint32_t timeout_ms);

/** @brief Load credentials from NVS into cfg. Returns ESP_ERR_NOT_FOUND if none stored. */
esp_err_t hybx_wifi_load_nvs(hybx_wifi_config_t *cfg);

/** @brief Save credentials to NVS */
esp_err_t hybx_wifi_save_nvs(const hybx_wifi_config_t *cfg);

// ── MQTT API ─────────────────────────────────────────────────────────────────

/**
 * @brief Initialize MQTT client. WiFi must be connected first.
 */
esp_err_t hybx_mqtt_init(const hybx_mqtt_config_t *cfg);

/** @brief Stop and destroy MQTT client */
esp_err_t hybx_mqtt_deinit(void);

/** @brief Returns true if MQTT broker is connected */
bool hybx_mqtt_is_connected(void);

/**
 * @brief Publish a message.
 * @param topic  Full topic string
 * @param data   Payload (need not be NUL-terminated)
 * @param len    Payload length; 0 = use strlen(data)
 * @param qos    0/1/2
 * @param retain Retain flag
 * @return Message ID, or -1 on error
 */
int hybx_mqtt_publish(const char *topic, const char *data, int len,
                      int qos, bool retain);

/**
 * @brief Subscribe to a topic.
 * @return Message ID, or -1 on error
 */
int hybx_mqtt_subscribe(const char *topic, int qos);

/** @brief Unsubscribe from a topic */
int hybx_mqtt_unsubscribe(const char *topic);

// ── mDNS API ─────────────────────────────────────────────────────────────────

/** @brief Initialize mDNS and advertise this node's service */
esp_err_t hybx_mdns_init(const hybx_mdns_config_t *cfg);

/** @brief Stop mDNS */
esp_err_t hybx_mdns_deinit(void);

/**
 * @brief Query for HybX nodes on the network.
 * @param results  Caller-allocated buffer for result strings (hostname\0...)
 * @param max      Max number of results
 * @param found    Number of results actually found
 */
esp_err_t hybx_mdns_find_nodes(char results[][HYBX_HOSTNAME_MAX_LEN],
                                 size_t max, size_t *found);

// ── SNTP API ─────────────────────────────────────────────────────────────────

/** @brief Initialize SNTP and start time synchronization */
esp_err_t hybx_sntp_init(const hybx_sntp_config_t *cfg);

/** @brief Returns true if system time has been synchronized */
bool hybx_sntp_is_synced(void);

/** @brief Block until synced or timeout. Returns ESP_ERR_TIMEOUT on timeout. */
esp_err_t hybx_sntp_wait_synced(uint32_t timeout_ms);

// ── Network Info API ─────────────────────────────────────────────────────────

/** @brief Print full network status to ESP log (INFO level) */
void hybx_netinfo_print(void);

/** @brief Get current IPv4 address as string. buf must be >= 16 bytes. */
esp_err_t hybx_netinfo_get_ip(char *buf, size_t len);

/** @brief Get MAC address as "AA:BB:CC:DD:EE:FF" string. buf must be >= 18 bytes. */
esp_err_t hybx_netinfo_get_mac(char *buf, size_t len);

/** @brief Get RSSI of current WiFi connection */
int8_t hybx_netinfo_get_rssi(void);

// ── Master Init ───────────────────────────────────────────────────────────────

/**
 * @brief One-call initialization of the full networking stack.
 *        Calls nvs_flash_init, esp_netif_init, esp_event_loop_create_default
 *        then brings up WiFi, MQTT, mDNS, SNTP per the config flags.
 */
esp_err_t hybx_net_init(const hybx_net_config_t *cfg);

/** @brief Tear down the full networking stack */
esp_err_t hybx_net_deinit(void);

#ifdef __cplusplus
}
#endif
