#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_littlefs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "hybx_json_utilities.h"

void mount_little_fs (void) {
    esp_vfs_littlefs_conf_t littlefs_conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&littlefs_conf);
}

char *read_secrets(void) {
    char *secrets = NULL;
    long int file_size;

    //  Try opening the file
    FILE *fp = fopen("/littlefs/Secrets.json", "r");

    if (fp != NULL) {
        //  Get the file size
        fseek(fp, 0L, SEEK_END);
        file_size = ftell(fp);

        //  Seek back to the beginning of the file
        fseek(fp, 0L, SEEK_SET);

        // Create a buffer of that size
        secrets = (char *)malloc(file_size + 1);

        //  Read the file into the buffer
        fread(secrets, 1, file_size, fp);

        //  Must terminate the string with a NULL
        secrets[file_size] = '\0';

        //  Close the file
        fclose(fp);
    }

    return secrets;
}

esp_err_t connect_to_wifi (char *ssid, char *passwd) {
    esp_err_t status = ESP_OK;

    /*
        Initialize structs for use by the WiFi routines
    */
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    //  Initialize NVS flash - required by the ESP-IDF WiFi driver
    status = nvs_flash_init();

    if (status == ESP_OK) {
        status = esp_netif_init();        
    }

    if (status == ESP_OK) {
        status = esp_event_loop_create_default();
    }

    if (status == ESP_OK) {
         if (esp_netif_create_default_wifi_sta() != NULL) {
            status = ESP_OK;
        } else {
            status = ESP_FAIL; 
        }
    }

    if (status == ESP_OK) {
        status = esp_wifi_init(&init_config);
    }

    if (status == ESP_OK) {
        status = esp_wifi_set_mode(WIFI_MODE_STA);
    }

    if (status == ESP_OK) {
        strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, passwd, sizeof(wifi_config.sta.password) - 1);

        status = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    }

    if (status == ESP_OK) {
        status = esp_wifi_start();
    }

    if (status == ESP_OK) {
        status = esp_wifi_connect();
    }

    return status;
}

void app_main(void) {
    char *secrets;
    char *ssid, *passwd;
    cJSON *json;
    esp_err_t status = ESP_OK;

    //  Mount the file system
    mount_little_fs();

    //  Read the Secrets file
    secrets = read_secrets();

    //  Parse the secrets
    json = hybx_json_parse(secrets);

    //  Get the WiFi credentials
    ssid = hybx_json_get_string(json, "WIFI_SSID");
    passwd = hybx_json_get_string(json, "WIFI_PASSWD");

    status = connect_to_wifi(ssid, passwd);

    if (status == ESP_OK) {
        ESP_LOGI("main", "WiFi connected successfully");
    } else {
        ESP_LOGE("main", "WiFi connection failed");
    }
}