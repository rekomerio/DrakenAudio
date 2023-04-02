#include <Arduino.h>
#include "CDCEmulator.hpp"
#include "SaabCAN.hpp"

#define SELF_TEST 0

#if SELF_TEST
#define TWAI_MODE_TO_USE TWAI_MODE_NO_ACK
#else
#define TWAI_MODE_TO_USE TWAI_MODE_NORMAL
#endif

SaabCAN can;
CDCEmulator cdcEmulator;

constexpr uint32_t acceptanceCode = static_cast<uint32_t>(SAAB_CAN_ID::CDC_CONTROL) << 21 | static_cast<uint32_t>(SAAB_CAN_ID::RADIO_TO_CDC) << 5;

twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_19, GPIO_NUM_18, TWAI_MODE_TO_USE);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_47_619KBITS();
twai_filter_config_t f_config = {.acceptance_code = acceptanceCode, .acceptance_mask = 0xFFFFFFFF, .single_filter = false}; // TWAI_FILTER_CONFIG_ACCEPT_ALL();

void setup()
{
  can.start(&g_config, &t_config, &f_config);
  cdcEmulator.addCANInterface(&can);
  cdcEmulator.start();
}

// This is a task that runs with priority 0 on core 1
void loop()
{
#if SELF_TEST
  twai_message_t message;
  uint8_t buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  message.extd = 0; // 11 bit id
  message.rtr = 0;
  message.self = 1;
  message.ss = 0;
  message.dlc_non_comp = 0;
  message.data_length_code = SAAB_CAN_MSG_LENGTH;
  message.identifier = static_cast<uint32_t>(SAAB_CAN_ID::IBUS_BUTTONS);

  memcpy(message.data, buf, SAAB_CAN_MSG_LENGTH);

  can.send(&message);
#else
  if (millis() - cdcEmulator.lastRelevantMessageReceivedAt > 2500)
  {
    const int sleepInSeconds = 3;
    esp_sleep_enable_timer_wakeup(sleepInSeconds * 1000000);
    ESP_LOGI("SLEEP", "Entering deep sleep...");
    esp_deep_sleep_start();
  }
#endif
}