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
    _sidMessageHandler.addCANInterface(can);
}

void CDCEmulator::start()
{
    i2s_config_t config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = 44100,
        .bits_per_sample = (i2s_bits_per_sample_t)16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LOWMED,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
#ifdef ESP_IDF_4
        .tx_desc_auto_clear = true, // avoiding noise in case of data unavailability
        .fixed_mclk = 0,
        .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
        .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT
#else
        .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
#endif
    };

    _bt.set_i2s_config(config);
    _bt.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST);
    _bt.set_avrc_metadata_callback(avrcMetadataCb);

    xTaskCreatePinnedToCore(taskCb, "CDCMain", 2048, NULL, 2, &_mainTaskHandle, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(statusTaskCb, "CDCStat", 2048, NULL, 2, &_statusTaskHandle, SAAB_TASK_CORE);
    _sidMessageHandler.start();
}

void CDCEmulator::task(void *arg)
{
    uint32_t notifiedValue;
    sendCDCStatus(false);

    while (1)
    {
        // The function will return true if an event was received and false if it timed out.
        auto notification = xTaskNotifyWait(0, ULONG_MAX, &notifiedValue, pdMS_TO_TICKS(1000));
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
        default:
            ESP_LOGW(LOG_TAG, "Received unexpected value %d");
            continue;
        }

        for (int i = 0; i < 4; i++)
        {
            if (i > 0)
            {
                vTaskDelay(pdMS_TO_TICKS(140));
            }

            ESP_LOGI(LOG_TAG, "Send CDC generic status [%d]", i);
            _can->send(SAAB_CAN_ID::CDC_TO_RADIO, ptrToResponse);
            ptrToResponse += 8;
        }
    }
}

void CDCEmulator::avrcMetadata(uint8_t id, const uint8_t *data)
{
    static char buffer[64];

    switch (id)
    {
    case ESP_AVRC_MD_ATTR_ARTIST:
    {
        // size_t len = std::min(_trackInfo.artist.size(), strlen((char *)data));

        // memset(_trackInfo.artist.data(), 0, _trackInfo.artist.size());
        // memcpy(_trackInfo.artist.data(), data, len);
        // memcpy(buffer, _trackInfo.artist.data(), len);
        // _sidMessageHandler.setMessage(_trackInfo.artist.data());
        break;
    }
    case ESP_AVRC_MD_ATTR_TITLE:
    {
        // size_t len = std::min(_trackInfo.artist.size(), strlen((char *)data));

        // memcpy(_trackInfo.title.data(), data, std::min(_trackInfo.title.size(), strlen((char *)data)));
        // memcpy(buffer, _trackInfo.artist.data(), len);
        // _sidMessageHandler->setMessage(buffer);
        break;
    }
    }
}

void CDCEmulator::receive(SAAB_CAN_ID id, uint8_t *buf)
{
    switch (id)
    {
    case SAAB_CAN_ID::CDC_CONTROL:
        handleRadioCommand(id, buf);
        ESP_LOGD(LOG_TAG, "MSG: CDC_CONTROL");
        break;
    case SAAB_CAN_ID::RADIO_TO_CDC:
        handleCDCStatusRequest(id, buf);
        ESP_LOGD(LOG_TAG, "MSG: RADIO_TO_CDC");
        break;
    }
}

/**
 * Commands such as power on, off, next track, mute...
 */
void CDCEmulator::handleRadioCommand(SAAB_CAN_ID id, uint8_t *buf)
{
    // This will not work for long button presses as the CHANGED status will be 0 in that case
    if (buf[0] == RADIO_COMMAND_0::CHANGED)
    {
        switch (buf[1])
        {
        case RADIO_COMMAND_1::POWER_ON:
            _isEnabled = true;
            if (!_bt.is_running())
            {
                _bt.set_auto_reconnect(true, 10);
                _bt.start("Draken Audio");
                _sidMessageHandler.setMessage("Draken Audio - Bluetooth for Saab");
                _sidMessageHandler.activate();
            }
            else if (!_bt.is_connected())
            {
                _bt.reconnect();
            }
            xTaskNotify(_mainTaskHandle, 0, eNoAction); // Send CDC status
            break;
        case RADIO_COMMAND_1::POWER_OFF:
            _isEnabled = false;
            _sidMessageHandler.disactivate();
            _bt.end();
            xTaskNotify(_mainTaskHandle, 0, eNoAction); // Send CDC status
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
                case 1:
                    ESP.restart();
                    break;
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

void CDCEmulator::handleCDCStatusRequest(SAAB_CAN_ID id, uint8_t *buf)
{
    if (_statusTaskHandle != NULL)
    {
        uint8_t value = buf[3] & 0x0f;
        xTaskNotify(_statusTaskHandle, value, eSetValueWithOverwrite);
    }
    else
    {
        ESP_LOGE(LOG_TAG, "CDC status request failed as the task has not started");
    }
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

    ESP_LOGI(LOG_TAG, "Send CDC status");
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

void CDCEmulator::avrcMetadataCb(uint8_t id, const uint8_t *data)
{
    cdcInstance->avrcMetadata(id, data);
}
