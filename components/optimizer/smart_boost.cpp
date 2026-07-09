#include "optimizer.h"
#include <algorithm>
#include <cmath>

using std::isnan;

namespace esphome
{
    namespace optimizer
    {

        float Optimizer::calculate_smart_boost(int profile, float error) {

            if (!this->state_.smart_boost_enabled->state) {
                this->current_stagnation_boost_ = 1.0f;
                this->stagnation_start_time_ = 0;
                return 1.0f;
            }

            uint32_t initial_wait_ms;
            uint32_t step_interval_ms;
            float max_boost_limit;
            float step_size;

            if (profile <= 1) {         // UFH
                initial_wait_ms   = 3600000;    // 60 min
                step_interval_ms  = 1800000;    // 30 min
                max_boost_limit   = 1.5f;
                step_size         = 0.15f;
            } else if (profile >= 4) {  // Radiator
                initial_wait_ms   = 1200000;    // 20 min
                step_interval_ms  = 600000;     // 10 min
                max_boost_limit   = 2.5f;
                step_size         = 0.20f;
            } else {                    // Hybrid
                initial_wait_ms   = 2700000;    // 45 min
                step_interval_ms  = 1200000;    // 20 min
                max_boost_limit   = 2.0f;
                step_size         = 0.15f;
            }

            bool is_stagnant = (error > 0.1f) && (error >= this->last_error_ - 0.01f);
            float target_boost = 1.0f;

            if (is_stagnant) {
                if (this->stagnation_start_time_ == 0)
                    this->stagnation_start_time_ = millis();

                uint32_t stuck_duration = millis() - this->stagnation_start_time_;

                if (stuck_duration > initial_wait_ms) {
                    uint32_t overtime  = stuck_duration - initial_wait_ms;
                    int      steps     = overtime / step_interval_ms;
                    float    extra     = (steps + 1) * step_size;
                    target_boost = std::min(1.0f + extra, max_boost_limit);
                } else {
                    target_boost = this->current_stagnation_boost_;
                }

                this->current_stagnation_boost_ = target_boost;

            } else {
                // Decay when error improves
                this->stagnation_start_time_ = 0;

                if (this->current_stagnation_boost_ > 1.0f) {
                    const float UPDATE_INTERVAL_MS = 300000.0f; // 5 min
                    float decay = step_size * (UPDATE_INTERVAL_MS / (float)step_interval_ms);
                    this->current_stagnation_boost_ = std::max(this->current_stagnation_boost_ - decay, 1.0f);
                } else {
                    this->current_stagnation_boost_ = 1.0f;
                }
            }

            this->last_error_ = error;
            return this->current_stagnation_boost_;
        }

    } // namespace optimizer
} // namespace esphome
