#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"

#include <u8g2.h>
#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "i2c_local.h"
#include "common.h"
#include "image_bitmap.h"

#include "display.h"

#define TAG "DISPLAY"

typedef struct
{
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t i2c_freq;
    uint8_t adrss;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
} display_t;

display_t g_display_device;
u8g2_t g_display_u8g2;

uint8_t display_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    esp_err_t ret;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
    {
        i2c_device_config_t config = {
            .device_address = g_display_device.adrss,
            .scl_speed_hz = g_display_device.i2c_freq
        };
        ret = i2c_master_bus_add_device(g_display_device.bus_handle, &config, &g_display_device.dev_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to add I2C device in callback: %s", esp_err_to_name(ret));
            return 0;
        }
        break;
    }

    case U8X8_MSG_BYTE_SET_DC:
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        u8x8->i2c_started = 1;
        u8x8->i2c_address = g_display_device.adrss;
        break;

    case U8X8_MSG_BYTE_SEND:
    {
        if (g_display_device.dev_handle == NULL)
        {
            ESP_LOGE(TAG, "dev_handle is NULL during BYTE_SEND");
            break;
        }
        ret = i2c_master_transmit(g_display_device.dev_handle, arg_ptr, arg_int, pdMS_TO_TICKS(100));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "I2C transmit failed in BYTE_SEND: %s", esp_err_to_name(ret));
        }
        break;
    }

    case U8X8_MSG_BYTE_END_TRANSFER:
        u8x8->i2c_started = 0;
        break;

    default:
        break;
    }

    return 0;
}

uint8_t display_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        // Já inicializamos o GPIO no i2c_local_init
        return 1;

    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int));
        return 1;

    case U8X8_MSG_DELAY_10MICRO:
        // Espera aproximada de 10 microssegundos
        for (volatile int i = 0; i < 320; i++)
            ;
        return 1;

    case U8X8_MSG_DELAY_100NANO:
        // Espera aproximada de 100 nanosegundos
        for (volatile int i = 0; i < 32; i++)
            ;
        return 1;

    default:
        return 0;
    }
}

esp_err_t display_init(void)
{
    g_display_device.scl_pin = SCL_PIN;
    g_display_device.sda_pin = SDA_PIN;
    g_display_device.i2c_freq = I2C_FREQ_HZ;
    g_display_device.adrss = DISPLAY_ADDRS;

    i2c_master_bus_handle_t local_bus_handle;
    esp_err_t res = i2c_local_get_bus(&local_bus_handle);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize display (E: %s)", esp_err_to_name(res));
        return res;
    }
    g_display_device.bus_handle = local_bus_handle;

    u8g2_Setup_ssd1306_128x64_noname_1(&g_display_u8g2, U8G2_R0, display_cb, display_gpio_and_delay_cb);
    // u8x8_SetI2CAddress(&g_display_u8g2.u8x8, DISPLAY_ADDRS << 1);

    u8g2_InitDisplay(&g_display_u8g2);

    u8g2_SetPowerSave(&g_display_u8g2, false);

    u8g2_ClearBuffer(&g_display_u8g2);

    // u8g2_DrawBitmap(&g_display_u8g2, 64, 32, 128, 64, &image_bitmap);
    u8g2_SetFont(&g_display_u8g2, u8g2_font_helvR14_tr);
    u8g2_DrawButtonUTF8(&g_display_u8g2, 64, 50, U8G2_BTN_SHADOW1 | U8G2_BTN_HCENTER | U8G2_BTN_BW2, 56, 2, 2, "Subscribe to ltkdt");
    // vTaskDelay(pdMS_TO_TICKS(500));

    return ESP_OK;
}
