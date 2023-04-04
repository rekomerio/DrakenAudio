#include "BluetoothAudio.hpp"

void BluetoothAudio::start(const char *name)
{
    BluetoothA2DPSink::start(name);
    _is_running = true;
    pinMode(GPIO_NUM_2, OUTPUT);
    digitalWrite(GPIO_NUM_2, HIGH);
}

void BluetoothAudio::end(bool release_memory)
{
    BluetoothA2DPSink::end(release_memory);
    _is_running = false;
    digitalWrite(GPIO_NUM_2, LOW);
}
