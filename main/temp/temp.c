#include "esp_log.h"
#include "esp_err.h"
#include "stdio.h"
#include "driver/i2c.h"

#include "mlx90614.h"
#include "i2c_local.h"
#include "temp.h"
#include "common.h"

mlx90614_handle_t g_temp_mlx90614_handle;

#define TAG "temp"

esp_err_t temp_init(void)
{
    esp_err_t res;
    mlx90614_config_t mlx_config = {
        .mlx90614_device.device_address = TEMP_ADDRS,
        .mlx90614_device.scl_speed_hz = I2C_FREQ_HZ};

    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_local_get_bus(&i2c_bus_handle);

    res = mlx90614_init(i2c_bus_handle, &mlx_config, &g_temp_mlx90614_handle);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed init device (E: %s)", esp_err_to_name(res));
    }

    ESP_LOGI(TAG,"Temp init");
    return res;
}

float temp_read_temp_to(void)
{
    float to;
    mlx90614_get_to(g_temp_mlx90614_handle, &to);
    // mlx90614_get_ta(g_temp_mlx90614_handle, &ta);
    // ESP_LOGI(TAG, "To: %.1f°C, Ta: %.1f°C", to, ta);
    return to;
}

float temp_read_temp_ta(void)
{
    float ta;
    // mlx90614_get_to(g_temp_mlx90614_handle, &to);
    mlx90614_get_ta(g_temp_mlx90614_handle, &ta);
    // ESP_LOGI(TAG, "To: %.1f°C, Ta: %.1f°C", to, ta);
    return ta;
}
