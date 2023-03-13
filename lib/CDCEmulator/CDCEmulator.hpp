#pragma once

#include "BluetoothAudio.hpp"
#include "SaabCAN.hpp"

enum RADIO_COMMAND_0
{
	CHANGED = 0x80
};

enum RADIO_COMMAND_1
{
	POWER_OFF = 0x14,
	POWER_ON = 0x24,
	SEEK_NEXT = 0x35,
	SEEK_PREV = 0x36,
	SEEK_NEXT_LONG = 0x45,
	SEEK_PREV_LONG = 0x46,
	SEEK_BOTH_LONG = 0x84,
	SEEK_BOTH_EXTRA_LONG = 0x88,
	NXT = 0x59,
	IHU_BTN = 0x68, // In this case, the byte[2] will tell which button was pressed. Its value will be from 1 to 6
	RANDOM = 0x76,  // Long press of CD/RDM button
	MUTE = 0xB0,
	UNMUTE = 0xB1,
};

class CDCEmulator : public SaabCANListener 
{
public:
	CDCEmulator();
	void start();
	void receive(SAAB_CAN_ID id, uint8_t len, uint8_t *buf);
    void addCANInterface(SaabCAN *can);

private:
	void handleRadioCommand(SAAB_CAN_ID id, uint8_t len, uint8_t *buf);
	void handleCDCStatusRequest(SAAB_CAN_ID id, uint8_t len, uint8_t *buf);
	void sendCDCStatus(bool hasStatusChanged);
	void task(void *arg);
	void statusTask(void *arg);
	static void taskCb(void *arg);
	static void statusTaskCb(void *arg);

	bool _isEnabled = false;
	BluetoothAudio _bt;
	SaabCAN *_can = nullptr;
	TaskHandle_t _mainTaskHandle;
	TaskHandle_t _statusTaskHandle;

	const char* LOG_TAG = "CDC";
};