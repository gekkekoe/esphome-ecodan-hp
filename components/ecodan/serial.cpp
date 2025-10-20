#include "ecodan.h"

namespace esphome {
namespace ecodan 
{
    bool EcodanHeatpump::serial_tx(Message& msg)
    {
        if (!uart_)
        {
            ESP_LOGE(TAG, "Serial connection unavailable for tx");
            return false;
        }

        msg.set_checksum();
        uart_->write_array(msg.buffer(), msg.size());
        
        return true;
    }
}
}
