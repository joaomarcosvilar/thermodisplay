#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mlx90614.h"
#include "sdkconfig.h"

#define SCL_IO_PIN CONFIG_I2C_MASTER_SCL
#define SDA_IO_PIN CONFIG_I2C_MASTER_SDA
#define MASTER_FREQUENCY CONFIG_I2C_MASTER_FREQUENCY
#define PORT_NUMBER -1

static const char TAG[] = "app_main";

void app_main(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = PORT_NUMBER,
        .scl_io_num = SCL_IO_PIN,
        .sda_io_num = SDA_IO_PIN,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    for (int i = 0; i < 128; i++) {
        esp_err_t ret = i2c_master_probe(bus_handle, i, 50);
        if (ret == ESP_OK) {
            if (i == MLX90614_DEFAULT_ADDRESS) {
                ESP_LOGI(TAG, "found mlx90614 address: 0x%02X", i);
            } else {
                ESP_LOGI(TAG, "found i2c device address: 0x%02X", i);
            }
        }
    }

    mlx90614_config_t mlx90614_config = {
        .mlx90614_device.scl_speed_hz = MASTER_FREQUENCY,
        .mlx90614_device.device_address = MLX90614_DEFAULT_ADDRESS,
    };
    mlx90614_handle_t mlx90614_handle;
    ESP_ERROR_CHECK(
        mlx90614_init(bus_handle, &mlx90614_config, &mlx90614_handle));

    while (1) {
        float to, ta;
        mlx90614_get_to(mlx90614_handle, &to);
        mlx90614_get_ta(mlx90614_handle, &ta);
        ESP_LOGI(TAG, "To: %.1f°C, Ta: %.1f°C", to, ta);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
