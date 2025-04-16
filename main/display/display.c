#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ssd1306.h"
#include "driver/i2c.h"

#include "display.h"

#define TAG "DISPLAY"

SSD1306_t display;

esp_err_t display_init(void)
{
    SSD1306_t dev;
    i2c_master_init(&dev, DISPLAY_SDA_IO, DISPLAY_SCL_IO, -1);

    dev._flip = false;
    ssd1306_init(&dev, 128, 64);

    // Limpa a tela
    ssd1306_clear_screen(&dev, false);

    // Mostra uma mensagem
    ssd1306_display_text(&dev, 0, "ESP32-C3 OLED", 14, false);
    ssd1306_display_text(&dev, 2, "Usando nopnop2002", 18, false);
    ssd1306_display_text(&dev, 4, "I2C GPIO4/SDA", 14, false);
    ssd1306_display_text(&dev, 5, "I2C GPIO5/SCL", 14, false);

    return ESP_OK;
}


