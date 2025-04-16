#ifndef I2C_LOCAL_H
#define I2C_LOCAL_H

esp_err_t i2c_local_init(void);
esp_err_t i2c_local_get_bus(void **handle);

#endif