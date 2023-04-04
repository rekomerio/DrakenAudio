#pragma once

#include "BluetoothA2DPSinkQueued.h"

class BluetoothAudio : public BluetoothA2DPSinkQueued
{
public:
    void start(const char *name) override;
    void end(bool release_memory = false) override;
    
    inline bool is_running() { return _is_running; }

private:
    bool _is_running = false;
};