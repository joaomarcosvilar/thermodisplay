#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "common.h"
#include "driver/i2c.h"
#include "driver/i2c_master.h"

#define TAG "I2C_local"

static void* g_i2c_bus_cache = NULL;
static i2c_master_bus_handle_t g_i2c_bus_handle = NULL;

esp_err_t i2c_local_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t res = i2c_new_master_bus(&i2c_mst_config, &g_i2c_bus_handle);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed init i2c bus (E: %s)", esp_err_to_name(res));
    }

    g_i2c_bus_cache = (void*)g_i2c_bus_handle;
    return res;
}

esp_err_t i2c_local_get_bus(void **handle)
{
    if (g_i2c_bus_handle == NULL)
        return ESP_ERR_INVALID_SIZE;

    *handle = g_i2c_bus_handle;
    return ESP_OK;
}