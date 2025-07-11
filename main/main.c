#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_spiffs.h"
#include "esp_timer.h"

#define WIFI_SSID      "Abhi"
#define WIFI_PASS      "12345678"
#define FILE_URL       "https://raw.githubusercontent.com/espressif/esp-idf/master/README.md"
#define FILE_PATH      "/spiffs/readme.txt"

static const char *TAG = "HTTPS_DOWNLOADER";
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

  // -------------------------------- WIFI SECTOR -------------------------------------------------------------- //

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP acquired");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Reconnecting Wi-Fi...");
    }
}

static void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS } };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, false, portMAX_DELAY);
}


// -------------------------------- SPIFFS PARTITION SECTOR -------------------------------------------------------------- //

static void spiffs_init(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    ESP_LOGI(TAG, "SPIFFS initialized");
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}


// -------------------------------- DOWNLOAD AND SPEED CHECK SECTOR -------------------------------------------------------------- //

static void download_file(void) {
    esp_http_client_config_t config = {
        .url = FILE_URL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .skip_cert_common_name_check = true
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP connection failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Invalid content length");
        esp_http_client_cleanup(client);
        return;
    }

    FILE *file = fopen(FILE_PATH, "wb");
    if (!file) {
        ESP_LOGE(TAG, "File open failed");
        esp_http_client_cleanup(client);
        return;
    }

    char buffer[1024];
    int total_read = 0, total_written = 0;
    int64_t start = esp_timer_get_time();
    int64_t download_time = 0, write_time = 0;

    while (1) {
        int64_t d_start = esp_timer_get_time();
        int read = esp_http_client_read(client, buffer, sizeof(buffer));
        int64_t d_end = esp_timer_get_time();
        if (read <= 0) break;
        download_time += (d_end - d_start);
        total_read += read;

        int64_t w_start = esp_timer_get_time();
        int written = fwrite(buffer, 1, read, file);
        int64_t w_end = esp_timer_get_time();
        write_time += (w_end - w_start);
        total_written += written;
    }

    fclose(file);
    esp_http_client_cleanup(client);

    double dl_sec = download_time / 1e6;
    double wr_sec = write_time / 1e6;

    ESP_LOGI(TAG, "Downloaded %d bytes", total_read);
    ESP_LOGI(TAG, "Written %d bytes", total_written);
    ESP_LOGI(TAG, "Download Speed: %.2f KB/s", total_read / 1024.0 / (dl_sec > 0 ? dl_sec : 0.001));
    ESP_LOGI(TAG, "Write Speed: %.2f KB/s", total_written / 1024.0 / (wr_sec > 0 ? wr_sec : 0.001));
}


// -------------------------------- APP MAIN  -------------------------------------------------------------- //

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    spiffs_init();
    wifi_init();
    download_file();
}
