#include "ecodan.h"

namespace esphome {
namespace ecodan 
{ 
    static auto last_proxy_activity = std::chrono::steady_clock::now();
    bool proxy_timedout() {
        auto now = std::chrono::steady_clock::now();
        return now - last_proxy_activity > std::chrono::minutes(2);
    }

    void EcodanHeatpump::proxy_ping() {
        last_proxy_activity = std::chrono::steady_clock::now();
    } 

    bool EcodanHeatpump::proxy_available() {
        return proxy_uart_ && !proxy_timedout();
    }

} // namespace ecodan
} // namespace esphome
