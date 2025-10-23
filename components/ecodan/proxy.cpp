#include "ecodan.h"

namespace esphome {
namespace ecodan 
{ 
    void EcodanHeatpump::proxy_ping() {
        this->last_proxy_activity_.store(std::chrono::steady_clock::now());
    } 

    bool EcodanHeatpump::proxy_available() {
        auto now = std::chrono::steady_clock::now();
        auto timeout = now - this->last_proxy_activity_.load() > std::chrono::minutes(2);
        return proxy_uart_ && !timeout;
    } 
/*
    // proxy serial communicatioons
    const uint8_t connect_request[] = { 0xfc, 0x5a, 0x02, 0x7a, 0x02, 0xca, 0x01, 0x5d};
    const uint8_t connect_response[] = { 0xfc, 0x7a, 0x02, 0x7a, 0x01, 0x00, 0x09 };
    const uint8_t first_request[] = { 0x02, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x02 };
    const uint8_t expected_first_response[] = { 0x02, 0xff, 0xff, 0x80, 0x00, 0x00, 0x0a, 0x01, 0x00, 0x40, 0x00, 0x00, 0x06, 0x02, 0x7a, 0x00, 0x00, 0xb5 };
    const uint8_t second_request[] = { 0x02, 0xff, 0xff, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00 };
    const uint8_t expected_second_response[] = { 0x02, 0xff, 0xff, 0x81, 0x00, 0x00, 0x00, 0x81 };
    const uint8_t keep_alive_request[] = { 0xfc, 0x41, 0x02, 0x7a, 0x10, 0x34, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xfd };
    const uint8_t keep_alive_response[] = { 0xfc, 0x61, 0x02, 0x7a, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13 };

    void EcodanHeatpump::handle_proxy() {

        static Message msg;
        uint8_t data;
        if (proxy_uart_ && proxy_uart_->available() > 0) {
            proxy_ping();

            while (proxy_uart_->available() > 0 && proxy_uart_->read_byte(&data)) {
                if (uart_)
                    uart_->write_byte(data);

                if (msg.get_write_offset() == 0 && data != HEADER_MAGIC_A1 && data != HEADER_MAGIC_A2)
                    continue;

                msg.append_byte(data);
                // Once the header is complete, verify it.
                if (msg.get_write_offset() == msg.header_size() && !msg.verify_header()) {
                    ESP_LOGW(TAG, "Invalid packet header. Discarding message.");
                    msg.reset();
                    continue;
                }

                // Once the full packet is received, verify its checksum.
                if (msg.get_write_offset() >= msg.size()) {
                    if (msg.verify_checksum()) {
                        // if (uart_)
                        //     uart_->write_array(msg.buffer(), msg.size());
                        if (msg.matches(first_request, sizeof(first_request))) {
                            ESP_LOGD(TAG, "Handshake: First request detected, sending response.");
                            proxy_uart_->write_array(expected_first_response, sizeof(expected_first_response));
                        }
                        else if (msg.matches(second_request, sizeof(second_request))) {
                            ESP_LOGD(TAG, "Handshake: Second request detected, sending response. Handshake complete!");
                            proxy_uart_->write_array(expected_second_response, sizeof(expected_second_response));
                        }
                        else if (msg.matches(connect_request, sizeof(connect_request))) {
                            ESP_LOGD(TAG, "Incoming connect request from proxy interface");
                            proxy_uart_->write_array(connect_response, sizeof(connect_response));
                        }
                        else if (msg.matches(keep_alive_request, sizeof(keep_alive_request))) {
                            ESP_LOGD(TAG, "keep alive request detected");
                            proxy_uart_->write_array(keep_alive_response, sizeof(keep_alive_response));
                        }
                    } else {
                        ESP_LOGW(TAG, "Invalid packet checksum. Discarding message.");
                    }
                    msg.reset();
                }
            }
        }
    }
    */
} // namespace ecodan
} // namespace esphome
