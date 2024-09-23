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
        uint8_t data;
        bool skipping = false;
        bool sync_packet = false;

        while (uart->available() && uart->read_byte(&data)) {
            // Discard bytes until we see one that might reasonably be
            // the first byte of a packet, complaining only once.
            if (msg.get_write_offset() == 0) {
                if (data == HEADER_MAGIC_B) {
                    sync_packet = true;
                }
                else if (data == HEADER_MAGIC_A) {
                    sync_packet = false;
                }
                else {
                    if (!skipping) {
                        ESP_LOGE(TAG, "Dropping serial data; header magic mismatch");
                        skipping = true;
                    }
                    sync_packet = false;
                    continue;
                }
            }
            skipping = false;

            // Add the byte to the packet.
            msg.append_byte(data);

            if (!sync_packet) {
                // If the header is now complete, check it for sanity.
                if (msg.get_write_offset() == HEADER_SIZE && !msg.verify_header()) {
                    ESP_LOGI(TAG, "Serial port message appears invalid, skipping payload...");
                    msg = Message();
                    continue;
                }

                // If we don't yet have the full header, or if we do have the
                // header but not yet the full payload, keep going.
                if (msg.get_write_offset() <= HEADER_SIZE ||
                    msg.get_write_offset() < msg.size()) {
                    continue;
                }

                // Got full packet. Verify its checksum.
                if (!msg.verify_checksum()) {
                    ESP_LOGI(TAG, "Serial port message checksum invalid");
                    msg = Message();
                    continue;
                }
            }
            else {
                // 0x02 packages are always 7 bytes
                if (msg.get_write_offset() > 8))
                    return true;
                continue;
            }

            return true;
        }

        return false;
    }

}
}
