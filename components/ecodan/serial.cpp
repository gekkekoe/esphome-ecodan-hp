#include "ecodan.h"

namespace esphome {
namespace ecodan 
{
    const uint8_t first_request[8] = { 0x02, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x02 };
    const uint8_t expected_first_response[18] = { 0x02, 0xFF, 0xFF, 0x80, 0x00, 0x00, 0x0A, 0x01, 0x00, 0x40, 0x00, 0x00, 0x06, 0x02, 0x7A, 0x00, 0x00, 0xB5 };
    const uint8_t second_request[9] = { 0x02, 0xff, 0xff, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00 };
    const uint8_t expected_second_response[8] = { 0x02, 0xff, 0xff, 0x81, 0x00, 0x00, 0x00, 0x81 };

    void EcodanHeatpump::handle_proxy_handshake(uart::UARTComponent *uart) {
        static uint8_t buffer[32];
        static size_t buffer_len = 0;

        if (!uart)
            return;

        while (uart->available() > 0) {
            uint8_t current_byte;
            uart->read_byte(&current_byte);

            if (buffer_len == 0 && current_byte != HEADER_MAGIC_A2) 
                continue; // scan for magic
            
            if (buffer_len < sizeof(buffer)) {
                buffer[buffer_len] = current_byte;
                buffer_len++;
            } else {
                // Buffer overflow, just in case
                buffer_len = 0;
                continue;
            }

            if (buffer_len == sizeof(first_request) && memcmp(buffer, first_request, sizeof(first_request)) == 0) {
                ESP_LOGD(TAG, "Handshake: First request detected, sending response.");
                uart->write_array(expected_first_response, sizeof(expected_first_response));
                buffer_len = 0; 
                continue;
            }

            if (buffer_len == sizeof(second_request) && memcmp(buffer, second_request, sizeof(second_request)) == 0) {
                ESP_LOGI(TAG, "Handshake: Second request detected, sending response. Handshake complete!");
                uart->write_array(expected_second_response, sizeof(expected_second_response));
                buffer_len = 0;
                this->handshake_state_ = ProxyHandshakeState::COMPLETED; 
                return;
            }
        }
    }

    bool EcodanHeatpump::serial_tx(uart::UARTComponent *uart, Message& msg)
    {
        if (!uart)
        {
            ESP_LOGE(TAG, "Serial connection unavailable for tx");
            return false;
        }

        msg.set_checksum();
        uart->write_array(msg.buffer(), msg.size());

        //ESP_LOGV(TAG, msg.debug_dump_packet().c_str());

        return true;
    }

    bool EcodanHeatpump::serial_rx(uart::UARTComponent *uart, Message& msg)
    {
        if (!uart || !uart->available())
            return false;

        if (msg.get_write_offset() == 0) {
            uint8_t first_byte;
            uart->peek_byte(&first_byte);

            if (first_byte != HEADER_MAGIC_A1 && first_byte != HEADER_MAGIC_A2) {
                ESP_LOGW(TAG, "Unsynchronized stream starting with: '%02X'.", first_byte);
                
                uint8_t data;
                while (uart->available()) {
                    uart->peek_byte(&first_byte);
                    if (first_byte == HEADER_MAGIC_A1 || first_byte == HEADER_MAGIC_A2) {
                        break; // Stop before the start of the next valid packet.
                    }
                    uart->read_byte(&data);
                    msg.append_byte(data);
                }
                // Return false to let loop() know it has an unknown packet to forward.
                return false;
            }
        }

        uint8_t data;
        while (uart->read_byte(&data)) {
            msg.append_byte(data);
            // Once the header is complete, verify it.
            if (msg.get_write_offset() == msg.header_size() && !msg.verify_header()) {
                ESP_LOGW(TAG, "Invalid packet header. Discarding message.");
                msg = Message();
                return false;
            }

            // Once the full packet is received, verify its checksum.
            if (msg.get_write_offset() == msg.size()) {
                if (msg.verify_checksum()) {
                    return true;
                } else {
                    ESP_LOGW(TAG, "Invalid packet checksum. Discarding message.");
                    msg = Message();
                    return false;
                }
            }
        }

        // Not enough data has arrived to complete the packet. Return false,
        // but leave the partial data in the buffer for the next call.
        return false;     
    }

}
}
