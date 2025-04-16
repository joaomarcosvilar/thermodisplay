#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stdbool.h"

#include "app.h"
#include "common.h"
#include "display/display.h"
#include "temp/temp.h"
#include "i2c_local.h"

#define TAG "main"

void app_main(void)
{
    esp_err_t res;

    res = i2c_local_init();
    if(res!= ESP_OK)
    {
        ESP_LOGE(TAG,"Failed to initize i2c (E: %s)",esp_err_to_name(res));
        return;
    }

    res = display_init();
    if(res!= ESP_OK)
    {
        ESP_LOGE(TAG,"Failed to initize display (E: %s)",esp_err_to_name(res));
        return;
    }

    res = temp_init();
    if(res!= ESP_OK)
    {
        ESP_LOGE(TAG,"Failed to initize temperature sensor (E: %s)",esp_err_to_name(res));
        return;
    }

    res = app_init();
    if(res!= ESP_OK)
    {
        ESP_LOGE(TAG,"Failed to initize aplication (E: %s)",esp_err_to_name(res));
        return;
    }
}