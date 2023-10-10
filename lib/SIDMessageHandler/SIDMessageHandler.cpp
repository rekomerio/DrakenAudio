#include "SIDMessageHandler.hpp"

SIDMessageHandler *sidMessageHandlerInstance;

SIDMessageHandler::SIDMessageHandler()
{
    sidMessageHandlerInstance = this;
}

void SIDMessageHandler::start()
{
    xTaskCreatePinnedToCore(requestWriteTaskCb, "SIDReqWriteTask", 2048, NULL, 1, &_requestWriteTaskHandle, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(scrollTaskCb, "SIDScrollTask", 2048, NULL, 1, NULL, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(sendTaskCb, "SIDSendTask", 2048, NULL, 1, &_sendTaskHandle, SAAB_TASK_CORE);
}

void SIDMessageHandler::setMessage(const char *message)
{
    static char buffer[64];
    utf_convert(message, buffer, strlen(message));
    _stringScroller.setString(message, SID_MAX_CHARACTERS);
}

void SIDMessageHandler::addCANInterface(SaabCAN *can)
{
    _can = can;
    _can->addListener(this, SAAB_CAN_LISTENER_TYPE::SID);
}

void SIDMessageHandler::requestWrite()
{
    static uint8_t buf[8] = {0x1F, 0x02, 0x00, static_cast<uint8_t>(SID_COMMUNICATION_ID::SPA), 0x00, 0x00, 0x00, 0x00};

    if (_isActive)
    {
        buf[2] = (_isOtherDeviceTryingToWrite ? 0x01 : 0x05);
    }
    else
    {
        buf[2] = 0xFF;
    }

    _can->send(SAAB_CAN_ID::SPA_TO_SID_CONTROL, buf);
}

void SIDMessageHandler::receive(SAAB_CAN_ID id, uint8_t *buf)
{
    if (!_isActive)
    {
        return;
    }

    switch (id)
    {
    case SAAB_CAN_ID::TEXT_PRIORITY:
    {
        bool writeAccessReceived = buf[0] == 0x02 && buf[1] == static_cast<uint8_t>(SID_COMMUNICATION_ID::SPA);
        if (writeAccessReceived)
        {
            // We want to display message and have received permission to do so
            sendMessage();
        }
        break;
    }
    case SAAB_CAN_ID::RADIO_PRIORITY:
    {
        bool radioRequestedWrite = buf[2] == 0x03 || buf[2] == 0x05;
        if (radioRequestedWrite)
        {
            // Radio wants to write to SID, but so do we, lets override it
            _isOtherDeviceTryingToWrite = true;
            xTaskNotify(_requestWriteTaskHandle, 0, eNoAction);
        }
        break;
    }
    }
}

void SIDMessageHandler::sendMessage()
{
    xTaskNotify(_sendTaskHandle, 0, eNoAction);
}

void SIDMessageHandler::requestWriteTask(void *arg)
{
    uint32_t notifiedValue;
    while (1)
    {
        requestWrite();
        _isOtherDeviceTryingToWrite = false;
        xTaskNotifyWait(0, ULONG_MAX, &notifiedValue, pdMS_TO_TICKS(1000));
    }
}

void SIDMessageHandler::scrollTask(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(350));

        if (_isActive)
        {
            _stringScroller.scroll();
        }
    }
}

void SIDMessageHandler::sendTask(void *arg)
{
    uint32_t notifiedValue;
    uint8_t buffer[8];

    while (1)
    {
        xTaskNotifyWait(0, ULONG_MAX, &notifiedValue, portMAX_DELAY);

        uint8_t msgIndex = 0;
        const char *message = _stringScroller.getScrolledString();
        for (int i = 0; i < 3; i++)
        {
            switch (i)
            {
            case 0:
            {
                buffer[0] = 0x42; // Message order, 7th bit is set to indicate new message
                buffer[1] = 0x96;
                buffer[2] = 0x82; // Row 2 + some bit to indicate the text has changed?
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
                buffer[2] = 0x82;
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
                buffer[2] = 0x82;
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
                vTaskDelay(pdMS_TO_TICKS(20));
            }

            ESP_LOGI(LOG_TAG, "Send SID message [%d]", i);
            _can->send(SAAB_CAN_ID::SPA_TO_SID_TEXT, buffer);
        }
    }
}

void SIDMessageHandler::requestWriteTaskCb(void *arg)
{
    sidMessageHandlerInstance->requestWriteTask(arg);
}

void SIDMessageHandler::scrollTaskCb(void *arg)
{
    sidMessageHandlerInstance->scrollTask(arg);
}

void SIDMessageHandler::sendTaskCb(void *arg)
{
    sidMessageHandlerInstance->sendTask(arg);
}
