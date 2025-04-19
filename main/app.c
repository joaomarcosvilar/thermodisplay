#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stdbool.h"
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "app.h"
#include "common.h"
#include "display/display.h"
#include "temp/temp.h"
#include "i2c_local.h"

#define TAG "app"

#define APP_TASK_NAME "app_task"
#define APP_TASK_STACK_SIZE 3 * 1024
#define APP_TASK_PRIOR 2

#define APP_TIMER_HAND_NAME "APP_TIMER_HAND"
#define APP_TIMER_HAND_PERIOD pdMS_TO_TICKS(5 * 1000)
#define APP_TIMER_HAND_AUTORELOAD false
#define APP_TIMER_HAND_ID 0

#define APP_HAND_ON (1 << 0)
#define APP_HAND_OFF (1 << 1)

#define ESP_INTR_FLAG_DEFAULT 0

TaskHandle_t app_task_handle;
EventGroupHandle_t app_event;
TimerHandle_t app_timer_hand;

volatile bool app_hand_state = false;
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
}

void app_task(void *args)
{
    EventBits_t bits;
    char buffer[APP_BUFFER_DISPLAY_SIZE_MAX] = {0};
    while (true)
    {
        bits = xEventGroupGetBits(app_event);
        if (bits & APP_HAND_ON)
        {
            ESP_LOGI(TAG, "APP_HAND_ON");

            if ((xTimerIsTimerActive(app_timer_hand) != pdFALSE) && !gpio_get_level(IR_GPIO))
            {
                xTimerStop(app_timer_hand, 0);
                ESP_LOGI(TAG,"stop timer");
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
        }

        if (gpio_get_level(IR_GPIO) && app_hand_state)
        {
            xEventGroupSetBits(app_event, APP_HAND_OFF);
        }

        if (bits & APP_HAND_OFF)
        {
            ESP_LOGI(TAG, "APP_HAND_OFF");
            app_hand_state = false;
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

esp_err_t app_init(void)
{
    esp_err_t res;

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

    app_event = xEventGroupCreate();
    if (app_event == NULL)
    {
        ESP_LOGE(TAG, "Failed create event group");
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

    BaseType_t xBase;
    xBase = xTaskCreate(app_task, APP_TASK_NAME, APP_TASK_STACK_SIZE, NULL, APP_TASK_PRIOR, &app_task_handle);
    if (xBase != pdPASS)
    {
        ESP_LOGE(TAG, "Failed create task");
        return ESP_FAIL;
    }

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(IR_GPIO, app_intr_hand, NULL);
    gpio_intr_enable(IR_GPIO);

    return ESP_OK;
}
