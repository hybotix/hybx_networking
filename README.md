# hybx_networking

**Hybrid RobotiX** — ESP32 Networking Library (PlatformIO / ESP-IDF)

A unified networking stack for HybX ESP32 nodes providing WiFi, MQTT, mDNS,
and SNTP in a single clean API — designed to drop into any HybX platform node.

---

## Features

| Module | Description |
|---|---|
| **WiFi** | Station mode, auto-reconnect, NVS credential persistence |
| **MQTT** | Full client with publish/subscribe, auto-reconnect, TLS-ready |
| **mDNS** | Service advertisement + peer discovery (`_hybx._tcp`) |
| **SNTP** | NTP time sync with callback notification |
| **Events** | Unified `HYBX_NET_EVENTS` event bus for the whole stack |
| **Netinfo** | IP, MAC, RSSI diagnostics |

---

## Supported Targets

| Environment | Board |
|---|---|
| `esp32s3` | ESP32-S3 DevKitC-1 (default) |
| `esp32` | ESP32 DevKit |
| `esp32c3` | ESP32-C3 DevKitM-1 |

---

## Quick Start

### 1. Clone

```bash
git clone https://github.com/hybotix/hybx_networking.git
cd hybx_networking
```

### 2. Configure

```bash
pio run --target menuconfig
```

Set your WiFi credentials, MQTT broker URI, and mDNS hostname under
**HybX Networking Configuration**. Or edit `sdkconfig.defaults` directly
before the first build.

### 3. Build & Flash

```bash
# Build for ESP32-S3 (default)
pio run -e esp32s3

# Flash
pio run -e esp32s3 --target upload

# Monitor
pio device monitor
```

---

## Integration

```c
#include "hybx_networking.h"

void app_main(void)
{
    hybx_net_config_t cfg = {
        .wifi = {
            .ssid         = "MySSID",
            .password     = "MyPassword",
            .store_in_nvs = true,
        },
        .mqtt = {
            .uri       = "mqtt://mqtt.local:1883",
            .client_id = "my_node",
        },
        .mdns = {
            .hostname = "my-node",
            .port     = 1883,
        },
        .sntp = {
            .servers         = { "pool.ntp.org", NULL, NULL },
            .wait_for_sync   = true,
            .sync_timeout_ms = 10000,
        },
        .enable_mqtt = true,
        .enable_mdns = true,
        .enable_sntp = true,
    };

    hybx_net_init(&cfg);

    hybx_mqtt_subscribe("hybx/my_node/cmd", 1);
    hybx_mqtt_publish("hybx/my_node/status", "{\"status\":\"online\"}", 0, 1, true);
}
```

---

## Event Bus

```c
esp_event_handler_register(HYBX_NET_EVENTS, HYBX_NET_EVENT_MQTT_DATA,
                            my_handler, NULL);
```

| Event | When |
|---|---|
| `HYBX_NET_EVENT_WIFI_CONNECTED` | Associated with AP |
| `HYBX_NET_EVENT_GOT_IP` | IP address assigned |
| `HYBX_NET_EVENT_WIFI_DISCONNECTED` | Lost connection |
| `HYBX_NET_EVENT_MQTT_CONNECTED` | Broker connected |
| `HYBX_NET_EVENT_MQTT_DISCONNECTED` | Broker disconnected |
| `HYBX_NET_EVENT_MQTT_DATA` | Incoming MQTT message |
| `HYBX_NET_EVENT_TIME_SYNCED` | SNTP sync complete |

---

## MQTT Topic Convention

```
hybx/<node_id>/status      ← retained online/offline
hybx/<node_id>/cmd         ← command input
hybx/<node_id>/telemetry   ← sensor data
hybx/<node_id>/log         ← debug/info log stream
```

---

## File Structure

```
hybx_networking/
├── platformio.ini          # PlatformIO environments (esp32s3, esp32, esp32c3)
├── CMakeLists.txt          # ESP-IDF project root
├── Kconfig.projbuild       # menuconfig integration
├── sdkconfig.defaults      # Recommended SDK defaults
├── include/
│   └── hybx_networking.h  # Public API (single include)
└── src/
    ├── CMakeLists.txt
    ├── main.c              # Entry point / example app_main
    ├── hybx_wifi.c         # WiFi station manager
    ├── hybx_mqtt.c         # MQTT client
    ├── hybx_mdns.c         # mDNS advertisement & discovery
    ├── hybx_sntp.c         # SNTP time sync
    └── hybx_netinfo.c      # Network diagnostics
```

---

## License

MIT License — Copyright (c) 2026 Dale Weber / Hybrid RobotiX

See [LICENSE](LICENSE) for full text.
