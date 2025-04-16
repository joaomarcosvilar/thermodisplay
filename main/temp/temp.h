#ifndef TEMP_H
#define TEMP_H

#define TEMP_SDA_IO 4
#define TEMP_SCL_IO 5
#define TEMP_PORT 0
#define TEMP_FREQ 40000

esp_err_t temp_init(void);
float temp_read_temp_to(void);
float temp_read_temp_ta(void);

#endif