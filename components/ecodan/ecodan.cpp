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
                ESP_LOGI(TAG, "Proxy UART not configured for 2400/9600 8E1. This may not work...");
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

        if (proxy_uart_ && proxy_uart_->available() > 0) {
            proxy_ping();
            if (serial_rx(proxy_uart_, proxy_buffer_, true))
            {
                // forward cmds from slave to master
                if (uart_)
                    uart_->write_array(proxy_buffer_.buffer(), proxy_buffer_.size());
                proxy_buffer_ = Message();    
            }

            // if we could not get the sync byte after 4*packet size attemp, we are probably using the wrong baud rate
            if (rx_sync_fail_count > 4*16) {
                //swap 9600 <-> 2400
                int current_baud = proxy_uart_->get_baud_rate(); 
                int new_baud =  current_baud == 2400 ? 9600 : 2400;
                ESP_LOGE(TAG, "Could not get sync byte, swapping baud from '%d' to '%d' for slave...", current_baud, new_baud);
                proxy_uart_->flush();
                proxy_uart_->set_baud_rate(new_baud);
                proxy_uart_->load_settings();
                
                // reset fail count
                rx_sync_fail_count = 0;
            }
        }

        if (serial_rx(uart_, res_buffer_))
        {
            last_response = std::chrono::steady_clock::now();
            if (proxy_available())
                proxy_uart_->write_array(res_buffer_.buffer(), res_buffer_.size());
            
            // interpret message
            handle_response(res_buffer_);
            res_buffer_ = Message();
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
