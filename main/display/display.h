#ifndef DISPLAY_H
#define DISPLAY_H

#define DISPLAY_SDA_IO 3
#define DISPLAY_SCL_IO 2
#define DISPLAY_PORT 1
#define DISPLAY_FREQ 40000

esp_err_t display_init(void);
void display_write(char *data, int len, int line, bool invert);
void display_clear(bool invert);


#endif