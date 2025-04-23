#ifndef MQTT_H
#define MQTT_H

typedef enum
{
    MQTT_STATUS_UNDEFINED = -3,
    MQTT_STATUS_FAILED = -2,
    MQTT_STATUS_DISCONNECTED = -1,
    MQTT_STATUS_CONNECTING = 0,
    MQTT_STATUS_CONNECTED
} mqtt_status_e;

#define MQTT_TOPIC_TEMP "mavi/temperature"
#define MQTT_TOPIC_HAND "mavi/hand_state"

#define MQTT_MAX_TOPIC_LEN 64
#define MQTT_MAX_DATA_LEN 256

typedef struct
{
    char topic[MQTT_MAX_TOPIC_LEN];
    char data[MQTT_MAX_DATA_LEN];
    int len;
    int qos;
} mqtt_data_s;

#define MQTT_RETAIN 0
#define MQTT_MAX_RETRY_PUBLISH 5

// Based in mqtt tcp transport

esp_err_t mqtt_init(void);
esp_err_t mqtt_server(char *url, int port, char *client_id, char *username, char *pwd);
mqtt_status_e mqtt_status(void);
esp_err_t mqtt_publish(char *topic, char *data, int len, int qos);
esp_err_t mqtt_subcribe(char *topic, char *data, int len, int qos);

#endif