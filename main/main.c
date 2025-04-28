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

    res = mqtt_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed init mqtt (E: %s)", esp_err_to_name(res));
        return;
    }

    res = app_init();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize aplication (E: %s)", esp_err_to_name(res));
        return;
    }
}