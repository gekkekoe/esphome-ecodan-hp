#include "ecodan.h"

namespace esphome {
namespace ecodan 
{ 
    void EcodanHeatpump::setup() {
        // ESP_LOGI(TAG, "register services"); 
        // register_service(&EcodanHeatpump::set_room_temperature, "set_room_temperature", {"newTemp", "zone"});
        // register_service(&EcodanHeatpump::set_flow_target_temperature, "set_flow_target_temperature", {"newTemp", "zone"});
        // register_service(&EcodanHeatpump::set_dhw_target_temperature, "set_dhw_target_temperature", {"newTemp"});
        // register_service(&EcodanHeatpump::set_dhw_mode, "set_dhw_mode", {"mode"});
        // register_service(&EcodanHeatpump::set_dhw_force, "set_dhw_force", {"on"});
        // register_service(&EcodanHeatpump::set_power_mode, "set_power_mode", {"on"});
        // register_service(&EcodanHeatpump::set_hp_mode, "set_hp_mode", {"mode"});        

        heatpumpInitialized = initialize();
        xTaskCreate(
            [](void* o){ static_cast<EcodanHeatpump*>(o)->serial_rx_thread(); },
            "serial_rx_task",
            8*1024,
            this,
            5,
            NULL);        
    }


    void EcodanHeatpump::publish_state(const std::string& sensorKey, float sensorValue) {
        auto sensor_it = sensors.find(sensorKey);
        if (sensor_it != sensors.end()) {
            sensor_it->second->publish_state(sensorValue);
        } 
        else 
        {
            ESP_LOGI(TAG, "Could not publish state of sensor '%s' with value: '%f'", sensorKey.c_str(), sensorValue);
        }
    }

    void EcodanHeatpump::publish_state(const std::string& sensorKey, const std::string& sensorValue) {        
        auto textSensor_it = textSensors.find(sensorKey);
        if (textSensor_it != textSensors.end()) {
            textSensor_it->second->publish_state(sensorValue);
        }
        else 
        {
            ESP_LOGI(TAG, "Could not publish state of sensor '%s' with value: '%s'", sensorKey.c_str(), sensorValue.c_str());
        }
    }

    void EcodanHeatpump::publish_state(const std::string& sensorKey, bool sensorValue) {
        auto binarySensor_it = binarySensors.find(sensorKey);
        if (binarySensor_it != binarySensors.end()) {
            binarySensor_it->second->publish_state(sensorValue);
        }
        else 
        {
            ESP_LOGI(TAG, "Could not publish state of sensor '%s' with value: '%d'", sensorKey.c_str(), sensorValue);
        }
    }

    void EcodanHeatpump::update() {        
        //ESP_LOGI(TAG, "Update() on core %d", xPortGetCoreID());
        if (heatpumpInitialized)
            handle_loop();            
    }

    void EcodanHeatpump::dump_config() {
        ESP_LOGI(TAG, "config"); 
    }

#pragma region Configuration

    void EcodanHeatpump::set_rx(int rx) { 
        serialRxPort = rx; 
    }

    void EcodanHeatpump::set_tx(int tx) { 
        serialTxPort = tx; 
    }

#pragma endregion Configuration

#pragma region Init

    bool EcodanHeatpump::initialize()
    {
        ESP_LOGI(TAG, "Initializing HeatPump with serial rx: %d, tx: %d", (int8_t)serialRxPort, (int8_t)serialTxPort);

        pinMode(serialRxPort, INPUT_PULLUP);
        pinMode(serialTxPort, OUTPUT);

        delay(25); // There seems to be a window after setting the pin modes where trying to use the UART can be flaky, so introduce a short delay

        port.begin(2400, SERIAL_8E1, serialRxPort, serialTxPort);
        if (!is_connected())
            begin_connect();
        init_hw_watchdog();        
        return true;
    }

    void EcodanHeatpump::handle_loop()
    {         
        if (!is_connected() && !port.available())
        {
            static auto last_attempt = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_attempt > std::chrono::seconds(5))
            {
                last_attempt = now;
                if (!begin_connect())
                {
                    ESP_LOGI(TAG, "Failed to start heatpump connection proceedure...");
                }
            }    
        }
        else if (is_connected())
        {
            dispatch_next_set_cmd();

            if (!dispatch_next_status_cmd())
            {
                ESP_LOGI(TAG, "Failed to begin heatpump status update!");
            }
        }
    }

    bool EcodanHeatpump::is_connected()
    {
        return connected;
    }


#pragma endregion Init

} // namespace ecodan
} // namespace esphome
