#ifndef U8G2_HAL_I2C_H
#define U8G2_HAL_I2C_H

#include "u8g2.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"

typedef struct
{
    i2c_port_t i2c_port;
    uint8_t device_adrss;
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_master_dev_handle_t i2c_dev_handle;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t clk_speed;
} u8g2_i2c_t;

#define U8G2_ESP32_HAL_UNDEFINED GPIO_NUM_NC

esp_err_t u8g2_i2c_init(u8g2_i2c_t *device);
uint8_t u8g2_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif
