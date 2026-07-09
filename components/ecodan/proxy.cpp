#include "ecodan.h"

namespace esphome {
namespace ecodan 
{ 
    void EcodanHeatpump::proxy_ping() {
        this->last_proxy_activity_.store(std::chrono::steady_clock::now());
    } 

    bool EcodanHeatpump::proxy_available() {
        auto now = std::chrono::steady_clock::now();
        auto timeout = now - this->last_proxy_activity_.load() > std::chrono::seconds(60);
        return this->proxy_uart_ && this->slaveDetected && !timeout;
    } 
    
} // namespace ecodan
} // namespace esphome
