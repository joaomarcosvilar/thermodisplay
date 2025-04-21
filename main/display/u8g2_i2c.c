#include "esp_err.h"
#include "esp_log.h"

#include "u8g2.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"

#include "common.h"
#include "u8g2_i2c.h"

#define TAG "U8G2_I2C"

static u8g2_i2c_t u8g2_i2c_device;

esp_err_t u8g2_i2c_init(u8g2_i2c_t *device)
{
    if (device == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    u8g2_i2c_device = *device;

    i2c_device_config_t config =
        {
            .device_address = u8g2_i2c_device.device_adrss,
            .scl_speed_hz = u8g2_i2c_device.clk_speed
        };

    esp_err_t res = i2c_master_bus_add_device(u8g2_i2c_device.i2c_bus_handle, &config, &u8g2_i2c_device.i2c_dev_handle);
    if(res!= ESP_OK)
    {
        return res;
    }
    return res;
}

uint8_t u8g2_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    esp_err_t ret;
    
    switch (msg)
    {
    case U8X8_MSG_BYTE_SEND:
        ret = i2c_master_transmit(u8g2_i2c_device.i2c_dev_handle, arg_ptr, arg_int, -1);
        return (ret == ESP_OK);

    case U8X8_MSG_BYTE_START_TRANSFER:
        // u8x8->i2c_address = u8x8->display_info->i2c_bus_address;
        return 1;

    case U8X8_MSG_BYTE_END_TRANSFER:
        return 1;

    default:
        return 0;
    }
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8,
                                     uint8_t msg,
                                     uint8_t arg_int,
                                     void *arg_ptr)
{
    ESP_LOGD(TAG,
             "gpio_and_delay_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p",
             msg, arg_int, arg_ptr);

    switch (msg)
    {
    // Para mensagens GPIO e delay, implementar conforme necessário
    // Retiradas referências a u8g2_esp32_hal que não estavam definidas
    
    // Delay for the number of milliseconds passed in through arg_int.
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(arg_int / portTICK_PERIOD_MS);
        break;
        
    // I2C clock e data (se for usar software I2C)
    case U8X8_MSG_GPIO_I2C_CLOCK:
        if (u8g2_i2c_device.scl_pin != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_i2c_device.scl_pin, arg_int);
        }
        break;
        
    case U8X8_MSG_GPIO_I2C_DATA:
        if (u8g2_i2c_device.sda_pin != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_i2c_device.sda_pin, arg_int);
        }
        break;
    }
    return 0;
}