#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ssd1306.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"

#include "i2c_local.h"
#include "common.h"

#include "display.h"

#define TAG "display"

SSD1306_t g_display_handle;

esp_err_t display_init(void)
{
    i2c_device_config_t display_dev_conf =
        {
            .device_address = DISPLAY_ADDRS,
            .scl_speed_hz = I2C_FREQ_HZ};

    i2c_master_bus_handle_t bus_handle;
    esp_err_t res = i2c_local_get_bus(&bus_handle);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize display (E: %s)", esp_err_to_name(res));
        return res;
    }

    i2c_master_bus_handle_t display_handle;
    i2c_master_bus_add_device(bus_handle, &display_dev_conf, &display_handle);

    g_display_handle._i2c_bus_handle = bus_handle;
    g_display_handle._i2c_dev_handle = display_handle;
    g_display_handle._address = DISPLAY_ADDRS;
    g_display_handle._flip = false;
    g_display_handle._i2c_num = I2C_NUM_0;

    ssd1306_init(&g_display_handle, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    ssd1306_clear_screen(&g_display_handle, true);
    ssd1306_display_text(&g_display_handle, 4, "Initializing...", 16, true);
    vTaskDelay(pdMS_TO_TICKS(500));
    ssd1306_clear_screen(&g_display_handle, false);
    ESP_LOGI(TAG, "Display init");

    return ESP_OK;
}

void display_write(char *data, int len, int line, bool invert)
{
    ssd1306_display_text(&g_display_handle, line, data, len, invert);
}
void display_clear(bool invert)
{
    ssd1306_clear_screen(&g_display_handle, invert);
}