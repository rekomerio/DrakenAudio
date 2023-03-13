#include "CDCEmulator.hpp"

CDCEmulator *cdcInstance;

CDCEmulator::CDCEmulator()
{
    cdcInstance = this;
}

void CDCEmulator::addCANInterface(SaabCAN *can)
{
    _can = can;
    _can->addListener(this, SAAB_CAN_LISTENER_TYPE::CDC);
}

void CDCEmulator::start()
{
    _bt.start("SAAB Draken");

    xTaskCreatePinnedToCore(taskCb, "CDCMain", 2048, NULL, 2, &_mainTaskHandle, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(statusTaskCb, "CDCStat", 2048, NULL, 2, &_statusTaskHandle, SAAB_TASK_CORE);
}

void CDCEmulator::task(void *arg)
{
    uint32_t notifiedValue;
    TickType_t delay = pdMS_TO_TICKS(1000);

    sendCDCStatus(false);

    while (1)
    {
        // The function will return true if an event was received and false if it timed out.
        auto notification = xTaskNotifyWait(0, ULONG_MAX, &notifiedValue, delay);
        bool isNotification = notification == pdTRUE;
        sendCDCStatus(isNotification);
    }
}

void CDCEmulator::statusTask(void *arg)
{
    static uint8_t activeResponse[32] = {
        0x32, 0x00, 0x00, 0x16, 0x01, 0x02, 0x00, 0x00,
        0x42, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00,
        0x52, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00,
        0x62, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00};

    static uint8_t powerOnResponse[32] = {
        0x32, 0x00, 0x00, 0x03, 0x01, 0x02, 0x00, 0x00,
        0x42, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00,
        0x52, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00,
        0x62, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00};

    static uint8_t powerDownResponse[32] = {
        0x32, 0x00, 0x00, 0x19, 0x01, 0x00, 0x00, 0x00,
        0x42, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00,
        0x52, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00,
        0x62, 0x00, 0x00, 0x38, 0x01, 0x00, 0x00, 0x00};

    uint8_t *ptrToResponse;

    uint32_t notifiedValue;
    constexpr TickType_t delay = pdMS_TO_TICKS(140);

    while (true)
    {
        xTaskNotifyWait(0, ULONG_MAX, &notifiedValue, portMAX_DELAY);

        switch (notifiedValue)
        {
        case 2:
            ptrToResponse = activeResponse;
            break;
        case 3:
            ptrToResponse = powerOnResponse;
            break;
        case 8:
            ptrToResponse = powerDownResponse;
            break;
        }

        for (int i = 0; i < 4; i++)
        {
            if (i > 0)
            {
                vTaskDelay(delay);
            }

            _can->send(SAAB_CAN_ID::CDC_TO_RADIO, ptrToResponse);
            ptrToResponse += 8;
        }

        ESP_LOGD(LOG_TAG, "Send CDC generic status");
    }
}

void CDCEmulator::receive(SAAB_CAN_ID id, uint8_t len, uint8_t *buf)
{
    switch (id)
    {
    case SAAB_CAN_ID::CDC_CONTROL:
        handleRadioCommand(id, len, buf);
        break;
    case SAAB_CAN_ID::RADIO_TO_CDC:
        handleCDCStatusRequest(id, len, buf);
        break;
    }
}

/**
 * Commands such as power on, off, next track, mute...
 */
void CDCEmulator::handleRadioCommand(SAAB_CAN_ID id, uint8_t len, uint8_t *buf)
{
    // This will not work for long button presses as the CHANGED status will be 0 in that case
    if (buf[0] == RADIO_COMMAND_0::CHANGED)
    {
        switch (buf[1])
        {
        case RADIO_COMMAND_1::POWER_ON:
            _isEnabled = true;
            _bt.reconnect();
            xTaskNotify(_mainTaskHandle, 0, eNoAction);
            break;
        case RADIO_COMMAND_1::POWER_OFF:
            _isEnabled = false;
            _bt.stop();
            _bt.disconnect();
            xTaskNotify(_mainTaskHandle, 0, eNoAction);
            break;
        }

        if (_isEnabled)
        {
            switch (buf[1])
            {
            case RADIO_COMMAND_1::NXT:
                _bt.play();
                break;
            case RADIO_COMMAND_1::SEEK_NEXT:
                _bt.next();
                break;
            case RADIO_COMMAND_1::SEEK_PREV:
                _bt.previous();
                break;
            case RADIO_COMMAND_1::IHU_BTN:
                switch (buf[2])
                {
                case 3:
                    _bt.reconnect();
                    break;
                case 6:
                    _bt.disconnect();
                    break;
                }
                break;
            }
        }
    }
}

void CDCEmulator::handleCDCStatusRequest(SAAB_CAN_ID id, uint8_t len, uint8_t *buf)
{
    uint8_t value = buf[3] & 0x0f;
    xTaskNotify(_statusTaskHandle, value, eSetValueWithOverwrite);
}

void CDCEmulator::sendCDCStatus(bool hasStatusChanged)
{
    /* Format of GENERAL_STATUS_CDC frame:
     ID: CDC node ID
     [0]:
     byte 0, bit 7: FCI NEW DATA: 0 - sent on base time, 1 - sent on event
     byte 0, bit 6: FCI REMOTE CMD: 0 - status change due to internal operation, 1 - status change due to CDC_COMMAND frame
     byte 0, bit 5: FCI DISC PRESENCE VALID: 0 - disc presence signal is not valid, 1 - disc presence signal is valid
     [1]: Disc presence validation (boolean)
     byte 1-2, bits 0-15: DISC PRESENCE: (bitmap) 0 - disc absent, 1 - disc present. Bit 0 is disc 1, bit 1 is disc 2, etc.
     [2]: Disc presence (bitmap)
     byte 1-2, bits 0-15: DISC PRESENCE: (bitmap) 0 - disc absent, 1 - disc present. Bit 0 is disc 1, bit 1 is disc 2, etc.
     [3]: Disc number currently playing
     byte 3, bits 7-4: DISC MODE
     byte 3, bits 3-0: DISC NUMBER
     [4]: Track number currently playing
     [5]: Minute of the current track
     [6]: Second of the current track
     [7]: CD changer status; D0 = Married to the car
     */

    static uint8_t buffer[8] = {0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xD0};
    buffer[0] = (hasStatusChanged ? 0xE0 : 0x20);
    buffer[1] = (_isEnabled ? 0xFF : 0x00); // Validation for presence of six discs in the magazine
    buffer[2] = (_isEnabled ? 0x3F : 0x01); // There are six discs in the magazine
    buffer[3] = (_isEnabled ? 0x41 : 0x01); // ToDo: check 0x01 | (discMode << 4) | 0x01

    ESP_LOGD(LOG_TAG, "Send CDC status");
    _can->send(SAAB_CAN_ID::CDC_INFO, buffer);
}

void CDCEmulator::taskCb(void *arg)
{
    cdcInstance->task(arg);
}

void CDCEmulator::statusTaskCb(void *arg)
{
    cdcInstance->statusTask(arg);
}