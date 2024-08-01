#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
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
        EcodanHeatpump() : PollingComponent() {}
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
        void set_dhw_mode(Status::DhwMode dhwMode);
        void set_dhw_force(bool on);
        void set_holiday(bool on);
        void set_power_mode(bool on);
        void set_hp_mode(int mode);
        void set_controller_mode(CONTROLLER_FLAG flag, bool on);

        const Status& get_status() const { return status; }

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

    class EcodanClimate : public climate::Climate, public PollingComponent  {
    public:
        EcodanClimate() { }
        void setup() override;
        void update() override;
        void control(const climate::ClimateCall &call) override;
        climate::ClimateTraits traits() override;

        // set fuctions for thermostat
        void set_target_temp_func(std::function<void(float)> target_temp_func) { set_target_temp = target_temp_func; };
        void set_get_current_temp_func(std::function<float(void)> current_temp_func) { get_current_temp = current_temp_func; };
        void set_get_target_temp_func(std::function<float(void)> target_temp_func) { get_target_temp = target_temp_func; };
        void set_cooling_func(std::function<void(void)> switch_cooling_func) { set_cooling_mode = switch_cooling_func; };
        void set_heating_func(std::function<void(void)> switch_heating_func) { set_heating_mode = switch_heating_func; };
        void set_status(std::function<const ecodan::Status& (void)> get_status_func) { get_status = get_status_func; };
        void set_dhw_climate_mode(bool mode) { this->dhw_climate_mode = mode; }
    private:
        std::function<void(float)> set_target_temp = nullptr;
        std::function<float(void)> get_current_temp = nullptr;
        std::function<float(void)> get_target_temp = nullptr;
        std::function<void(void)> set_cooling_mode = nullptr;
        std::function<void(void)> set_heating_mode = nullptr;
        std::function<const ecodan::Status& (void)> get_status = nullptr;
        bool dhw_climate_mode = false;

        void refresh();
        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };    

} // namespace ecodan
} // namespace esphome
