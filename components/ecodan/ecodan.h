#pragma once

#include <functional>
#include <string>
#include <chrono>

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
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
        void setup() override;
        void update() override;
        void loop() override;
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
        void set_hp_mode(uint8_t mode, esphome::ecodan::SetZone zone);
        void set_controller_mode(CONTROLLER_FLAG flag, bool on);
        void set_uart_parent(uart::UARTComponent *uart) { this->uart_ = uart; }
        void set_proxy_uart(uart::UARTComponent *uart) { this->proxy_uart_ = uart; }
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
        void handle_loop();
        bool is_connected();        
    
    private:
        uart::UARTComponent *uart_ = nullptr;
        uart::UARTComponent *proxy_uart_ = nullptr;
        Message res_buffer_;
        Message proxy_buffer_;
        int rx_sync_fail_count = 0;

        Status status;
        float temperatureStep = 0.5f;
        bool connected = false;
        bool heatpumpInitialized = false;
        
        std::queue<Message> cmdQueue;

        bool serial_rx(uart::UARTComponent *uart, Message& msg, bool count_sync_errors = false);
        bool serial_tx(uart::UARTComponent *uart, Message& msg);

        bool dispatch_next_status_cmd();
        bool dispatch_next_cmd();
        bool schedule_cmd(Message& cmd);
        
        void handle_response(Message& res);
        void handle_get_response(Message& res);
        void handle_set_response(Message& res);
        void handle_connect_response(Message& res);

        void proxy_ping();
        bool proxy_available();
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
        void set_status(std::function<const ecodan::Status& (void)> get_status_func) { get_status = get_status_func; };
        void set_dhw_climate_mode(bool mode) { this->dhw_climate_mode = mode; }
        void set_thermostat_climate_mode(bool mode) { this->thermostat_climate_mode = mode; }
    private:
        std::function<void(float)> set_target_temp = nullptr;
        std::function<float(void)> get_current_temp = nullptr;
        std::function<float(void)> get_target_temp = nullptr;
        std::function<const ecodan::Status& (void)> get_status = nullptr;
        bool dhw_climate_mode = false;
        bool thermostat_climate_mode = false;

        void refresh();
        void validate_target_temperature();
        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };    

} // namespace ecodan
} // namespace esphome
