#pragma once

#include "BluetoothA2DPSink.h"

class BluetoothAudio : public BluetoothA2DPSink
{
public:
    void start(const char *name) override;
    void end(bool release_memory = false) override;
    
    inline bool is_running() { return _is_running; }

private:
    bool _is_running = false;
};