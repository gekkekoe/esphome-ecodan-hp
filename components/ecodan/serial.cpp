#include "ecodan.h"

#if ARDUINO_ARCH_ESP32
#include <esp_task_wdt.h>
#endif

namespace esphome {
namespace ecodan 
{
#pragma region Serial
    bool EcodanHeatpump::serial_tx(Message& msg)
    {
        if (!uart_)
        {
            ESP_LOGE(TAG, "Serial connection unavailable for tx");
            return false;
        }
#if 0
        if (port.availableForWrite() < msg.size())
        {
            ESP_LOGI(TAG, "Serial tx buffer size: %u", port.availableForWrite());
            return false;
        }
#endif
        msg.set_checksum();
        {
            std::lock_guard<std::mutex> lock{portWriteMutex};
            uart_->write_array(msg.buffer(), msg.size());
        }
        //port.flush(true);

        //ESP_LOGV(TAG, msg.debug_dump_packet().c_str());

        return true;
    }
    
    void EcodanHeatpump::resync_rx()
    {
        uint8_t byte;

        while (uart_->available() > 0)
            uart_->read_byte(&byte);
    }

    bool EcodanHeatpump::serial_rx(Message& msg)
    {
        if (uart_->available() < HEADER_SIZE)
        {
            const TickType_t maxBlockingTime = pdMS_TO_TICKS(1000);
            ulTaskNotifyTakeIndexed(0, pdTRUE, maxBlockingTime);

            // We were woken by an interrupt, but there's not enough data available
            // yet on the serial port for us to start processing it as a packet.
            if (uart_->available() < HEADER_SIZE)
                return false;
        }

        // Scan for the start of an Ecodan packet.
        if (!uart_->read_array(msg.buffer(), HEADER_SIZE))
        {
            ESP_LOGI(TAG, "Serial port header read failure!");
            resync_rx();
            return false;
        }
        if (msg.buffer()[0] != HEADER_MAGIC_A)
        {
            ESP_LOGE(TAG, "Dropping serial data, header magic mismatch");
            resync_rx();
            return false;
        }

        msg.increment_write_offset(HEADER_SIZE);

        if (!msg.verify_header())
        {
            ESP_LOGI(TAG, "Serial port message appears invalid, skipping payload wait...");
            resync_rx();
            return false;
        }

        // It shouldn't take long to receive the rest of the payload after we get the header.
        size_t remainingBytes = msg.payload_size() + CHECKSUM_SIZE;
        auto startTime = std::chrono::steady_clock::now();
        while (uart_->available() < remainingBytes)
        {
            delay(1);
            if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(10))
            {
                ESP_LOGI(TAG, "Serial port message could not be received within 10s (got %u / %u bytes)", uart_->available(), remainingBytes);
                resync_rx();
                return false;
            }
        }

        if (!uart_->read_array(msg.payload(), remainingBytes))
        {
            ESP_LOGI(TAG, "Serial port payload read failure!");
            resync_rx();
            return false;
        }

        msg.increment_write_offset(msg.payload_size()); // Don't count checksum byte.

        if (!msg.verify_checksum())
        {
            resync_rx();
            return false;
        }

        //ESP_LOGW(TAG, msg.debug_dump_packet().c_str());

        return true;
    }

#pragma endregion Serial

}
}
