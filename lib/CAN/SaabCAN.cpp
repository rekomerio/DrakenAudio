#include "SaabCAN.hpp"

SaabCAN *canInstance;

SaabCAN::SaabCAN()
{
    canInstance = this;
    _listeners.fill(nullptr);
}

SaabCAN::~SaabCAN()
{
}

void SaabCAN::start()
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_19, GPIO_NUM_18, TWAI_MODE_NO_ACK); // TODO: Change mode to normal if possible
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_47_619KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG, "CAN driver installed");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to install CAN driver");
        assert(0);
    }

    if (twai_reconfigure_alerts(TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF, NULL) == ESP_OK)
    {
        ESP_LOGD(LOG_TAG, "Alerts reconfigured");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to reconfigure alerts");
    }

    if (twai_start() == ESP_OK)
    {
        ESP_LOGD(LOG_TAG, "CAN driver started");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to start CAN driver");
        assert(0);
    }

    _mutex = xSemaphoreCreateMutex();

    if (_mutex == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to create the mutex");
        assert(0);
    }

    xTaskCreatePinnedToCore(receiveTaskCb, "CANReceive", 4096, NULL, 1, &_receiveTaskHandle, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(alertTaskCb, "CANAlerts", 4096, NULL, 1, &_alertTaskHandle, SAAB_TASK_CORE);
}

void SaabCAN::send(SAAB_CAN_ID id, const uint8_t *buf)
{
    constexpr TickType_t delay = pdMS_TO_TICKS(20);

    twai_message_t message;
    message.flags = 0;
    message.extd = 0; // 11 bit id
    message.rtr = 0;
    message.self = 0;
    message.ss = 0;
    message.dlc_non_comp = 0;
    message.data_length_code = SAAB_CAN_MSG_LENGTH;
    message.identifier = static_cast<uint32_t>(id);

    memcpy(message.data, buf, SAAB_CAN_MSG_LENGTH);

    // Queue message for transmission
    xSemaphoreTake(_mutex, portMAX_DELAY);
    esp_err_t result = twai_transmit(&message, delay);
    xSemaphoreGive(_mutex);

    if (result == ESP_OK)
    {
        ESP_LOGD(LOG_TAG, "Message queued for transmission");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to queue message for transmission");

        switch (result)
        {
        case ESP_ERR_INVALID_ARG:
            ESP_LOGE(LOG_TAG, "Invalid arguments");
            break;
        case ESP_ERR_TIMEOUT:
            ESP_LOGE(LOG_TAG, "Timed out waiting for space in TX queue");
            break;
        case ESP_FAIL:
            ESP_LOGE(LOG_TAG, "Queue is disabled and another message is transmitting");
            break;
        case ESP_ERR_INVALID_STATE:
            ESP_LOGE(LOG_TAG, "TWAI driver is not in running state, or is not installed");
            break;
        case ESP_ERR_NOT_SUPPORTED:
            ESP_LOGE(LOG_TAG, "Listen Only Mode does not support transmissions");
        default:
            ESP_LOGE(LOG_TAG, "Unknown error code %d", result);
            break;
        }
    }
}

void SaabCAN::addListener(SaabCANListener *listener, SAAB_CAN_LISTENER_TYPE type)
{
    _listeners[type] = listener;
}

void SaabCAN::receiveTask(void *arg)
{
    twai_message_t message;

    while (1)
    {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        esp_err_t result = twai_receive(&message, pdMS_TO_TICKS(5));
        xSemaphoreGive(_mutex);

        if (result == ESP_OK)
        {
            ESP_LOGD(LOG_TAG, "Message received");

            ESP_LOGD(LOG_TAG, "ID: %x", message.identifier);

            if (message.rtr)
            {
                ESP_LOGD(LOG_TAG, "Frame is remote frame");
            }
            else
            {
                for (int i = 0; i < _listeners.size(); i++)
                {
                    if (_listeners[i] != nullptr)
                    {
                        _listeners[i]->receive(static_cast<SAAB_CAN_ID>(message.identifier), message.data);
                    }
                }
            }
        }
        else if (result != ESP_ERR_TIMEOUT)
        {
            ESP_LOGE(LOG_TAG, "Error with receiving. Check the configuration for TWAI");
        }
    }
}

void SaabCAN::alertTask(void *arg)
{
    uint32_t alerts;
    while (1)
    {
        twai_read_alerts(&alerts, portMAX_DELAY);
        if (alerts & TWAI_ALERT_ABOVE_ERR_WARN)
        {
            ESP_LOGI(LOG_TAG, "Surpassed Error Warning Limit");
        }
        if (alerts & TWAI_ALERT_ERR_PASS)
        {
            ESP_LOGI(LOG_TAG, "Entered Error Passive state");
        }
        if (alerts & TWAI_ALERT_BUS_OFF)
        {
            ESP_LOGI(LOG_TAG, "Bus Off state");
            // Prepare to initiate bus recovery, reconfigure alerts to detect bus recovery completion
            twai_reconfigure_alerts(TWAI_ALERT_BUS_RECOVERED, NULL);
            for (int i = 3; i > 0; i--)
            {
                ESP_LOGW(LOG_TAG, "Initiate bus recovery in %d", i);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            twai_initiate_recovery(); // Needs 128 occurrences of bus free signal
            ESP_LOGI(LOG_TAG, "Initiate bus recovery");
        }
        if (alerts & TWAI_ALERT_BUS_RECOVERED)
        {
            // Bus recovery was successful, exit control task to uninstall driver
            ESP_LOGI(LOG_TAG, "Bus Recovered");
            break;
        }
    }
}

void SaabCAN::receiveTaskCb(void *arg)
{
    canInstance->receiveTask(arg);
}

void SaabCAN::alertTaskCb(void *arg)
{
    canInstance->alertTask(arg);
}