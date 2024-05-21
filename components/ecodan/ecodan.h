#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "proto.h"
#include "status.h"

namespace esphome {
namespace ecodan 
{    
    static constexpr const char *TAG = "ecodan.component";   

    class EcodanHeatpump : public PollingComponent {
    public:        
        EcodanHeatpump() : PollingComponent(1000) {}
        void set_rx(int rx);
        void set_tx(int tx);
        void setup() override;
        void update() override;
        void dump_config() override;    
    
        void register_sensor(sensor::Sensor *obj, const std::string& key) {
            sensors[key] = obj;
        }

        void register_textSensor(text_sensor::TextSensor *obj, const std::string& key) {
            textSensors[key] = obj;
        }

        void register_binarySensor(binary_sensor::BinarySensor *obj, const std::string& key) {
            binarySensors[key] = obj;
        }

        // exposed as external component commands
        void set_room_temperature(float value, esphome::ecodan::SetZone zone);
        void set_flow_target_temperature(float value, esphome::ecodan::SetZone zone);
        void set_dhw_target_temperature(float value);
        void set_dhw_mode(std::string mode);
        void set_dhw_force(bool on);
        void set_holiday(bool on);
        void set_power_mode(bool on);
        void set_hp_mode(int mode);
        void set_controller_mode(CONTROLLER_FLAG flag, bool on);

    protected:
        std::map<std::string, sensor::Sensor*> sensors;
        std::map<std::string, text_sensor::TextSensor*> textSensors;
        std::map<std::string, binary_sensor::BinarySensor*> binarySensors;

        // publish func
        void publish_state(const std::string& sensorKey, float sensorValue);
        void publish_state(const std::string& sensorKey, const std::string& sensorValue);
        void publish_state(const std::string& sensorKey, bool sensorValue);

        bool begin_connect();
        bool begin_update_status();

        bool initialize();
        void init_hw_watchdog();
        void handle_loop();
        bool is_connected();        
    
    private:
        HardwareSerial& port = Serial1;
        std::mutex portWriteMutex;
        uint8_t serialRxPort{2};
        uint8_t serialTxPort{1};

        Status status;
        float temperatureStep = 0.5f;
        bool connected = false;
        bool heatpumpInitialized = false;
        
        std::queue<Message> cmdQueue;
        std::mutex cmdQueueMutex;

        void resync_rx();
        bool serial_rx(Message& msg);
        bool serial_tx(Message& msg);

        bool dispatch_next_status_cmd();
        bool dispatch_next_set_cmd();
        bool schedule_cmd(Message& cmd);
        
        void handle_response();
        void handle_get_response(Message& res);
        void handle_set_response(Message& res);
        void handle_connect_response(Message& res);

        void serial_rx_thread();
    };

} // namespace ecodan
} // namespace esphome
