#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stdbool.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "app.h"
#include "common.h"
#include "display/display.h"
#include "temp/temp.h"
#include "i2c_local.h"
#include "wifi/mqtt.h"
#include "wifi/wifi.h"

#define TAG "app"

#define APP_TASK_NAME "app_task"
#define APP_TASK_STACK_SIZE 3 * 1024
#define APP_TASK_PRIOR 2

#define APP_MQTT_TASK_NAME "app_mqtt_task"
#define APP_MQTT_TASK_STACK_SIZE 3 * 1024
#define APP_MQTT_TASK_PRIOR 3

#define APP_NETWORK_TASK_NAME "app_network_task"
#define APP_NETWORK_TASK_STACK_SIZE 3 * 1024
#define APP_NETWORK_TASK_PRIOR 4

#define APP_NETWORK_TIMEOUT 10 * 1000 * 1000

#define APP_TIMER_HAND_NAME "APP_TIMER_HAND"
#define APP_TIMER_HAND_PERIOD pdMS_TO_TICKS(5 * 1000)
#define APP_TIMER_HAND_AUTORELOAD false
#define APP_TIMER_HAND_ID 0

#define APP_TIMER_NETWORK_NAME "APP_TIMER_NETWORK"
#define APP_TIMER_NETWORK_PERIOD pdMS_TO_TICKS(5 * 1000)
#define APP_TIMER_NETWORK_AUTORELOAD false
#define APP_TIMER_NETWORK_ID 1

#define APP_QUEUE_MQTT_LEN 10

#define APP_HAND_ON (1 << 0)
#define APP_HAND_OFF (1 << 1)

#define ESP_INTR_FLAG_DEFAULT 0

TaskHandle_t app_task_handle;
TaskHandle_t app_mqtt_task_handle;
TaskHandle_t app_network_handle;
EventGroupHandle_t app_event;
TimerHandle_t app_timer_hand;
TimerHandle_t app_timer_network;
QueueHandle_t app_queue_mqtt;

volatile bool app_hand_state = false;
bool send_state_hand = false;

float temp_sum = 0.0, contt = 0.0;

static void IRAM_ATTR app_intr_hand(void *arg)
{
    BaseType_t pxH = pdFALSE;

    xEventGroupSetBitsFromISR(app_event, APP_HAND_ON, &pxH);
    app_hand_state = true;
    portYIELD_FROM_ISR(pxH);
}

void app_timer_hand_callback(TimerHandle_t app_timer_hand)
{
    ESP_LOGE(TAG, "timer flag");
    temp_sum = 0;
    contt = 0.0;
    xEventGroupClearBits(app_event, APP_HAND_ON);
    display_clear(false);
    send_state_hand = true;
}

void app_timer_network_callback(TimerHandle_t app_timer_network)
{
    // if()
    ESP_LOGW(TAG, "Timer network flag resume");
    vTaskResume(app_network_handle);
}

void app_task(void *args)
{
    EventBits_t bits;
    char buffer[APP_BUFFER_DISPLAY_SIZE_MAX] = {0};
    mqtt_data_s mqtt_data = {0};
    while (true)
    {
        bits = xEventGroupGetBits(app_event);
        if (bits & APP_HAND_ON)
        {
            ESP_LOGI(TAG, "APP_HAND_ON");
            if (!send_state_hand)
            {
                snprintf(mqtt_data.topic, MQTT_MAX_TOPIC_LEN, MQTT_TOPIC_HAND);
                snprintf(mqtt_data.data, MQTT_MAX_DATA_LEN, "1");
                mqtt_data.len = strlen(mqtt_data.data);
                mqtt_data.qos = 0;
                xQueueSend(app_queue_mqtt, (void *)&mqtt_data, 0);
                memset(&mqtt_data, 0, sizeof(mqtt_data_s));
                ESP_LOGI(TAG, "Send hand state");
                send_state_hand = true;
            }

            volatile float temp = temp_read_temp_to();

            contt += 1.0; // <- TODO: limitar tamanho

            // Média flutuante
            // temp_sum = temp_sum + temp;
            // float temp_float = temp_sum / contt;
            // snprintf(buffer, APP_BUFFER_DISPLAY_SIZE_MAX, "C: %.2f", temp_float);

            // EMA
            float alfa = 0.1;
            if (temp_sum == 0)
            {
                temp_sum = temp;
            }
            else
            {
                temp_sum = temp * alfa + (1 - alfa) * temp_sum;
            }
            snprintf(buffer, APP_BUFFER_DISPLAY_SIZE_MAX, "C: %.2f", temp_sum);
            display_write(buffer, strlen(buffer), 4, false);

            snprintf(mqtt_data.topic, MQTT_MAX_TOPIC_LEN, MQTT_TOPIC_TEMP);
            snprintf(mqtt_data.data, MQTT_MAX_DATA_LEN, "%.4f", temp_sum);
            mqtt_data.len = strlen(mqtt_data.data);
            mqtt_data.qos = 0;
            xQueueSend(app_queue_mqtt, (void *)&mqtt_data, 0);
            memset(&mqtt_data, 0, sizeof(mqtt_data));
        }

        if (gpio_get_level(IR_GPIO) && app_hand_state)
        {
            xEventGroupSetBits(app_event, APP_HAND_OFF);
        }

        if (bits & APP_HAND_OFF)
        {
            ESP_LOGI(TAG, "APP_HAND_OFF");
            app_hand_state = false;

            snprintf(mqtt_data.topic, MQTT_MAX_TOPIC_LEN, MQTT_TOPIC_HAND);
            snprintf(mqtt_data.data, MQTT_MAX_DATA_LEN, "0");
            mqtt_data.len = strlen(mqtt_data.data);
            mqtt_data.qos = 0;
            xQueueSend(app_queue_mqtt, (void *)&mqtt_data, 0);
            memset(&mqtt_data, 0, sizeof(mqtt_data_s));
            ESP_LOGI(TAG, "Send hand state 0 ");

            xEventGroupClearBitsFromISR(app_event, APP_HAND_OFF);

            if (xTimerIsTimerActive(app_timer_hand) != pdFALSE)
            {
                xTimerReset(app_timer_hand, 0);
                ESP_LOGE(TAG, "timer reset");
            }
            else
            {
                xTimerStart(app_timer_hand, 0);
                ESP_LOGE(TAG, "timer init");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_mqtt_task(void *args)
{
    wifi_status_e wifi;
    mqtt_data_s buffer = {0};
    while (true)
    {
        if (xQueueReceive(app_queue_mqtt, &buffer, pdMS_TO_TICKS(100)) == pdPASS)
        {
            wifi = wifi_status();
            if (wifi != WIFI_STATUS_CONNECTED)
            {
                ESP_LOGE(TAG, "Failed send, wifi status: %d", wifi);
                continue;
            }

            mqtt_publish(&buffer.topic, &buffer.data, buffer.len, buffer.qos);
            memset(&buffer, 0, sizeof(mqtt_data_s));
        }
    };
}

esp_err_t app_network_wifi_connect(void)
{
    wifi_status_e wifi = WIFI_STATUS_UNDEFINED;
    esp_err_t res = wifi_connect("PTHREAD", "fifo2rrobin");
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to connect wifi station (E: %s)", esp_err_to_name(res));
        return ESP_FAIL;
    }

    uint64_t start_time = esp_timer_get_time();
    do
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        wifi = wifi_status();
        ESP_LOGE(TAG, "STATUS WIFI: %d", wifi);
        if ((wifi == WIFI_STATUS_FAILED) || ((esp_timer_get_time() - start_time) > APP_NETWORK_TIMEOUT))
        {
            ESP_LOGE(TAG, "Failed to connect wifi");
            return ESP_FAIL;
        }
    } while (wifi <= 0);

    return ESP_OK;
}

esp_err_t app_network_mqtt_connect(void)
{
    esp_err_t res;
    res = mqtt_server("mqtt://192.168.15.15", 1883, "mavi_esp32c3", "default", "default");
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed init mqtt (E: %s)", esp_err_to_name(res));
    }

    mqtt_status_e mqtt = MQTT_STATUS_UNDEFINED;
    uint64_t start_time = esp_timer_get_time();
    do
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        mqtt = mqtt_status();
        ESP_LOGW(TAG, "STATUS: %d", mqtt);
        if (mqtt == MQTT_STATUS_FAILED || ((esp_timer_get_time() - start_time) > APP_NETWORK_TIMEOUT))
        {
            ESP_LOGE(TAG, "Failed connect to server");
            return ESP_FAIL;
        }
    } while (mqtt <= 0);

    return ESP_OK;
}

void app_network(void *args)
{
    esp_err_t res;
    while (true)
    {
        res = app_network_wifi_connect();
        if (res == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(2000));
            res = app_network_mqtt_connect();
        }

        if (res != ESP_OK)
        {
            if (xTimerIsTimerActive(app_timer_network) != pdFALSE)
            {
                xTimerReset(app_timer_network, 0);
                ESP_LOGW(TAG, "timer NETWORK reset");
            }
            else
            {
                xTimerStart(app_timer_network, 0);
                ESP_LOGW(TAG, "timer NETWORK init");
            }
        }
        vTaskSuspend(app_network_handle);
    }
}

esp_err_t app_init(void)
{
    esp_err_t res;

    app_event = xEventGroupCreate();
    if (app_event == NULL)
    {
        ESP_LOGE(TAG, "Failed create event group");
        return ESP_FAIL;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1 << IR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,
        .pull_up_en = 1};
    res = gpio_config(&io_conf);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed configure gpio");
        return res;
    }

    res = gpio_set_intr_type(IR_GPIO, GPIO_INTR_NEGEDGE);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed configure gpio interrupt");
        return res;
    }

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(IR_GPIO, app_intr_hand, NULL);
    gpio_intr_enable(IR_GPIO);

    app_queue_mqtt = xQueueCreate(APP_QUEUE_MQTT_LEN, sizeof(mqtt_data_s));
    if (app_queue_mqtt == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue mqtt");
        return ESP_FAIL;
    }

    app_timer_hand = xTimerCreate(
        APP_TIMER_HAND_NAME,
        APP_TIMER_HAND_PERIOD,
        APP_TIMER_HAND_AUTORELOAD,
        APP_TIMER_HAND_ID,
        app_timer_hand_callback);
    if (app_timer_hand == NULL)
    {
        ESP_LOGE(TAG, "Failed create timer");
        return ESP_FAIL;
    }

    app_timer_network = xTimerCreate(
        APP_TIMER_NETWORK_NAME,
        APP_TIMER_NETWORK_PERIOD,
        APP_TIMER_NETWORK_AUTORELOAD,
        APP_TIMER_NETWORK_ID,
        app_timer_network_callback);
    if (app_timer_network == NULL)
    {
        ESP_LOGE(TAG, "Failed create timer");
        return ESP_FAIL;
    }

    BaseType_t xBase;
    xBase = xTaskCreate(
        app_task,
        APP_TASK_NAME,
        APP_TASK_STACK_SIZE,
        NULL,
        APP_TASK_PRIOR,
        &app_task_handle);
    if (xBase != pdPASS)
    {
        ESP_LOGE(TAG, "Failed create task %s", APP_TASK_NAME);
        return ESP_FAIL;
    }

    xBase = xTaskCreate(
        app_mqtt_task,
        APP_MQTT_TASK_NAME,
        APP_MQTT_TASK_STACK_SIZE,
        NULL,
        APP_MQTT_TASK_PRIOR,
        &app_mqtt_task_handle);
    if (xBase != pdPASS)
    {
        ESP_LOGE(TAG, "Failed create task %s", APP_MQTT_TASK_NAME);
        return ESP_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(2000));

    xBase = xTaskCreate(
        app_network,
        APP_NETWORK_TASK_NAME,
        APP_NETWORK_TASK_STACK_SIZE,
        NULL,
        APP_NETWORK_TASK_PRIOR,
        &app_network_handle);
    if (xBase != pdPASS)
    {
        ESP_LOGE(TAG, "Failed create task %s", APP_NETWORK_TASK_NAME);
        return ESP_FAIL;
    }

    return ESP_OK;
}
