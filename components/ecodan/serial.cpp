#include "ecodan.h"

namespace esphome {
namespace ecodan 
{
    // proxy serial communicatioons
    const uint8_t connect_request[] = { 0xfc, 0x5a, 0x02, 0x7a, 0x02, 0xca, 0x01, 0x5d};
    const uint8_t connect_response[] = { 0xfc, 0x7a, 0x02, 0x7a, 0x01, 0x00, 0x09 };
    const uint8_t first_request[] = { 0x02, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x02 };
    const uint8_t expected_first_response[] = { 0x02, 0xff, 0xff, 0x80, 0x00, 0x00, 0x0a, 0x01, 0x00, 0x40, 0x00, 0x00, 0x06, 0x02, 0x7a, 0x00, 0x00, 0xb5 };
    const uint8_t second_request[] = { 0x02, 0xff, 0xff, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00 };
    const uint8_t expected_second_response[] = { 0x02, 0xff, 0xff, 0x81, 0x00, 0x00, 0x00, 0x81 };
    const uint8_t keep_alive_request[] = { 0xfc, 0x41, 0x02, 0x7a, 0x10, 0x34, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xfd };
    const uint8_t keep_alive_response[] = { 0xfc, 0x61, 0x02, 0x7a, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13 };

    const uint8_t ack_response[] = { 0xfc, 0x61, 0x02, 0x7a, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13 };

    void EcodanHeatpump::process_serial_byte(uint8_t byte, Message& buffer, bool is_proxy_message) {

        if (buffer.get_write_offset() == 0) {
            bool skip_byte = is_proxy_message ? (byte != HEADER_MAGIC_A1 && byte != HEADER_MAGIC_A2) : (byte != HEADER_MAGIC_A1);
            if (skip_byte) {
                return;
            }
            else if (is_proxy_message && !this->slaveDetected) {
                this->slaveDetected = true;
            }
        }

        buffer.append_byte(byte);

        if (buffer.get_write_offset() == buffer.header_size() && !buffer.verify_header()) {
            ESP_LOGW(TAG, "Invalid packet header. Discarding message.");
            buffer.reset();
            return;
        }

        if (buffer.get_write_offset() == buffer.size()) {
            if (buffer.verify_checksum()) {
                if (is_proxy_message) { // proxy comm

                    if (this->ignoreSlaveCMDs) {
                        switch (buffer.payload_type<SetType>())
                        {
                        case SetType::BASIC_SETTINGS:
                        case SetType::ROOM_SETTINGS:
                            // send ack and drop cmd
                            proxy_uart_->write_array(ack_response, sizeof(ack_response));
                            buffer.reset();
                            //ESP_LOGD(TAG, "Dropping slave cmd");
                            return;
                        break;
                        }
                    }
                    
                    // proxy full packet only
                    if (xSemaphoreTake(this->uart_tx_mutex_, (TickType_t)10) == pdTRUE) {
                        this->uart_->write_array(buffer.buffer(), buffer.get_write_offset());
                        xSemaphoreGive(this->uart_tx_mutex_);
                    } else {
                        ESP_LOGE(TAG, "Could not acquire uart_tx_mutex");
                    }

                    // handle handshake cached
                    if (buffer.matches(first_request, sizeof(first_request))) {
                        proxy_uart_->write_array(expected_first_response, sizeof(expected_first_response));
                        //ESP_LOGD(TAG, "Handshake: First request detected, sending response.");
                    }
                    else if (buffer.matches(second_request, sizeof(second_request))) {
                        proxy_uart_->write_array(expected_second_response, sizeof(expected_second_response));
                        //ESP_LOGD(TAG, "Handshake: Second request detected, sending response. Handshake complete!");
                    }
                    else if (buffer.matches(connect_request, sizeof(connect_request))) {
                        proxy_uart_->write_array(connect_response, sizeof(connect_response));
                        this->connected = true;
                        //ESP_LOGD(TAG, "Incoming connect request from proxy interface");
                    }
                    else if (buffer.matches(keep_alive_request, sizeof(keep_alive_request))) {
                        proxy_uart_->write_array(keep_alive_response, sizeof(keep_alive_response));
                        //ESP_LOGD(TAG, "keep alive request detected");
                    } 

                } else { // FTC comm
                    if (this->proxy_available())
                        this->proxy_uart_->write_array(buffer.buffer(), buffer.get_write_offset());
                    // enqueue full packet 
                    if (uxQueueSpacesAvailable(this->rx_message_queue_) == 0) {
                        Message discarded_message;
                        xQueueReceive(this->rx_message_queue_, &discarded_message, (TickType_t)0);
                        ESP_LOGW(TAG, "Message queue was full. Discarded oldest message.");
                    }
                    xQueueSend(this->rx_message_queue_, &buffer, (TickType_t)0);
                }
            } else {
                ESP_LOGW(TAG, "Invalid packet checksum. Discarding message.");
            }
            
            buffer.reset();
        }
    }

    void EcodanHeatpump::serial_io_task() {
        static Message rx_buffer_;
        static Message proxy_rx_buffer_;

        while (true) {
            // handle proxy port (slave -> ftc)
            if (this->proxy_uart_ && this->uart_) {
                if (this->proxy_uart_->available() > 0) {
                    proxy_ping();
                    uint8_t current_byte;

                    while (this->proxy_uart_->available() > 0) {        
                        this->proxy_uart_->read_byte(&current_byte);
                        process_serial_byte(current_byte, proxy_rx_buffer_, true);
                    }
                }
            }

            // handle ftc port (ftc -> slave)
            if (this->uart_ && this->uart_->available() > 0) {
                uint8_t current_byte;
                while (this->uart_->available() > 0) {
                    this->uart_->read_byte(&current_byte);

                    // timing is off when we buffer full package on the proxy uart.
                    // if (this->proxy_available())
                    //     this->proxy_uart_->write_byte(current_byte);
                    process_serial_byte(current_byte, rx_buffer_, false);
                }
            }
            // yield
            vTaskDelay(1); 
        }
    }    

    bool EcodanHeatpump::serial_tx(Message& msg)
    {
        if (!uart_)
        {
            ESP_LOGE(TAG, "Serial connection unavailable for tx");
            return false;
        }

        msg.set_checksum();
        if (xSemaphoreTake(this->uart_tx_mutex_, (TickType_t)100) == pdTRUE) {    
            uart_->write_array(msg.buffer(), msg.size());
            xSemaphoreGive(this->uart_tx_mutex_);
        } else {
            ESP_LOGE(TAG, "failed to acquire uart_tx_mutex");
        }
        return true;
    }
}
}
