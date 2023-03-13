#define DEBUG_MODE 0

#include <Arduino.h>
#include "CDCEmulator.hpp"
#include "SaabCAN.hpp"

SaabCAN can;
CDCEmulator cdcEmulator;

void setup() {
  can.start();
  cdcEmulator.addCANInterface(&can);
  cdcEmulator.start();
}

void loop() {
  // This is a task that runs with priority 0 on core 1
}