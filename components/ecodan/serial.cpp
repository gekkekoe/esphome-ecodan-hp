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

    bool EcodanHeatpump::serial_rx(uart::UARTComponent *uart, Message& msg, bool count_sync_errors)
    {
        uint8_t data;
        bool skipping = false;

        while (uart && uart->available() && uart->read_byte(&data)) {
            // Discard bytes until we see one that might reasonably be
            // the first byte of a packet, complaining only once.
            if (msg.get_write_offset() == 0 && data != HEADER_MAGIC_A && data != HEADER_MAGIC_B) {
                if (count_sync_errors)
                    rx_sync_fail_count++;
                if (!skipping) {
                    ESP_LOGE(TAG, "Dropping serial data '%02X', header magic mismatch", data);
                    skipping = true;
                }
                continue;
            }
            skipping = false;
            if (count_sync_errors)
                rx_sync_fail_count = 0;

            // Add the byte to the packet.
            msg.append_byte(data);

            // If the header is now complete, check it for sanity.
            if (msg.get_write_offset() == msg.header_size() && !msg.verify_header()) {
                ESP_LOGI(TAG, "Serial port message appears invalid, skipping payload...");
                msg = Message();
                continue;
            }

            // If we don't yet have the full header, or if we do have the
            // header but not yet the full payload, keep going.
            if (msg.get_write_offset() <= msg.header_size() ||
                msg.get_write_offset() < msg.size()) {
                continue;
            }

            // Got full packet. Verify its checksum.
            if (!msg.verify_checksum()) {
                ESP_LOGI(TAG, "Serial port message checksum invalid");
                msg = Message();
                continue;
            }

            return true;
        }

        return false;
    }

}
}
