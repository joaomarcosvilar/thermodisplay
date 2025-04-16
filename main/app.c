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

#define APP_HAND_ON (1 << 0)
#define APP_HAND_OFF (1 << 1)

#define ESP_INTR_FLAG_DEFAULT 0

TaskHandle_t app_task_handle;
EventGroupHandle_t app_event;
TimerHandle_t timer_hand;

volatile bool app_hand_state = false;

static void IRAM_ATTR app_intr_hand(void *arg)
{
    BaseType_t pxH = pdFALSE;

    app_hand_state = !gpio_get_level(IR_GPIO);
    if (app_hand_state)
    {
        xEventGroupSetBitsFromISR(app_event, APP_HAND_ON, &pxH);
    }
    else
    {
        xEventGroupClearBitsFromISR(app_event, APP_HAND_ON);
        xEventGroupSetBitsFromISR(app_event, APP_HAND_OFF, &pxH);
    }

    portYIELD_FROM_ISR(pxH);
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
            volatile float temp = temp_read_temp_to();

            snprintf(buffer, APP_BUFFER_DISPLAY_SIZE_MAX, "C: %.2f", temp);

            display_write(buffer, strlen(buffer), 4, false);
        }

        if (bits & APP_HAND_OFF)
        {
            display_clear(false);
            xEventGroupClearBits(app_event, APP_HAND_OFF);
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
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = 1};
    res = gpio_config(&io_conf);
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed configure gpio");
        return res;
    }

    res = gpio_set_intr_type(IR_GPIO, GPIO_INTR_ANYEDGE);
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
