#ifndef _WIFI_WIFI_H_
#define _WIFI_WIFI_H_

#include "esp_err.h"

typedef enum
{
    WIFI_STATUS_UNDEFINED = -3,
    WIFI_STATUS_FAILED = -2,
    WIFI_STATUS_DISCONNECTED = -1,
    WIFI_STATUS_CONNECTING = 0,
    WIFI_STATUS_CONNECTED,
}wifi_status_e;

esp_err_t wifi_driver_init(void);
esp_err_t wifi_connect(char *ssid, const char *password);
wifi_status_e wifi_status(void);

#endif