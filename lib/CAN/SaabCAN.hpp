#pragma once

#include <array>
#include "mcp_can.h"
#include "../../include/defines.h"

#define CAN_CS_PIN 5

#define I_BUS CAN_47KBPS
#define P_BUS CAN_500KBPS

#define SAAB_CAN_MSG_LENGTH 8

enum class SAAB_CAN_ID
{
    IBUS_BUTTONS = 0x290,
    RADIO_MSG = 0x328,
    RADIO_PRIORITY = 0x348,
    O_SID_MSG = 0x33F,
    O_SID_PRIORITY = 0x358,
    TEXT_PRIORITY = 0x368,
    CDC_CONTROL = 0x3C0,
    CDC_INFO = 0x3C8,
    LIGHTING = 0x410,
    SOUND_REQUEST = 0x430,
    SPEED_RPM = 0x460,
    RADIO_TO_CDC = 0x6A1,
    CDC_TO_RADIO = 0x6A2
};

enum SAAB_CAN_LISTENER_TYPE : uint8_t
{
    CDC = 0,
    N_TYPES // Special type for keeping track of the number of listeners
};

class SaabCANListener
{
public:
    virtual void receive(SAAB_CAN_ID id, uint8_t len, uint8_t *buf) = 0;
};

struct SaabCANMessage
{
    SAAB_CAN_ID id;
    uint8_t data[SAAB_CAN_MSG_LENGTH];
};

class SaabCAN
{
public:
    SaabCAN();
    ~SaabCAN();
    void start();
    void send(SAAB_CAN_ID id, const uint8_t *buf);
    void addListener(SaabCANListener *listener, SAAB_CAN_LISTENER_TYPE type);

private:
    void receiveTask(void *arg);
    void sendTask(void *arg);
    static void receiveTaskCb(void *arg);
    static void sendTaskCb(void *arg);

    std::array<SaabCANListener *, SAAB_CAN_LISTENER_TYPE::N_TYPES> _listeners;

    TaskHandle_t _receiveTaskHandle;
    TaskHandle_t _sendTaskHandle;
    QueueHandle_t _queue;
    SemaphoreHandle_t _mutex;

    MCP_CAN _mcp;
    SPIClass *_spi;

    const char* LOG_TAG = "CAN";
};