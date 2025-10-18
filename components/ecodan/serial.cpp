#include "ecodan.h"

namespace esphome {
namespace ecodan 
{
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
