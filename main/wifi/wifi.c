#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "wifi.h"
#include "mqtt.h"

#define WIFI_TAG "Wi-Fi"

#define WIFI_TASK_NAME "WiFi"
#define WIFI_TASK_STACK_SIZE 1024 * 5
#define WIFI_TASK_PRIOR 2

#define WIFI_RETRY_CONNECT 3

#define WIFI_CONNECTED (1 << 0)
#define WIFI_READY (1 << 1)
#define WIFI_START_CONNECT (1 << 2)
#define WIFI_STATUS_UPDATE (1 << 3)

static EventGroupHandle_t wifi_event_group_handle;
static TaskHandle_t wifi_task_handle;
// static TimerHandle_t wifi_timer_handle;

static wifi_config_t g_wifi_config_sta;
static wifi_status_e wifi_connection_status;

static esp_err_t wifi_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

// TODO: Implement timer routine
//  static void wifi_timer_cb( TimerHandle_t xTimer )
//  {

// }

static void wifi_event_handle(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_WIFI_READY:
            ESP_LOGI(WIFI_TAG, "WIFI_EVENT_WIFI_READY");
            xEventGroupSetBits(wifi_event_group_handle, WIFI_READY);
            break;
        case WIFI_EVENT_STA_START:
            ESP_LOGI(WIFI_TAG, "WIFI_EVENT_STA_START");
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGI(WIFI_TAG, "WIFI_EVENT_STA_STOP");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(WIFI_TAG, "WIFI_EVENT_STA_CONNECTED");
            xEventGroupSetBits(wifi_event_group_handle, WIFI_CONNECTED | WIFI_STATUS_UPDATE);
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(WIFI_TAG, "WIFI_EVENT_STA_DISCONNECTED");
            xEventGroupSetBits(wifi_event_group_handle, WIFI_STATUS_UPDATE);
            xEventGroupClearBits(wifi_event_group_handle, WIFI_CONNECTED);
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
        case IP_EVENT_GOT_IP6:
            xEventGroupSetBits(wifi_event_group_handle, WIFI_CONNECTED);
            break;

        default:
            break;
        }
    }
}

static void wifi_task(void *args)
{
    EventBits_t event;
    wifi_scan_config_t scan_config =
        {
            .bssid = 0,
            .ssid = 0,
            .channel = 0,
            .show_hidden = true,
        };
    uint16_t ap_num = 0;
    esp_err_t err = ESP_OK;

    wifi_connection_status = WIFI_STATUS_UNDEFINED;
    for (;;)
    {
        event = xEventGroupWaitBits(wifi_event_group_handle,
                                    (WIFI_START_CONNECT | WIFI_STATUS_UPDATE | WIFI_CONNECTED),
                                    pdTRUE,
                                    pdFALSE,
                                    pdMS_TO_TICKS(100));

        if (event & WIFI_START_CONNECT)
        {
            wifi_connection_status = WIFI_STATUS_UNDEFINED;
            if (xEventGroupGetBits(wifi_event_group_handle) & WIFI_CONNECTED)
            {
                esp_wifi_disconnect();
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            err = esp_wifi_set_config(WIFI_IF_STA, &g_wifi_config_sta);
            if (err != ESP_OK)
            {
                ESP_LOGE(WIFI_TAG, "Failed to setup wifi configuration, ret = 0x%x", err);
                wifi_connection_status = WIFI_STATUS_FAILED;
            }
            else
            {
                err = esp_wifi_connect();
                if (err != ESP_OK)
                {
                    ESP_LOGE(WIFI_TAG, "Failed to Connected 0x%x", err);
                    wifi_connection_status = WIFI_STATUS_FAILED;
                }
                else
                {
                    wifi_connection_status = WIFI_STATUS_CONNECTING;
                }
            }
        }
        else if (event & WIFI_STATUS_UPDATE)
        {
            wifi_connection_status = wifi_status();
        }

        else if (event & WIFI_CONNECTED)
        {
            wifi_connection_status = WIFI_STATUS_CONNECTED;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t wifi_driver_init(void)
{
    esp_err_t ret = ESP_OK;

    ret = wifi_nvs_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Failed to initialize NVS");
        return ret;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Failed to initilaze the TCP/IP stack");
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Failed to create event loop");
    }

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t wifi_confg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_confg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Failed to init the WiFi, ret=0x%x", ret);
        return ret;
    }

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handle,
                                        NULL,
                                        NULL);

    esp_event_handler_instance_register(IP_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handle,
                                        NULL,
                                        NULL);

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Failed to setup the WiFi mode, ret=0x%x", ret);
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Failed ");
        return ret;
    }

    memset(&g_wifi_config_sta, 0, sizeof(g_wifi_config_sta));
    g_wifi_config_sta.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    g_wifi_config_sta.sta.failure_retry_cnt = WIFI_RETRY_CONNECT;
    g_wifi_config_sta.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    g_wifi_config_sta.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    wifi_event_group_handle = xEventGroupCreate();

    BaseType_t xRet = xTaskCreate(wifi_task,
                                  WIFI_TASK_NAME,
                                  WIFI_TASK_STACK_SIZE,
                                  NULL,
                                  WIFI_TASK_PRIOR,
                                  (TaskHandle_t *const)wifi_task_handle);

    if (xRet != pdTRUE)
    {
        ret = ESP_FAIL;
    }

    // TODO: Add timer creation
    // wifi_timer_handle = xTimerCreate("w-timer", pdMS_TO_TICKS(1500), pdFALSE, NULL, wifi_timer_cb);
    // if (wifi_timer_handle == NULL)
    // {
    //     ret = ESP_FAIL;
    // }

    return ret;
}

// TODO: Include DHCP option (SSID, GATEWAY and DNS)
esp_err_t wifi_connect(char *ssid, const char *password)
{
    if (!ssid || !password)
    {
        return ESP_FAIL;
    }

    if (strlen(ssid) < 32 && strlen(password) < 64)
    {
        memcpy(&g_wifi_config_sta.sta.ssid, (uint8_t *)ssid, 32);
        memcpy(&g_wifi_config_sta.sta.password, (uint8_t *)password, 64);
    }
    else
    {
        return ESP_ERR_INVALID_SIZE;
    }
    xEventGroupSetBits(wifi_event_group_handle, WIFI_START_CONNECT);
    return ESP_OK;
}

wifi_status_e wifi_status(void)
{
    return wifi_connection_status;
}