#ifndef DISPLAY_H
#define DISPLAY_H

#define DISPLAY_SDA_IO 3
#define DISPLAY_SCL_IO 2
#define DISPLAY_PORT 1
#define DISPLAY_FREQ 40000

#define DISPLAY_WIDTH 128
#define DISPLAY_EIGHT 64


esp_err_t display_init(void);

#endif