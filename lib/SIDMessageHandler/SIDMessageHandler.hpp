#pragma once

#include <Arduino.h>
#include <array>
#include "SaabCAN.hpp"
#include "../../include/defines.h"
#include "StringScroller.hpp"
#include "utf_convert.h"

#define SID_MAX_CHARACTERS 12

enum class SID_COMMUNICATION_ID : uint8_t
{
    SPA = 0x12,
    RADIO = 0x19,
    TRIONIC = 0x21,
    ACC = 0x23,
    TWICE = 0x2D,
    OPEN_SID = 0x32,
    NO_DISPLAY = 0xFF
};

class SIDMessageHandler : public SaabCANListener
{
public:
    SIDMessageHandler();
    void start();
    void setMessage(const char *message);
    void receive(SAAB_CAN_ID id, uint8_t *buf);
    void addCANInterface(SaabCAN *can);
    void activate() { _isActive = true; }
    void disactivate() { _isActive = false; }
    void showNotification(const char *message, int durationMs);

private:
    bool isWriteAllowed(uint8_t row, SID_COMMUNICATION_ID communicationId);
    void sendMessage();
    void scrollTask(void *arg);
    void sendTask(void *arg);
    void notificationTask(void *arg);
    static void scrollTaskCb(void *arg);
    static void sendTaskCb(void *arg);
    static void notificationTaskCb(void *arg);

    bool _isMessageOverrideRequired = false;
    bool _isActive = false;
    bool _hasWritePermission = false;
    bool _isOtherDeviceTryingToWrite = false;
    uint8_t _messageStatusFlag = 0;
    int _dropNotificationAt = 0;
    char _buffer[SID_MESSAGE_BUFFER_SIZE];
    SaabCAN *_can = NULL;
    StringScroller _stringScroller;
    StringScroller _stringScrollerCopy;
    TaskHandle_t _requestWriteTaskHandle = NULL;
    TaskHandle_t _sendTaskHandle = NULL;
    std::array<SID_COMMUNICATION_ID, 3> _writeAccessDevice;

    const char *LOG_TAG = "SID";
};