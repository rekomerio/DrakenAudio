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

// NOTE: Only these two CAN messages can be received when using following acceptance code and mask
constexpr uint32_t acceptanceCode = static_cast<uint32_t>(SAAB_CAN_ID::CDC_CONTROL) << 21 | static_cast<uint32_t>(SAAB_CAN_ID::RADIO_TO_CDC) << 5;
// Set all bits to zero that we should care about in the ID's
constexpr uint32_t acceptanceMask = ~((0x7ff << 21) | (0x7ff << 5));

twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_19, GPIO_NUM_18, TWAI_MODE_TO_USE);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_47_619KBITS();
twai_filter_config_t f_config = {.acceptance_code = acceptanceCode, .acceptance_mask = acceptanceMask, .single_filter = false}; // TWAI_FILTER_CONFIG_ACCEPT_ALL();

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
  message.identifier = static_cast<uint32_t>(SAAB_CAN_ID::CDC_CONTROL);

  memcpy(message.data, buf, SAAB_CAN_MSG_LENGTH);
  can.send(&message);
  vTaskDelay(500);
  // This should be visible in the received messages
  message.identifier = static_cast<uint32_t>(SAAB_CAN_ID::RADIO_TO_CDC);
  can.send(&message);
  vTaskDelay(500);
  // This should not be received since it is not allowed by the filter
  message.identifier = static_cast<uint32_t>(SAAB_CAN_ID::O_SID_MSG);
  can.send(&message);
  vTaskDelay(500);
#endif
}