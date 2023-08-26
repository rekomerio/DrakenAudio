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
    // Custom implementation to not clear the last connected device from memory
    // Start code from BluetoothA2DPSink
    is_autoreconnect_allowed = false;
    // Start code from BluetoothA2DPCommon
    is_start_disabled = false;
    log_free_heap();

    // Disconnect
    disconnect();
    while (is_connected()) {
        delay(100);
    }

    // deinit AVRC
    ESP_LOGI(BT_AV_TAG,"deinit avrc");
    if (esp_avrc_ct_deinit() != ESP_OK){
         ESP_LOGE(BT_AV_TAG,"Failed to deinit avrc");
    }
    log_free_heap();
    // End code from BluetoothA2DPCommon
    app_task_shut_down();

    // stop I2S
    if (is_i2s_output){
        ESP_LOGI(BT_AV_TAG,"uninstall i2s");
        if (i2s_driver_uninstall(i2s_port) != ESP_OK){
            ESP_LOGE(BT_AV_TAG,"Failed to uninstall i2s");
        }
        else {
            player_init = false;
        }
    }
    log_free_heap();
    // End code from BluetoothA2DPSink
    _is_running = false;
    digitalWrite(GPIO_NUM_2, LOW);
}
