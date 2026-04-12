#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_littlefs.h"
#include "cJSON.h"

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

void app_main(void) {
    mount_little_fs();
}