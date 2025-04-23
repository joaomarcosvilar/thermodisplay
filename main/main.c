#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stdbool.h"

#include "freertos/FreeRTOS.h"

#include "app.h"
#include "common.h"
#include "display/display.h"
#include "temp/temp.h"
#include "i2c_local.h"
#include "wifi/wifi.h"
#include "wifi/mqtt.h"

#define TAG "main"

void app_main(void)
{
    esp_err_t res;

    res = i2c_local_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize i2c (E: %s)", esp_err_to_name(res));
        return;
    }

    res = display_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize display (E: %s)", esp_err_to_name(res));
        return;
    }

    res = temp_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize temperature sensor (E: %s)", esp_err_to_name(res));
        return;
    }

    res = wifi_driver_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize wifi driver (E: %s)", esp_err_to_name(res));
        return;
    }

    res = wifi_connect("PTHREAD", "fifo2rrobin");
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to connect wifi station (E: %s)", esp_err_to_name(res));
        return;
    }

    wifi_status_e wifi = WIFI_STATUS_UNDEFINED;
    do
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        wifi = wifi_status();
        if (wifi == -1)
        {
            ESP_LOGE(TAG, "Failed to connect wifi");
            return;
        }
    } while (wifi <= 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    res = mqtt_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed init mqtt (E: %s)", esp_err_to_name(res));
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(500));
    res = mqtt_server("mqtt://192.168.15.15", 1883, "mavi_esp32c3", "default", "default");
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed init mqtt (E: %s)", esp_err_to_name(res));
    }

    mqtt_status_e mqtt = MQTT_STATUS_UNDEFINED;
    do
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        mqtt = mqtt_status();
        if (mqtt == MQTT_STATUS_FAILED)
        {
            ESP_LOGE(TAG, "Failed connect to server");
            return;
        }
    } while (mqtt <= 0);

    res = app_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize aplication (E: %s)", esp_err_to_name(res));
        return;
    }
}