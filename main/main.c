#include "stdio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "temp/temp.h"
#include "display/display.h"
#include "i2c_local.h"

#include "driver/i2c_master.h"
#include "driver/i2c.h"

void app_main(void)
{
    i2c_local_init();
    temp_init();
}


// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/i2c.h"
// #include "ssd1306.h"
// #include "mlx90614.h"
// #include "esp_log.h"
// // #include <string.h>

// #define TAG "MLX_OLED_EXAMPLE"

// // Pinos I2C compartilhados
// #define I2C_PORT I2C_NUM_0
// #define SDA_PIN 4
// #define SCL_PIN 5
// #define RESET_PIN -1

// // Endereço I2C do MLX90614
// #define MLX90614_ADDR 0x5A
// // Frequência I2C explícita (padronizada)
// #define I2C_FREQ_HZ 100000 // 100 kHz é uma velocidade segura para ambos dispositivos

// SSD1306_t oled;
// mlx90614_handle_t mlx_handle = NULL;

// void app_main(void)
// {
//     // Configuração do barramento I2C
//     i2c_master_bus_config_t i2c_mst_config = {
//         .clk_source = I2C_CLK_SRC_DEFAULT,
//         .i2c_port = I2C_PORT,
//         .scl_io_num = SCL_PIN,
//         .sda_io_num = SDA_PIN,
//         .flags.enable_internal_pullup = true,
//     };

//     i2c_master_bus_handle_t i2c_bus_handle;
//     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

//     // Configuração do dispositivo OLED
//     i2c_device_config_t oled_dev_cfg = {
//         .dev_addr_length = I2C_ADDR_BIT_LEN_7,
//         .device_address = 0x3C, // Endereço padrão do SSD1306
//         .scl_speed_hz = I2C_FREQ_HZ,
//     };

//     i2c_master_dev_handle_t oled_dev_handle;
//     ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &oled_dev_cfg, &oled_dev_handle));

//     // Inicializa o display OLED usando os handles criados manualmente
//     oled._i2c_bus_handle = i2c_bus_handle;
//     oled._i2c_dev_handle = oled_dev_handle;
//     oled._address = 0x3C;
//     oled._flip = false;
//     oled._i2c_num = I2C_PORT;

//     ssd1306_init(&oled, 128, 64);
//     ssd1306_clear_screen(&oled, false);
//     ssd1306_display_text(&oled, 1, "OLED WORKING", 13, false);

//     // vTaskDelay(pdMS_TO_TICKS(1000));
//     // // Configuração do dispositivo MLX90614
//     // i2c_device_config_t mlx_dev_cfg = {
//     //     .dev_addr_length = I2C_ADDR_BIT_LEN_7,
//     //     .device_address = MLX90614_ADDR,
//     //     .scl_speed_hz = I2C_FREQ_HZ,  // Mesma frequência do OLED
//     // };
//     // ESP_LOGI(TAG, "SCL frequency: %lu Hz", mlx_dev_cfg.scl_speed_hz);
//     // i2c_master_dev_handle_t mlx_dev_handle;
//     // esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &mlx_dev_cfg, &mlx_dev_handle);
//     // if (ret != ESP_OK) {
//     //     ESP_LOGE(TAG, "Failed to add MLX90614 device: %s", esp_err_to_name(ret));
//     //     ssd1306_display_text(&oled, 3, "MLX DEV ERROR", 13, false);
//     //     return;
//     // }
//     esp_err_t ret;
//     // Configuração do MLX90614
//     mlx90614_config_t mlx_config = {
//         .mlx90614_device.device_address = MLX90614_ADDR,
//         .mlx90614_device.scl_speed_hz = I2C_FREQ_HZ};

//     // Inicializa o MLX90614 usando a handle criada manualmente
//     ret = mlx90614_init(i2c_bus_handle, &mlx_config, &mlx_handle);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "MLX90614 init failed: %s", esp_err_to_name(ret));
//         ssd1306_display_text(&oled, 3, "MLX ERROR", 9, false);
//     }
//     else
//     {
//         ESP_LOGI(TAG, "MLX90614 initialized successfully");
//         ssd1306_display_text(&oled, 3, "MLX OK", 6, false);
//     }

//     char temp_str_obj[32];
//     char temp_str_amb[32];

//     while (1)
//     {
//         float to, ta;

//         // Lê temperaturas do objeto e ambiente
//         esp_err_t ret_to = mlx90614_get_to(mlx_handle, &to);
//         esp_err_t ret_ta = mlx90614_get_ta(mlx_handle, &ta);

//         // Prepara e exibe temperatura do objeto
//         if (ret_to == ESP_OK)
//         {
//             snprintf(temp_str_obj, sizeof(temp_str_obj), "Obj: %.1f C", to);
//             ssd1306_display_text(&oled, 4, temp_str_obj, strlen(temp_str_obj), false);
//         }
//         else
//         {
//             ssd1306_display_text(&oled, 4, "Obj: ERROR", 10, false);
//         }

//         // Prepara e exibe temperatura ambiente
//         if (ret_ta == ESP_OK)
//         {
//             snprintf(temp_str_amb, sizeof(temp_str_amb), "Amb: %.1f C", ta);
//             ssd1306_display_text(&oled, 5, temp_str_amb, strlen(temp_str_amb), false);
//         }
//         else
//         {
//             ssd1306_display_text(&oled, 5, "Amb: ERROR", 10, false);
//         }

//         // Log para console
//         if (ret_to == ESP_OK && ret_ta == ESP_OK)
//         {
//             ESP_LOGI(TAG, "To: %.1f C, Ta: %.1f C", to, ta);
//         }
//         else
//         {
//             ESP_LOGE(TAG, "Error reading temperatures");
//         }

//         // Atualiza display a cada segundo
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }