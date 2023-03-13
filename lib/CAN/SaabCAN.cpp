#include "SaabCAN.hpp"

SaabCAN *canInstance;

SaabCAN::SaabCAN() : _mcp(_spi = new SPIClass(), CAN_CS_PIN)
{
    canInstance = this;
    _listeners.fill(nullptr);
}

SaabCAN::~SaabCAN()
{
    delete _spi;
}

void SaabCAN::start()
{
    _mutex = xSemaphoreCreateMutex();
    _queue = xQueueCreate(3, sizeof(SaabCANMessage));

    if (_mutex == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to create the mutex");
    }

    if (_queue == NULL)
    {
        ESP_LOGE(LOG_TAG, "Failed to create the queue");
    }

    while (_mcp.begin(MCP_ANY, I_BUS, MCP_8MHZ) != CAN_OK)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        ESP_LOGI(LOG_TAG, "Waiting for MCP...");
    }

    _mcp.setMode(MCP_NORMAL);

    xTaskCreatePinnedToCore(receiveTaskCb, "CANReceive", 2048, NULL, 1, &_receiveTaskHandle, SAAB_TASK_CORE);
    xTaskCreatePinnedToCore(sendTaskCb, "CANSend", 2048, NULL, 3, &_sendTaskHandle, SAAB_TASK_CORE);
}

void SaabCAN::send(SAAB_CAN_ID id, const uint8_t *buf)
{
    constexpr TickType_t delay = pdMS_TO_TICKS(20);

    SaabCANMessage msg;
    msg.id = id;
    memcpy(&msg.data, buf, SAAB_CAN_MSG_LENGTH);

    ESP_LOGD(LOG_TAG, "Queueing message");
    if (xQueueSend(_queue, &msg, delay) != pdTRUE)
    {
        ESP_LOGE(LOG_TAG, "Failed to queue CAN message %x", static_cast<unsigned long>(id));
    }
}

void SaabCAN::addListener(SaabCANListener *listener, SAAB_CAN_LISTENER_TYPE type)
{
    _listeners[type] = listener;
}

void SaabCAN::receiveTask(void *arg)
{
    uint8_t len;
    uint8_t data[8];
    long unsigned id;

    while (1)
    {
        ESP_LOGD(LOG_TAG, "Checking CAN messages");
        xSemaphoreTake(_mutex, portMAX_DELAY);

        if (_mcp.checkReceive() == CAN_MSGAVAIL && _mcp.readMsgBuf(&id, &len, data) == CAN_OK)
        {
            for (int i = 0; i < _listeners.size(); i++)
            {
                if (_listeners[i] != nullptr)
                {
                    _listeners[i]->receive(static_cast<SAAB_CAN_ID>(id), len, data);
                }
            }
        }

        xSemaphoreGive(_mutex);
    }
}

void SaabCAN::sendTask(void *arg)
{
    SaabCANMessage msg;
    while (1)
    {
        xQueueReceive(_queue, &msg, portMAX_DELAY);
        ESP_LOGD(LOG_TAG, "Sending message from queue");
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _mcp.sendMsgBuf(static_cast<unsigned long>(msg.id), SAAB_CAN_MSG_LENGTH, msg.data);
        xSemaphoreGive(_mutex);
    }
}

void SaabCAN::receiveTaskCb(void *arg)
{
    canInstance->receiveTask(arg);
}

void SaabCAN::sendTaskCb(void *arg)
{
    canInstance->sendTask(arg);
}