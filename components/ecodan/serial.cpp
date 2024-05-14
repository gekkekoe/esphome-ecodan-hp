#include "ecodan.h"

#include "esphome.h"

#include <HardwareSerial.h>
#include <freertos/task.h>
#include <functional>

#include <mutex>
#include <queue>
#include <thread>

#if ARDUINO_ARCH_ESP32
#include <esp_task_wdt.h>
#endif

namespace esphome {
namespace ecodan 
{
#pragma region ESP32Hardware
    TaskHandle_t serialRxTaskHandle = nullptr;

    void IRAM_ATTR serial_rx_isr()
    {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveIndexedFromISR(serialRxTaskHandle, 0, &higherPriorityTaskWoken);
#if CONFIG_IDF_TARGET_ESP32C3
        portEND_SWITCHING_ISR(higherPriorityTaskWoken);
#else
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
#endif
    }

    void init_watchdog()
    {
#if ARDUINO_ARCH_ESP32
        esp_task_wdt_init(15, true); // Reset the board if the watchdog timer isn't reset every 15s.        
#endif
    }

    void add_thread_to_watchdog()
    {
#if ARDUINO_ARCH_ESP32
        esp_task_wdt_add(nullptr);
#endif
    }

    void ping_watchdog()
    {
#if ARDUINO_ARCH_ESP32
        esp_task_wdt_reset();
#endif
    }
#pragma endregion ESP32Hardware    


#pragma region Serial
    bool EcodanHeatpump::serial_tx(Message& msg)
    {
        if (!port)
        {
            ESP_LOGE(TAG, "Serial connection unavailable for tx");
            return false;
        }

        if (port.availableForWrite() < msg.size())
        {
            ESP_LOGI(TAG, "Serial tx buffer size: %u", port.availableForWrite());
            return false;
        }

        msg.set_checksum();
        {
            std::lock_guard<std::mutex> lock{portWriteMutex};
            port.write(msg.buffer(), msg.size());
        }
        //port.flush(true);

        //ESP_LOGV(TAG, msg.debug_dump_packet().c_str());

        return true;
    }
    
    void EcodanHeatpump::resync_rx()
    {
        while (port.available() > 0)
            port.read();
    }

    bool EcodanHeatpump::serial_rx(Message& msg)
    {
        if (!port)
        {
            ESP_LOGE(TAG, "Serial connection unavailable for rx");
            return false;
        }

        if (port.available() < HEADER_SIZE)
        {
            const TickType_t maxBlockingTime = pdMS_TO_TICKS(1000);
            ulTaskNotifyTakeIndexed(0, pdTRUE, maxBlockingTime);

            // We were woken by an interrupt, but there's not enough data available
            // yet on the serial port for us to start processing it as a packet.
            if (port.available() < HEADER_SIZE)
                return false;
        }

        // Scan for the start of an Ecodan packet.
        if (port.peek() != HEADER_MAGIC_A)
        {
            ESP_LOGE(TAG, "Dropping serial data, header magic mismatch");
            resync_rx();
            return false;
        }

        if (port.readBytes(msg.buffer(), HEADER_SIZE) < HEADER_SIZE)
        {
            ESP_LOGI(TAG, "Serial port header read failure!");
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
        while (port.available() < remainingBytes)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            if (std::chrono::steady_clock::now() - startTime > std::chrono::seconds(10))
            {
                ESP_LOGI(TAG, "Serial port message could not be received within 10s (got %u / %u bytes)", port.available(), remainingBytes);
                resync_rx();
                return false;
            }
        }

        if (port.readBytes(msg.payload(), remainingBytes) < remainingBytes)
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

    void EcodanHeatpump::serial_rx_thread()
    {        
        add_thread_to_watchdog();
        // Wake the serial RX thread when the serial RX GPIO pin changes (this may occur during or after packet receipt)
        serialRxTaskHandle = xTaskGetCurrentTaskHandle();

        {
            attachInterrupt(digitalPinToInterrupt(serialRxPort), serial_rx_isr, FALLING);
        }        
        
        while (true)
        {   
            //ESP_LOGI(TAG, "handle response on core %d", xPortGetCoreID());
            ping_watchdog();            
            handle_response();            
            //vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void EcodanHeatpump::init_hw_watchdog() 
    {
        init_watchdog();
    }

#pragma endregion Serial

}
}