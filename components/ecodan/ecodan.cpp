#include "ecodan.h"

namespace esphome {
namespace ecodan 
{ 

    EcodanHeatpump::EcodanHeatpump() : PollingComponent() {
        // ring buffer for messages from the uart port
        this->rx_message_queue_ = xQueueCreate(10, sizeof(Message));
        if (this->rx_message_queue_ == nullptr) {
            ESP_LOGE(TAG, "Could not create rx_message_queue");
        }

        // write mutex uart tx
        this->uart_tx_mutex_ = xSemaphoreCreateMutex();
        if (this->uart_tx_mutex_ == nullptr) {
            ESP_LOGE(TAG, "Could not create uart_tx_mutex");
        }
    }

    void EcodanHeatpump::setup() {
        heatpumpInitialized = initialize();
        this->last_proxy_activity_ = std::chrono::steady_clock::now();

        BaseType_t task_core_id;
#if CONFIG_FREERTOS_UNICORE
        task_core_id = 0;
        ESP_LOGI(TAG, "Setup: Single Core detected. Serial task pinned to Core 0.");
#else
        int main_core_id = xPortGetCoreID();
        task_core_id = 1 - main_core_id;
        ESP_LOGI(TAG, "Setup: Dual Core detected. Main running on Core %d. Serial task pinned to Core %d.", main_core_id, task_core_id);
#endif
        // background serial io handler
        xTaskCreatePinnedToCore(
            serial_io_task_trampoline,
            "serial_io_task", 4096, this,
            configMAX_PRIORITIES - 1, &this->serial_io_task_handle_,
            task_core_id // pin to other core than esphome if available
        );
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

        static Message received_message;
        // Get messages from queue
        while (xQueueReceive(this->rx_message_queue_, &received_message, (TickType_t)0) == pdTRUE) {
            last_response = std::chrono::steady_clock::now();
            handle_response(received_message);
            received_message.reset();
        }
        
        auto now = std::chrono::steady_clock::now();
        if (now - last_response > std::chrono::seconds(90))
        {
            last_response = now;
            reset_connection();
            ESP_LOGW(TAG, "No reply received from the heatpump in the last 2 minutes, going to reconnect...");
        }
    }

    void EcodanHeatpump::handle_loop()
    {        
        if (!is_connected() && uart_)
        {
            if (proxy_available()) {
                // re-use previous connect
                dispatch_next_cmd();
            }
            else {
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
