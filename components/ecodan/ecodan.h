#pragma once

#include <functional>
#include <string>
#include <chrono>
#include <optional>
#include <atomic>

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/thermostat/thermostat_climate.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" 
#include "freertos/semphr.h"

#include "proto.h"
#include "status.h"

namespace esphome {
namespace ecodan 
{    
    static constexpr const char *TAG = "ecodan.component";

    struct QueuedCommand {
        Message message;
        int retries = 0;
        unsigned long last_sent_time = 0; 
    };

    class EcodanHeatpump : public PollingComponent {
    public:        
        EcodanHeatpump();
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
        void set_ignore_slave_cmd(bool ignoreCmds) { ignoreSlaveCMDs = ignoreCmds; };
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
        bool slaveDetected = false;
        bool ignoreSlaveCMDs = false;

        Status status;
        float temperatureStep = 0.5f;
        float specificHeatConstantOverride {NAN};
        bool connected = false;
        bool heatpumpInitialized = false;
        
        bool hasRequestCodeSensors = false;
        bool requestCodesEnabled = true;
        Status::REQUEST_CODE activeRequestCode = Status::REQUEST_CODE::NONE;

        std::optional<CONTROLLER_FLAG> serverControlFlagBeforeLockout = {};
        std::queue<QueuedCommand> cmdQueue;

        std::atomic<std::chrono::steady_clock::time_point> last_proxy_activity_;
        TaskHandle_t serial_io_task_handle_ = nullptr;
        QueueHandle_t rx_message_queue_ = nullptr;
        SemaphoreHandle_t uart_tx_mutex_ = nullptr;

        bool serial_tx(Message& msg);
        
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
        void serial_io_task();
        void process_serial_byte(uint8_t byte, Message& buffer, bool is_proxy_message);
        
        static void serial_io_task_trampoline(void *arg) {
            static_cast<EcodanHeatpump*>(arg)->serial_io_task();
        };
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
        void get_current_limits(float &min_limit, float &max_limit);
        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };    

    class EcodanVirtualThermostat : public thermostat::ThermostatClimate {
    public:
        void control(const climate::ClimateCall &call) override {

            if (call.get_target_temperature().has_value()) {
                this->target_temperature = *call.get_target_temperature();
            }
            if (!std::isnan(this->target_temperature)) {
                this->target_temperature_low = this->target_temperature;
                this->target_temperature_high = this->target_temperature;
            }
            thermostat::ThermostatClimate::control(call);  
        };

        climate::ClimateTraits traits() override {
            auto traits = thermostat::ThermostatClimate::traits();
            traits.clear_feature_flags(climate::CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE);
            traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
            traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);

            traits.set_supported_modes({
                climate::CLIMATE_MODE_OFF,
                climate::CLIMATE_MODE_HEAT,
                climate::CLIMATE_MODE_COOL
            });

            traits.set_visual_min_temperature(8);
            traits.set_visual_max_temperature(28);
            traits.set_visual_target_temperature_step(0.1);
            traits.set_visual_current_temperature_step(0.1);

            return traits;
        }
    };

} // namespace ecodan
} // namespace esphome
