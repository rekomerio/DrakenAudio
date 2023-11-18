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

void SaabCAN::start(const twai_general_config_t *g_config, const twai_timing_config_t *t_config, const twai_filter_config_t *f_config)
{
    if (twai_driver_install(g_config, t_config, f_config) == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "CAN driver installed");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to install CAN driver");
        assert(0);
    }

    if (twai_start() == ESP_OK)
    {
        ESP_LOGI(LOG_TAG, "CAN driver started");
    }
    else
    {
        ESP_LOGE(LOG_TAG, "Failed to start CAN driver");
        assert(0);
    }

    xTaskCreatePinnedToCore(receiveTaskCb, "CANReceive", 4096, NULL, 2, &_receiveTaskHandle, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(alertTaskCb, "CANAlerts", 2048, NULL, 1, &_alertTaskHandle, SAAB_TASK_CORE);
}

void SaabCAN::send(SAAB_CAN_ID id, const uint8_t *buf)
{
    twai_message_t message;
    message.extd = 0; // 11 bit id
    message.rtr = 0;
    message.self = 0;
    message.ss = 0;
    message.dlc_non_comp = 0;
    message.data_length_code = SAAB_CAN_MSG_LENGTH;
    message.identifier = static_cast<uint32_t>(id);

    memcpy(message.data, buf, SAAB_CAN_MSG_LENGTH);

    send(&message);
}

void SaabCAN::send(const twai_message_t *message)
{
    // Queue message for transmission
    esp_err_t result = twai_transmit(message, pdMS_TO_TICKS(20));

    if (result == ESP_OK)
    {
        ESP_LOGD(LOG_TAG, "Message queued for transmission: %x", message->identifier);
        _nFailedConsecutiveEnqueues = 0;
    }
    else
    {
        _nFailedConsecutiveEnqueues++;

        if (_nFailedConsecutiveEnqueues > 10)
        {
            ESP_LOGE(LOG_TAG, "Too many failed consecutive transmission attempts. Restarting...");
            ESP.restart();
        }

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
        esp_err_t result = twai_receive(&message, pdMS_TO_TICKS(50));

        if (result == ESP_OK)
        {
            ESP_LOGD(LOG_TAG, "Received ID: %x", message.identifier);

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
    uint32_t monitoredAlerts = TWAI_ALERT_ABOVE_ERR_WARN | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF;
    uint32_t busRecoveryStartedAt = 0;
    twai_reconfigure_alerts(monitoredAlerts, NULL);

    while (1)
    {
        if (twai_read_alerts(&alerts, pdMS_TO_TICKS(15500)) == ESP_OK)
        {
            if (alerts & TWAI_ALERT_ABOVE_ERR_WARN)
            {
                ESP_LOGW(LOG_TAG, "Surpassed Error Warning Limit");
            }
            if (alerts & TWAI_ALERT_ERR_PASS)
            {
                ESP_LOGE(LOG_TAG, "Entered Error Passive state");
            }
            if (alerts & TWAI_ALERT_BUS_OFF)
            {
                ESP_LOGE(LOG_TAG, "Bus Off state");
                // Prepare to initiate bus recovery, reconfigure alerts to detect bus recovery completion
                twai_reconfigure_alerts(TWAI_ALERT_BUS_RECOVERED, NULL);
                ESP_LOGW(LOG_TAG, "Initiating bus recovery...");
                twai_initiate_recovery();
                ESP_LOGI(LOG_TAG, "Bus recovery started");
                busRecoveryStartedAt = millis();
            }
            if (alerts & TWAI_ALERT_BUS_RECOVERED)
            {
                ESP_LOGW(LOG_TAG, "Bus Recovered");
                twai_reconfigure_alerts(monitoredAlerts, NULL);
                busRecoveryStartedAt = 0;
            }
        }

        // If the recovery takes too long: restart ESP
        if (busRecoveryStartedAt != 0 && (millis() - 15000UL) > busRecoveryStartedAt)
        {
            ESP_LOGW(LOG_TAG, "Restarting ESP because of too long recovery");
            ESP.restart();
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