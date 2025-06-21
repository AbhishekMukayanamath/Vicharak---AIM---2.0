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

// Event group for Wi-Fi connection
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Wi-Fi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Got IP!");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Wi-Fi init
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

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
}

// SPIFFS init
static void spiffs_init(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    ESP_LOGI(TAG, "SPIFFS mounted.");
}

// HTTP event handler
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

// HTTPS download file
// HTTPS download file with separate download and write speed logging
static void download_file(void) {
    esp_http_client_config_t config = {
        .url = FILE_URL,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .skip_cert_common_name_check = true,
        .disable_auto_redirect = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_open(client, 0);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
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
        ESP_LOGE(TAG, "Failed to open file for writing");
        esp_http_client_cleanup(client);
        return;
    }

    char buffer[1024];
    int total_read = 0;
    int total_written = 0;

    int64_t start = esp_timer_get_time();
    int64_t download_time = 0;
    int64_t write_time = 0;

    while (1) {
        int64_t chunk_download_start = esp_timer_get_time();
        int data_read = esp_http_client_read(client, buffer, sizeof(buffer));
        int64_t chunk_download_end = esp_timer_get_time();

        if (data_read <= 0) break;
        download_time += (chunk_download_end - chunk_download_start);
        total_read += data_read;

        int64_t chunk_write_start = esp_timer_get_time();
        int written = fwrite(buffer, 1, data_read, file);
        int64_t chunk_write_end = esp_timer_get_time();

        write_time += (chunk_write_end - chunk_write_start);
        total_written += written;
    }

    int64_t end = esp_timer_get_time();
    double total_time = (end - start) / 1000000.0;
    double dl_time_sec = download_time / 1000000.0;
    double wr_time_sec = write_time / 1000000.0;

    fclose(file);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "📥 Downloaded: %d bytes", total_read);
    ESP_LOGI(TAG, "💾 Written to SPIFFS: %d bytes", total_written);
    ESP_LOGI(TAG, "⏱️ Total Time: %.2f seconds", total_time);
    ESP_LOGI(TAG, "⚡ Download Speed: %.2f KB/s", total_read / 1024.0 / (dl_time_sec > 0 ? dl_time_sec : 0.001));
    ESP_LOGI(TAG, "⚡ Write Speed: %.2f KB/s", total_written / 1024.0 / (wr_time_sec > 0 ? wr_time_sec : 0.001));
}



// Entry point
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    spiffs_init();
    wifi_init();
    download_file();
}
