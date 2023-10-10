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
    SPA = 0x12
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

private:
    void requestWrite();
    void sendMessage();
    void requestWriteTask(void *arg);
    void scrollTask(void *arg);
    void sendTask(void *arg);
    static void requestWriteTaskCb(void *arg);
    static void scrollTaskCb(void *arg);
    static void sendTaskCb(void *arg);

    bool _isActive = false;
    bool _hasWritePermission = false;
    bool _isOtherDeviceTryingToWrite = false;
    SaabCAN *_can = NULL;
    StringScroller _stringScroller;
    TaskHandle_t _requestWriteTaskHandle = NULL;
    TaskHandle_t _sendTaskHandle = NULL;

    const char* LOG_TAG = "SID";
};