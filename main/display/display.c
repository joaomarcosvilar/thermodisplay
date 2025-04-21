#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"

#include <u8g2.h>
#include "u8g2_i2c.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"

#include "i2c_local.h"
#include "common.h"

#include "display.h"

#define TAG "display"

// Declare a estrutura u8g2 como global para persistir além da função de inicialização
static u8g2_t u8g2;
static u8g2_i2c_t display_handle;
static bool display_initialized = false;

esp_err_t display_init(void)
{
    display_handle.sda_pin = SDA_PIN;
    display_handle.scl_pin = SCL_PIN;
    display_handle.clk_speed = I2C_FREQ_HZ;
    display_handle.device_adrss = 0x3C; // Endereço real (sem deslocamento)

    i2c_master_bus_handle_t i2c_bus_handle;
    esp_err_t ret = i2c_local_get_bus(&i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get I2C bus handle: %d", ret);
        return ret;
    }
    
    display_handle.i2c_bus_handle = i2c_bus_handle;

    ret = u8g2_i2c_init(&display_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize U8G2 I2C: %d", ret);
        return ret;
    }

    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0,
                                        u8g2_i2c_byte_cb,
                                        u8g2_esp32_gpio_and_delay_cb);

    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78); // 0x3C << 1

    ESP_LOGI(TAG, "u8g2_InitDisplay");
    u8g2_InitDisplay(&u8g2);

    ESP_LOGI(TAG, "u8g2_SetPowerSave");
    u8g2_SetPowerSave(&u8g2, 0); // Wake up display
    
    ESP_LOGI(TAG, "u8g2_ClearBuffer");
    u8g2_ClearBuffer(&u8g2);

    // Teste inicial
    u8g2_SetFont(&u8g2, u8g2_font_helvR18_tf);
    u8g2_DrawFrame(&u8g2, 0, 0, 125, 64);
    u8g2_DrawButtonUTF8(&u8g2, 64, 41, U8G2_BTN_HCENTER, 1, 1, 1, "Hi World");
    u8g2_SendBuffer(&u8g2);

    display_initialized = true;
    return ESP_OK;
}

void display_clear(bool invert)
{
    if (!display_initialized) return;
    
    u8g2_ClearBuffer(&u8g2);
    if (invert) {
        u8g2_DrawBox(&u8g2, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }
    u8g2_SendBuffer(&u8g2);
}

void display_write(char *data, int len, int line, bool invert)
{
    if (!display_initialized) return;
    
    // Altura aproximada de uma linha com a fonte padrão
    const int line_height = 16;
    int y_pos = line * line_height;
    
    if (invert) {
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawBox(&u8g2, 0, y_pos - line_height + 2, DISPLAY_WIDTH, line_height);
        u8g2_SetDrawColor(&u8g2, 1);
    }
    
    u8g2_SetFont(&u8g2, u8g2_font_ncenB10_tr);
    u8g2_DrawStr(&u8g2, 0, y_pos, data);
    u8g2_SendBuffer(&u8g2);
}