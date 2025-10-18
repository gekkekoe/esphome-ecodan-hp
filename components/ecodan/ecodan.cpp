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
        
        if (proxy_available())
            handle_proxy();

        static Message rx_buffer_; 
        // consume all data and forward directly
        if (uart_ && uart_->available() > 0) {
            last_response = std::chrono::steady_clock::now();
            while (uart_->available() > 0) {
                uint8_t current_byte;
                uart_->read_byte(&current_byte);

                // Immediately forward the raw byte to the proxy
                if (proxy_available())
                    proxy_uart_->write_byte(current_byte);

                if (rx_buffer_.get_write_offset() == 0 && current_byte != HEADER_MAGIC_A1) 
                    continue;

                rx_buffer_.append_byte(current_byte);

                // Once the header is complete, verify it.
                if (rx_buffer_.get_write_offset() == rx_buffer_.header_size() && !rx_buffer_.verify_header()) {
                    ESP_LOGW(TAG, "Invalid packet header. Discarding message.");
                    rx_buffer_ = Message();
                    continue;
                }

                // Once the full packet is received, verify its checksum.
                if (rx_buffer_.get_write_offset() == rx_buffer_.size()) {
                    if (rx_buffer_.verify_checksum()) {
                        handle_response(rx_buffer_);
                    } else {
                        ESP_LOGW(TAG, "Invalid packet checksum. Discarding message.");
                    }
                    rx_buffer_ = Message();
                }
            }
        }
        
        auto now = std::chrono::steady_clock::now();
        if (now - last_response > std::chrono::minutes(2))
        {
            last_response = now;
            reset_connection();
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
