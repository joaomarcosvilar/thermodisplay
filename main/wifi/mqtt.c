#include "stdio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "string.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "common.h"
#include "mqtt.h"

#define MQTT_TAG "MQTT"

#define MQTT_TASK_NAME "mqtt task"
#define MQTT_TASK_STACK_SIZE 3 * 1024
#define MQTT_TASK_PRIOR 3

#define MQTT_PUBLISH_TASK_NAME "mqtt publish task"
#define MQTT_PUBLISH_TASK_STACK_SIZE 3 * 1024
#define MQTT_PUBLISH_TASK_PRIOR 4

#define MQTT_SUBSCRIBE_TASK_NAME "mqtt subscribe task"
#define MQTT_SUBSCRIBE_TASK_STACK_SIZE 3 * 1024
#define MQTT_SUBSCRIBE_TASK_PRIOR 4

#define MQTT_QUEUE_LEN 10

#define MQTT_CONNECT (1 << 0)
#define MQTT_FAIL (1 << 1)
#define MQTT_DISCONNECTED (1 << 3)
#define MQTT_CONNECTED (1 << 4)
#define MQTT_FINISHED (1 << 5)

static TaskHandle_t mqtt_task_handle;
static TaskHandle_t mqtt_publish_task_handle;
static TaskHandle_t mqtt_subscribe_task_handle;
static EventGroupHandle_t mqtt_event_group_handle;
static QueueHandle_t mqtt_queue_publish;
static QueueHandle_t mqtt_queue_subcribe;

esp_mqtt_client_config_t g_mqtt_config = {0};
esp_mqtt_client_handle_t g_mqtt_client = NULL;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(MQTT_TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id, retry_connection = 0;
    mqtt_data_s payload = {0};

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGW(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(mqtt_event_group_handle, MQTT_CONNECTED);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        xEventGroupSetBits(mqtt_event_group_handle, MQTT_DISCONNECTED);
        if (retry_connection < MQTT_MAX_RETRY_PUBLISH)
        {
            retry_connection++;
            xEventGroupSetBits(mqtt_event_group_handle, MQTT_CONNECT);
        }
        else
        {
            xEventGroupSetBits(mqtt_event_group_handle, MQTT_FAIL);
        }

        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGW(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED");
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGW(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGW(MQTT_TAG, "MQTT_EVENT_PUBLISHED");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGW(MQTT_TAG, "MQTT_EVENT_DATA");

        strncpy(payload.data, event->data, event->data_len);
        payload.len = event->data_len;
        xQueueSend(mqtt_queue_subcribe, &payload, 0);

        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", payload.len, payload.data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(MQTT_TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        xEventGroupSetBits(mqtt_event_group_handle, MQTT_FAIL);
        break;
    default:
        ESP_LOGW(MQTT_TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_task(void *args)
{
    esp_err_t res;
    EventBits_t event;

    mqtt_status_e mqtt_status = MQTT_STATUS_UNDEFINED;

    while (true)
    {
        event = xEventGroupWaitBits(mqtt_event_group_handle,
                                    MQTT_CONNECT | MQTT_FAIL | MQTT_DISCONNECTED,
                                    pdTRUE,
                                    pdFALSE,
                                    pdMS_TO_TICKS(100));

        if (event & MQTT_CONNECT)
        {
            if (mqtt_status == MQTT_STATUS_CONNECTED)
            {
                esp_mqtt_client_disconnect(g_mqtt_client);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            g_mqtt_client = esp_mqtt_client_init(&g_mqtt_config);
            esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
            res = esp_mqtt_client_start(g_mqtt_client);
            if (res != ESP_OK)
            {
                ESP_LOGE(MQTT_TAG, "Failed connect");
                mqtt_status = MQTT_STATUS_FAILED;
                xEventGroupSetBits(mqtt_event_group_handle, MQTT_FAIL);
            }
        }

        if (event & MQTT_CONNECTED)
        {
            mqtt_status = MQTT_STATUS_CONNECTED;
        }
        if (event & MQTT_DISCONNECTED)
        {
            mqtt_status = MQTT_STATUS_DISCONNECTED;
        }
        if (event & MQTT_FAIL)
        {
            ESP_LOGI(MQTT_TAG, "MQTT_FAIL");
            mqtt_status = MQTT_STATUS_FAILED;
        }
        if (event & MQTT_FINISHED)
        {
            ESP_LOGI(MQTT_TAG, "MQTT_FINISHED");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void mqtt_publish_task(void *args)
{
    mqtt_data_s buffer;
    mqtt_status_e status = MQTT_STATUS_UNDEFINED;
    int retry = 0;
    while (true)
    {
        if (buffer.len != 0)
        {
            memset(&buffer, 0, sizeof(mqtt_data_s));
        }
        if (xQueueReceive(mqtt_queue_publish, &buffer, portMAX_DELAY) == pdTRUE)
        {
            status = mqtt_status();
            if (status == MQTT_STATUS_CONNECTED)
            {
                esp_mqtt_client_publish(g_mqtt_client, buffer.topic, buffer.data, buffer.len, buffer.qos, MQTT_RETAIN);
                ESP_LOGI(MQTT_TAG, "Send publish: %s -> %s", buffer.topic, buffer.data);
                retry = 0;
            }
            else if (retry > MQTT_MAX_RETRY_PUBLISH || status == MQTT_STATUS_FAILED)
            {
                ESP_LOGE(MQTT_TAG, "Failed to send payload");
                continue;
            }
            else
            {
                ESP_LOGE(MQTT_TAG, "Connection status: %d", mqtt_status());
                xQueueSendToFront(mqtt_queue_publish, &buffer, 0);
                retry++;
                vTaskDelay(pdTICKS_TO_MS(500));
            }
        }
    }
}

void mqtt_subscribe_task(void *args)
{
    ;
}

esp_err_t mqtt_init(void)
{
    mqtt_event_group_handle = xEventGroupCreate();
    if (mqtt_event_group_handle == NULL)
    {
        ESP_LOGE(MQTT_TAG, "Failed create event group");
        return ESP_FAIL;
    }

    mqtt_queue_publish = xQueueCreate(MQTT_QUEUE_LEN, sizeof(mqtt_data_s));
    if (mqtt_queue_publish == NULL)
    {
        ESP_LOGE(MQTT_TAG, "Failed create publish queue");
        return ESP_FAIL;
    }

    mqtt_queue_subcribe = xQueueCreate(MQTT_QUEUE_LEN, sizeof(mqtt_data_s));
    if (mqtt_queue_subcribe == NULL)
    {
        ESP_LOGE(MQTT_TAG, "Failed create subcribe queue");
        return ESP_FAIL;
    }

    BaseType_t xBs = xTaskCreate(mqtt_task,
                                 MQTT_TASK_NAME,
                                 MQTT_TASK_STACK_SIZE,
                                 NULL,
                                 MQTT_TASK_PRIOR,
                                 &mqtt_task_handle);
    if (xBs != pdTRUE)
    {
        ESP_LOGE(MQTT_TAG, "Failed create %s", MQTT_TASK_NAME);
        return ESP_FAIL;
    }

    xBs = xTaskCreate(mqtt_publish_task,
                      MQTT_PUBLISH_TASK_NAME,
                      MQTT_PUBLISH_TASK_STACK_SIZE,
                      NULL,
                      MQTT_PUBLISH_TASK_PRIOR,
                      &mqtt_publish_task_handle);
    if (xBs != pdTRUE)
    {
        ESP_LOGE(MQTT_TAG, "Failed create %s", MQTT_PUBLISH_TASK_NAME);
        return ESP_FAIL;
    }

    g_mqtt_config.network.disable_auto_reconnect = false;

    return ESP_OK;
}

esp_err_t mqtt_server(char *url, int port, char *client_id, char *username, char *pwd)
{
    if (url == NULL || client_id == NULL || username == NULL || pwd == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if ((strlen(url) < 64) && (strlen(client_id) < 32) && (strlen(username) < 32) && (strlen(pwd) < 32))
    {
        g_mqtt_config.broker.address.uri = url;
        g_mqtt_config.broker.address.port = port;
        g_mqtt_config.credentials.client_id = client_id;
        g_mqtt_config.credentials.username = username;
        g_mqtt_config.credentials.authentication.password = pwd;
    }
    else
    {
        return ESP_ERR_INVALID_SIZE;
    }

    xEventGroupSetBits(mqtt_event_group_handle, MQTT_CONNECT);

    return ESP_OK;
}

mqtt_status_e mqtt_status(void)
{
    EventBits_t event = xEventGroupGetBits(mqtt_event_group_handle);
    if (event & MQTT_CONNECTED)
    {
        return MQTT_STATUS_CONNECTED;
    }
    else if (event & MQTT_DISCONNECTED)
    {
        return MQTT_STATUS_DISCONNECTED;
    }
    else if (event & MQTT_FAIL)
    {
        return MQTT_STATUS_FAILED;
    }

    return MQTT_STATUS_UNDEFINED;
}

esp_err_t mqtt_publish(char *topic, char *data, int len, int qos)
{
    if (!topic || !data || len <= 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    mqtt_data_s buffer = {0};

    strncpy(buffer.topic, topic, sizeof(buffer.topic) - 1);
    strncpy(buffer.data, data, len < sizeof(buffer.data) ? len : sizeof(buffer.data) - 1);

    buffer.len = len;
    buffer.qos = qos;

    BaseType_t Bx = xQueueSend(mqtt_queue_publish, &buffer, 0);
    if (Bx != pdPASS)
    {
        ESP_LOGE(MQTT_TAG, "Failed send to queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t mqtt_subcribe(char *topic, char *data, int len, int qos)
{
    if (!topic || !data || len <= 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    mqtt_data_s buffer = {0};

    strncpy(buffer.topic, topic, sizeof(buffer.topic) - 1);
    buffer.len = len;
    buffer.qos = qos;

    esp_mqtt_client_subscribe(g_mqtt_client, topic, qos);

    xQueueReceive(mqtt_queue_subcribe, &buffer, portMAX_DELAY);

    ESP_LOGI(MQTT_TAG, "DATA: %.*s", buffer.len, buffer.data);

    esp_mqtt_client_unsubscribe(g_mqtt_client, buffer.topic);

    return ESP_OK;
}