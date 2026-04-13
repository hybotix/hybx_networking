#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_littlefs.h"
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

char *get_credential(char *key, char *secrets) {

}

void app_main(void) {
    char *secrets;
    char *ssid, *passwd;
    cJSON *json;

    //  Initialize NVS flash - required by the ESP-IDF WiFi driver
    nvs_flash_init();

    //  Mount the file system
    mount_little_fs();

    //  Read the Secrets file
    secrets = read_secrets();

    //  Parse the secrets
    json = hybx_json_parse(secrets);

    //  Get the WiFi credentials
    ssid = hybx_json_get_string(json, "WIFI_SSID");
    passwd = hybx_json_get_string(json, "WIFI_PASSWD");
}