#include "ecodan.h"
#include "esphome.h"

namespace esphome {
namespace ecodan 
{
    void EcodanHeatpump::clear_obsoleted_cycle_detection_entries() {
        // clean obsoleted entries before inserting new items
        auto timeStamp = std::chrono::steady_clock::now();
        if (!cycleDetectionList.empty()) {
            auto it = cycleDetectionList.begin();
            while (it != cycleDetectionList.end()) {
                if ((timeStamp - *it) >= std::chrono::minutes(10))
                    break;
                it++;
            }

            if (it != cycleDetectionList.end())
                cycleDetectionList.erase(it, cycleDetectionList.end());
        }
    }

}}
