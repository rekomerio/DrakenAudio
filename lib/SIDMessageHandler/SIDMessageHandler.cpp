#include "SIDMessageHandler.hpp"

SIDMessageHandler *sidMessageHandlerInstance;

SIDMessageHandler::SIDMessageHandler()
{
    sidMessageHandlerInstance = this;
    memset(_writeAccessDevice.data(), static_cast<int>(SID_COMMUNICATION_ID::NO_DISPLAY), _writeAccessDevice.size());
}

void SIDMessageHandler::start()
{
    xTaskCreatePinnedToCore(notificationTaskCb, "SIDNotificationTask", 2048, NULL, 1, NULL, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(sendTaskCb, "SIDSendTask", 2048, NULL, 1, &_sendTaskHandle, SAAB_TASK_CORE);
}

void SIDMessageHandler::addCANInterface(SaabCAN *can)
{
    _can = can;
    _can->addListener(this, SAAB_CAN_LISTENER_TYPE::SID);
}

void SIDMessageHandler::setMessage(const char *message)
{
    utf_convert(message, _buffer, strlen(message));
    if (_dropNotificationAt == 0 || _dropNotificationAt - millis() > 50)
    {
        _stringScroller.setString(_buffer, SID_MAX_CHARACTERS);
    }
    else
    {
        // Let the notification be displayed for a while, the notification task will pick up the message later
        _stringScrollerCopy.setString(_buffer, SID_MAX_CHARACTERS);
    }
}

void SIDMessageHandler::showNotification(const char *message, int durationMs)
{
    memcpy(&_stringScrollerCopy, &_stringScroller, sizeof(StringScroller));
    utf_convert(message, _buffer, strlen(message));
    _stringScroller.setString(_buffer, SID_MAX_CHARACTERS);
    _dropNotificationAt = millis() + durationMs;
    _isMessageOverrideRequired = true;
}

bool SIDMessageHandler::isWriteAllowed(uint8_t row, SID_COMMUNICATION_ID communicationId)
{
    if (row > 2)
    {
        return false;
    }

    return _writeAccessDevice[row] == communicationId;
}

void SIDMessageHandler::receive(SAAB_CAN_ID id, uint8_t *buf)
{
    switch (id)
    {
    case SAAB_CAN_ID::TEXT_PRIORITY:
    {
        uint8_t row = buf[0];
        if (row <= 2)
        {
            _writeAccessDevice[row] = static_cast<SID_COMMUNICATION_ID>(buf[1]);
        }
        break;
    }
    case SAAB_CAN_ID::RADIO_TO_SID_TEXT:
    {
        switch (buf[0])
        {
        case 0:
            _messageStatusFlag = 1 << 0;
            break;
        }

        constexpr uint8_t messageCompleteFlag = 1 << 0;

        if (_messageStatusFlag == messageCompleteFlag)
        {
            _messageStatusFlag = 0;
            _isMessageOverrideRequired = true;
        }
    }
    }
}

void SIDMessageHandler::sendTask(void *arg)
{
    uint32_t notifiedValue;
    uint8_t buffer[8];
    uint8_t scrollCounter = 0;

    while (1)
    {
        // if (!_isActive || !isWriteAllowed(2, SID_COMMUNICATION_ID::RADIO))
        // {
        //     vTaskDelay(pdMS_TO_TICKS(50));
        //     continue;
        // }

        if (!_isMessageOverrideRequired)
        {
            vTaskDelay(pdMS_TO_TICKS(250));
            
            scrollCounter++;
            
            if (scrollCounter & 1 == 1)
            {
                _stringScroller.scroll();
                scrollCounter = 0;
            }
        }
        else
        {
            _isMessageOverrideRequired = false;
        }

        uint8_t msgIndex = 0;
        const char *message = _stringScroller.getScrolledString();
        ESP_LOGW(LOG_TAG, "%s", message);
        for (int i = 0; i < 3; i++)
        {
            switch (i)
            {
            case 0:
            {
                buffer[0] = 0x42; // Message order, 7th bit is set to indicate new message
                buffer[1] = 0x96;
                buffer[2] = 0x02; // Row 2
                buffer[3] = message[0];
                buffer[4] = message[1];
                buffer[5] = message[2];
                buffer[6] = message[3];
                buffer[7] = message[4];
                break;
            }
            case 1:
            {
                buffer[0] = 0x01; // Message order
                buffer[1] = 0x96;
                buffer[2] = 0x02;
                buffer[3] = message[5];
                buffer[4] = message[6];
                buffer[5] = message[7];
                buffer[6] = message[8];
                buffer[7] = message[9];
                break;
            }
            case 2:
            {
                buffer[0] = 0x00; // Message order
                buffer[1] = 0x96;
                buffer[2] = 0x02;
                buffer[3] = message[10];
                buffer[4] = message[11];
                buffer[5] = 0;
                buffer[6] = 0;
                buffer[7] = 0;
                break;
            }
            }

            if (i > 0)
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            // ESP_LOGI(LOG_TAG, "Send SID message [%d]", i);
            // _can->send(SAAB_CAN_ID::RADIO_TO_SID_TEXT, buffer);
        }
    }
}

void SIDMessageHandler::notificationTask(void *arg)
{
    while (1)
    {
        if (_dropNotificationAt != 0 && _dropNotificationAt < millis())
        {
            // Restore original message
            memcpy(&_stringScroller, &_stringScrollerCopy, sizeof(StringScroller));
            _dropNotificationAt = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void SIDMessageHandler::sendTaskCb(void *arg)
{
    sidMessageHandlerInstance->sendTask(arg);
}

void SIDMessageHandler::notificationTaskCb(void *arg)
{
    sidMessageHandlerInstance->notificationTask(arg);
}
