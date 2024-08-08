#include "ecodan.h"

namespace esphome {
namespace ecodan 
{
    void EcodanHeatpump::handle_proxy() {
        if (!serial_rx(proxy_uart_, proxy_buffer_))
            return;

        ESP_LOGI(TAG, "Proxy RX: %s", proxy_buffer_.debug_dump_packet().c_str());
        proxy_buffer_ = Message();
    }

}
}
