#pragma once

#include <Arduino.h>
#include <array>
#include "SaabCAN.hpp"
#include "../../include/defines.h"
#include "StringScroller.hpp"
#include "utf_convert.h"

enum class SID_ID : uint8_t
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
    void requestWrite();
    void activate() { _isActive = true; }
    void disactivate() { _isActive = false; }

private:
    void requestBreakthrough();
    void sendMessage();
    void task(void *arg);
    void scrollTask(void *arg);
    void sendTask(void *arg);
    static void taskCb(void *arg);
    static void scrollTaskCb(void *arg);
    static void sendTaskCb(void *arg);

    bool _isActive = false;
    bool _hasWritePermission = false;
    bool _isBreakthroughRequested = false;
    SaabCAN *_can = NULL;
    StringScroller _stringScroller;
    TaskHandle_t _taskHandle = NULL;
    TaskHandle_t _sendTaskHandle = NULL;

    const char* LOG_TAG = "SID";
};