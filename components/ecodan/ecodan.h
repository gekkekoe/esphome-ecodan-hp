#pragma once

#include <functional>
#include <string>
#include <chrono>
#include <optional>

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
            if (obj != nullptr)
                sensors[key] = obj;
        }

        void register_textSensor(text_sensor::TextSensor *obj, const std::string& key) {
            if (obj != nullptr) 
                textSensors[key] = obj;
        }

        void register_binarySensor(binary_sensor::BinarySensor *obj, const std::string& key) {
            if (obj != nullptr) {
                obj->publish_state(false);
                binarySensors[key] = obj;
            }
        }

        void enable_request_code_sensors() {
            hasRequestCodeSensors = true;
        }

        void enable_request_codes(bool enable) {
            requestCodesEnabled = enable;
        }

        // exposed as external component commands
        void set_room_temperature(float value, esphome::ecodan::Zone zone);
        void set_flow_target_temperature(float value, esphome::ecodan::Zone zone);
        void set_dhw_target_temperature(float value);
        void set_dhw_mode(Status::DhwMode dhwMode);
        void set_dhw_force(bool on);
        void set_holiday(bool on);
        void set_power_mode(bool on);
        void set_hp_mode(uint8_t mode, esphome::ecodan::Zone zone);
        void set_controller_mode(CONTROLLER_FLAG flag, bool on);
        void set_mrc_mode(Status::MRC_FLAG flag);
        void set_svc_state_before_lockout(CONTROLLER_FLAG flag) { serverControlFlagBeforeLockout = flag; }
        void reset_svc_state_before_lockout() { serverControlFlagBeforeLockout.reset(); }
        void set_specific_heat_constant(float newConstant) { specificHeatConstantOverride = newConstant; }
        void set_polling_interval(uint32_t ms) { this->set_update_interval(ms); }
        void set_uart_parent(uart::UARTComponent *uart) { this->uart_ = uart; }
        void set_proxy_uart(uart::UARTComponent *uart) { this->proxy_uart_ = uart; }
        const Status& get_status() const { return status; }
        std::optional<CONTROLLER_FLAG> get_svc_state_before_lockout() { return serverControlFlagBeforeLockout; }

    protected:
        std::map<std::string, sensor::Sensor*> sensors;
        std::map<std::string, text_sensor::TextSensor*> textSensors;
        std::map<std::string, binary_sensor::BinarySensor*> binarySensors;

        // publish func
        void publish_state(const std::string& sensorKey, float sensorValue);
        void publish_state(const std::string& sensorKey, const std::string& sensorValue);
        void publish_state(const std::string& sensorKey, bool sensorValue);

        bool begin_connect();

        bool initialize();
        void handle_loop();
        bool is_connected();        
    
    private:
        uart::UARTComponent *uart_ = nullptr;
        uart::UARTComponent *proxy_uart_ = nullptr;
        uint8_t initialCount = 0;

        Status status;
        float temperatureStep = 0.5f;
        float specificHeatConstantOverride {NAN};
        bool connected = false;
        bool heatpumpInitialized = false;
        
        bool hasRequestCodeSensors = false;
        bool requestCodesEnabled = true;
        Status::REQUEST_CODE activeRequestCode = Status::REQUEST_CODE::NONE;

        std::optional<CONTROLLER_FLAG> serverControlFlagBeforeLockout = {};
        std::queue<Message> cmdQueue;

        bool serial_rx(uart::UARTComponent *uart, Message& msg);
        bool serial_tx(uart::UARTComponent *uart, Message& msg);
        void handle_proxy();
        
        bool disconnect();
        void reset_connection() {
            if (proxy_available())
                return;

            connected = false;
            initialCount = 0;
            disconnect();
    };
        bool initialCmdCompleted() { return initialCount == 3; };

        bool dispatch_next_status_cmd();
        bool dispatch_next_cmd();
        bool schedule_cmd(Message& cmd);
        bool handle_active_request_codes();
        
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
        void set_zone_identifier(uint8_t zone_identifier) { this->climate_zone_identifier = static_cast<ClimateZoneIdentifier>(zone_identifier); }
    private:
        std::function<void(float)> set_target_temp = nullptr;
        std::function<float(void)> get_current_temp = nullptr;
        std::function<float(void)> get_target_temp = nullptr;
        std::function<const ecodan::Status& (void)> get_status = nullptr;
        bool dhw_climate_mode = false;
        bool thermostat_climate_mode = false;
        ClimateZoneIdentifier climate_zone_identifier = ClimateZoneIdentifier::SINGLE_ZONE;

        void refresh();
        void validate_target_temperature();
        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };    

} // namespace ecodan
} // namespace esphome
