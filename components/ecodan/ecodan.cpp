#include "ecodan.h"

namespace esphome {
namespace ecodan 
{ 
    void EcodanHeatpump::setup() {
        heatpumpInitialized = initialize();
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
        if (heatpumpInitialized)
            handle_loop();            
    }

    void EcodanHeatpump::dump_config() {
        ESP_LOGI(TAG, "config"); 
    }

    bool EcodanHeatpump::initialize()
    {
        if (!uart_) {
            ESP_LOGE(TAG, "No UART configured");
            return false;
        }

        ESP_LOGI(TAG, "Initializing HeatPump using UART %p, proxy %p", uart_, proxy_uart_);
 
        if (uart_->get_baud_rate() != 2400 ||
            uart_->get_stop_bits() != 1 ||
            uart_->get_data_bits() != 8 ||
            uart_->get_parity() != uart::UART_CONFIG_PARITY_EVEN) {
            ESP_LOGI(TAG, "UART not configured for 2400 8E1. This may not work...");
        }

        if (proxy_uart_) {    
            if ((proxy_uart_->get_baud_rate() != 2400 && proxy_uart_->get_baud_rate() != 9600) ||
                proxy_uart_->get_stop_bits() != 1 ||
                proxy_uart_->get_data_bits() != 8 ||
                proxy_uart_->get_parity() != uart::UART_CONFIG_PARITY_EVEN) {
                ESP_LOGI(TAG, "Proxy UART not configured for 2400 8E1. This may not work...");
            }            
        }
        else if (!is_connected()){
            begin_connect();
        }

        return true;
    }

    void EcodanHeatpump::loop()
    {
        static auto last_response = std::chrono::steady_clock::now();

        if (uart_ && serial_rx(uart_, res_buffer_))
        {
            last_response = std::chrono::steady_clock::now();
            if (proxy_uart_)
                proxy_uart_->write_array(res_buffer_.buffer(), res_buffer_.size());
            
            // interpret message
            handle_response(res_buffer_);
            res_buffer_ = Message();
        }

        if (proxy_uart_ && serial_rx(proxy_uart_, proxy_buffer_))
        {
            // forward cmds from slave to master
            //schedule_cmd(proxy_buffer_);
            uart_->write_array(proxy_buffer_.buffer(), proxy_buffer_.size());
            proxy_buffer_ = Message();    
        }

        auto now = std::chrono::steady_clock::now();
        if (now - last_response > std::chrono::minutes(2))
        {
            last_response = now;
            connected = false;
            ESP_LOGW(TAG, "No reply received from the heatpump in the last 2 minutes, going to reconnect...");
        }
    
    }

    void EcodanHeatpump::handle_loop()
    {        
        if (!is_connected() && uart_ && !uart_->available())
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
            dispatch_next_cmd();

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


} // namespace ecodan
} // namespace esphome
