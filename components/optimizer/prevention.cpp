#include "optimizer.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;

        void Optimizer::predictive_short_cycle_check_for_zone_(const ecodan::Status &status, OptimizerZone zone, bool is_cooling) {

            // base_setpoint != NAN is the single source of truth for "a boost is active".
            float &base_setpoint = (zone == OptimizerZone::ZONE_2) ? this->predictive_boost_base_z2_setpoint_ : this->predictive_boost_base_z1_setpoint_;

            if (this->is_system_hands_off(status) || !status.CompressorOn)
            {
                // Dropping out of protection: if we had raised the setpoint, put the
                // original back so a boosted value isn't left stranded on the heat pump.
                if (!isnan(base_setpoint))
                {
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Z%d: protection ended, restoring flow setpoint %.1f°C",
                        static_cast<uint8_t>(zone), base_setpoint);
                    this->set_flow_temp(base_setpoint, zone);
                    base_setpoint = NAN;
                }
                return;
            }

            float live_setpoint = this->get_flow_setpoint(zone);
            float actual_flow = this->get_feed_temp(zone);

            if (isnan(live_setpoint) || isnan(actual_flow))
            {
                // Leave any active boost in place — the boosted setpoint is still applied.
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Requested or Actual feed temperature unavailable. Exiting.");
                return;
            }

            float requested_flow = isnan(base_setpoint) ? live_setpoint : base_setpoint;

            float step_limited = this->enforce_step_limit(status, actual_flow, requested_flow, is_cooling);
            // Boost is happening exactly when this cycle adjusted the setpoint.
            bool boosting = (step_limited != requested_flow);

            if (boosting)
            {
                // Remember the original target once, on the first boost of a run.
                if (isnan(base_setpoint))
                    base_setpoint = requested_flow;
                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Z%d (%s): step-limit flow %.1f°C -> %.1f°C (actual feed %.1f°C)",
                    static_cast<uint8_t>(zone), is_cooling ? "cooling" : "heating", requested_flow, step_limited, actual_flow);
            }
            else if (!isnan(base_setpoint))
            {
                // No longer boosting — restore the original setpoint we were protecting.
                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Z%d (%s): boost cleared, restoring flow setpoint %.1f°C",
                    static_cast<uint8_t>(zone), is_cooling ? "cooling" : "heating", requested_flow);
            }

            auto limits = is_cooling ? this->get_cool_flow_limits(zone) : this->get_flow_limits(zone);
            step_limited = this->clamp_flow_temp(step_limited, limits.min, limits.max);
            this->set_flow_temp(step_limited, zone);

            // step_limited == requested (== base) here when not boosting, so the write above
            // already restored the original; drop the base so the next run starts fresh.
            if (!boosting)
                base_setpoint = NAN;
        }

        void Optimizer::clear_predictive_boost_(OptimizerZone zone, bool restore)
        {
            float &base_setpoint = (zone == OptimizerZone::ZONE_2) ? this->predictive_boost_base_z2_setpoint_ : this->predictive_boost_base_z1_setpoint_;

            // If asked, and we had raised the setpoint, put the original back before letting go.
            if (restore && !isnan(base_setpoint))
            {
                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Z%d: predictive released, restoring flow setpoint %.1f°C",
                    static_cast<uint8_t>(zone), base_setpoint);
                this->set_flow_temp(base_setpoint, zone);
            }
            base_setpoint = NAN;
        }

        void Optimizer::predictive_short_cycle_check()
        {
            auto &status = this->state_.ecodan_instance->get_status();

            // lockout active, monitor feed temp vs flow setpoint
            if (this->state_.status_short_cycle_lockout != nullptr && this->state_.status_short_cycle_lockout->state)
            {
                if (this->active_lockout_strategy_ == 1)
                {
                    this->apply_flow_lockout_setpoint_(status, OptimizerZone::ZONE_1, this->get_feed_temp(OptimizerZone::ZONE_1), false);
                    if (status.has_2zones())
                        this->apply_flow_lockout_setpoint_(status, OptimizerZone::ZONE_2, this->get_feed_temp(OptimizerZone::ZONE_2), false);
                }
                // Lockout owns the setpoint now — drop our boost claim without restoring.
                this->clear_predictive_boost_(OptimizerZone::ZONE_1, false);
                this->clear_predictive_boost_(OptimizerZone::ZONE_2, false);
                this->update_boost_sensor();
                return;
            }

            if (!this->state_.predictive_short_cycle_control_enabled->state)
            {
                // Feature turned off — undo any boost we still have applied.
                this->clear_predictive_boost_(OptimizerZone::ZONE_1, true);
                this->clear_predictive_boost_(OptimizerZone::ZONE_2, true);
                this->update_boost_sensor();
                return;
            }

            auto multizone_status = status.MultiZoneStatus;

            bool multizone_z1_active = status.has_2zones() && (multizone_status == 1 || multizone_status == 2);
            bool multizone_z2_active = status.has_2zones() && (multizone_status == 1 || multizone_status == 3);

            bool is_heating_z1 = status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1)
                || status.is_heating(esphome::ecodan::Zone::ZONE_1)
                || (multizone_z1_active && this->is_heating_active(status));

            bool is_heating_z2 = status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2)
                || status.is_heating(esphome::ecodan::Zone::ZONE_2)
                || (multizone_z2_active && this->is_heating_active(status));

            bool is_cooling_z1 = status.has_cooling()
                && (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_1) || status.is_cooling(esphome::ecodan::Zone::ZONE_1)
                    || (multizone_z1_active && this->is_cooling_active(status)));

            bool is_cooling_z2 = status.has_cooling()
                && (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_2) || status.is_cooling(esphome::ecodan::Zone::ZONE_2)
                    || (multizone_z2_active && this->is_cooling_active(status)));

            if (is_heating_z1)
                this->predictive_short_cycle_check_for_zone_(status, OptimizerZone::ZONE_1, false);
            else if (is_cooling_z1)
                this->predictive_short_cycle_check_for_zone_(status, OptimizerZone::ZONE_1, true);
            else
                this->clear_predictive_boost_(OptimizerZone::ZONE_1, true); // zone idle → restore + release

            if (status.has_2zones())
            {
                if (is_heating_z2)
                    this->predictive_short_cycle_check_for_zone_(status, OptimizerZone::ZONE_2, false);
                else if (is_cooling_z2)
                    this->predictive_short_cycle_check_for_zone_(status, OptimizerZone::ZONE_2, true);
                else
                    this->clear_predictive_boost_(OptimizerZone::ZONE_2, true);
            }
            else
            {
                this->clear_predictive_boost_(OptimizerZone::ZONE_2, true);
            }

            this->update_boost_sensor();
        }

        bool Optimizer::get_predictive_boost_state()
        {
            // A boost is active whenever a pre-boost base setpoint is being held for a zone.
            return !isnan(this->predictive_boost_base_z1_setpoint_) || !isnan(this->predictive_boost_base_z2_setpoint_);
        }

        void Optimizer::update_boost_sensor()
        {
            if (this->state_.status_predictive_boost_active != nullptr)
                this->state_.status_predictive_boost_active->publish_state(this->get_predictive_boost_state());
        }

        void Optimizer::restore_pre_lockout_state()
        {
            if (this->active_lockout_strategy_ == 1)
            {
                if (!isnan(this->flow_lockout_old_z1_setpoint_)) {
                    ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Restoring Z1 flow setpoint after Flow Control Lockout: %.1f°C", this->flow_lockout_old_z1_setpoint_);
                    this->set_flow_temp(this->flow_lockout_old_z1_setpoint_, OptimizerZone::ZONE_1);
                    this->flow_lockout_old_z1_setpoint_ = NAN;
                }
                if (!isnan(this->flow_lockout_old_z2_setpoint_)) {
                    ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Restoring Z2 flow setpoint after Flow Control Lockout: %.1f°C", this->flow_lockout_old_z2_setpoint_);
                    this->set_flow_temp(this->flow_lockout_old_z2_setpoint_, OptimizerZone::ZONE_2);
                    this->flow_lockout_old_z2_setpoint_ = NAN;
                }
            }
            else
            {
                auto flag_before_lockout = this->state_.ecodan_instance->get_svc_state_before_lockout();

                if (flag_before_lockout.has_value())
                {
                    auto flag = *flag_before_lockout;

                    auto &status = this->state_.ecodan_instance->get_status();
                    auto current_flag = status.get_svc_flags();
                    auto dhw_mask = esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_DHW;
                    flag = (flag & ~dhw_mask) | (current_flag & dhw_mask);

                    flag |= esphome::ecodan::CONTROLLER_FLAG::SERVER_CONTROL;
                    this->state_.ecodan_instance->set_controller_mode(flag, true);
                }
                else
                {
                    this->state_.ecodan_instance->set_controller_mode(esphome::ecodan::CONTROLLER_FLAG::SERVER_CONTROL, false);
                }
            }

            this->state_.lockout_expiration_timestamp = 0;
            if (this->state_.status_short_cycle_lockout != nullptr) {
                this->state_.status_short_cycle_lockout->publish_state(false);
            }
        }

        //   initial = true  : lockout is starting — snapshot the current setpoint (so we
        //                      can restore it later) and force the offset unconditionally.
        //   initial = false : chasing during the lockout — the feed temp can drift back
        //                      toward the forced setpoint 
        void Optimizer::apply_flow_lockout_setpoint_(const ecodan::Status &status, OptimizerZone zone, float actual_flow_temp, bool initial)
        {
            auto ecodan_zone = (zone == OptimizerZone::ZONE_2) ? esphome::ecodan::Zone::ZONE_2 : esphome::ecodan::Zone::ZONE_1;
            auto &mapped_old_setpoint_ = (zone == OptimizerZone::ZONE_2) ? this->flow_lockout_old_z2_setpoint_ : this->flow_lockout_old_z1_setpoint_;

            bool cooling = status.is_cooling(ecodan_zone);
            bool heating = status.is_heating(ecodan_zone);
            if (!cooling && !heating)
                return;

            if (isnan(actual_flow_temp))
            {
                if (initial)
                    ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Flow Control Lockout: Z%d actual feed temp unavailable. Skipping.", static_cast<uint8_t>(zone));
                return;
            }

            // Chasing but this zone was never forced (idle at lockout start) — nothing to do.
            if (!initial && isnan(mapped_old_setpoint_))
                return;

            const float FLOW_LOCKOUT_OFFSET = 5.0f;
            float target = cooling ? (actual_flow_temp + FLOW_LOCKOUT_OFFSET) : (actual_flow_temp - FLOW_LOCKOUT_OFFSET);

            if (initial)
            {
                mapped_old_setpoint_ = this->get_flow_setpoint(zone);
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Flow Control Lockout Z%d (%s): actual %.1f°C -> forcing setpoint %.1f°C (was %.1f°C)",
                    static_cast<uint8_t>(zone), cooling ? "cooling" : "heating", actual_flow_temp, target, mapped_old_setpoint_);
                this->set_flow_temp(target, zone);
                return;
            }

            float current_setpoint = this->get_flow_setpoint(zone);
            bool needs_chase = cooling ? (target > current_setpoint) : (target < current_setpoint);
            if (!needs_chase)
                return;

            ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Flow Control Lockout Z%d (%s): actual feed drifted to %.1f°C, re-chasing setpoint %.1f°C -> %.1f°C",
                static_cast<uint8_t>(zone), cooling ? "cooling" : "heating", actual_flow_temp, current_setpoint, target);
            this->set_flow_temp(target, zone);
        }

        void Optimizer::start_lockout()
        {
            auto &status = this->state_.ecodan_instance->get_status();
            time_t current_timestamp = status.timestamp();

            if (current_timestamp == -1)
            {
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Cannot start lockout: Ecodan controller time is not valid!");
                return;
            }

            int strategy = 0; // 0 = Server Control, 1 = Flow Control
            if (this->state_.lockout_strategy != nullptr && this->state_.lockout_strategy->active_index().has_value())
                strategy = this->state_.lockout_strategy->active_index().value();
            this->active_lockout_strategy_ = strategy;

            if (strategy == 1)
            {
                this->apply_flow_lockout_setpoint_(status, OptimizerZone::ZONE_1, this->get_feed_temp(OptimizerZone::ZONE_1), true);
                if (status.has_2zones())
                    this->apply_flow_lockout_setpoint_(status, OptimizerZone::ZONE_2, this->get_feed_temp(OptimizerZone::ZONE_2), true);
            }
            else
            {
                auto flag = status.get_svc_flags();

                if (status.ServerControl)
                    this->state_.ecodan_instance->set_svc_state_before_lockout(flag);
                else
                    this->state_.ecodan_instance->reset_svc_state_before_lockout();

                if (status.is_heating(esphome::ecodan::Zone::ZONE_1))
                    flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z1_HEATING;
                else if (status.is_cooling(esphome::ecodan::Zone::ZONE_1))
                    flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z1_COOLING;

                if (status.is_heating(esphome::ecodan::Zone::ZONE_2))
                    flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z2_HEATING;
                else if (status.is_cooling(esphome::ecodan::Zone::ZONE_2))
                    flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z2_COOLING;

                flag |= esphome::ecodan::CONTROLLER_FLAG::SERVER_CONTROL;
                this->state_.ecodan_instance->set_controller_mode(flag, true);
            }

            auto val = esphome::parse_number<int>(this->state_.lockout_duration->current_option());
            uint32_t duration_s = (val.has_value() ? *val : 0) * 60UL;
            if (duration_s > 0)
            {
                this->state_.lockout_expiration_timestamp = (uint32_t)(current_timestamp + duration_s);
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Lockout active. Expiration timestamp set to: %lu", this->state_.lockout_expiration_timestamp);
            }
        }

        void Optimizer::check_lockout_expiration()
        {
            uint32_t expiration = this->state_.lockout_expiration_timestamp;

            if (expiration == 0)
                return;

            auto &status = this->state_.ecodan_instance->get_status();
            time_t current_time_signed = status.timestamp();

            if (current_time_signed == -1)
            {
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Cannot check lockout: Ecodan time is not valid. Retrying in 30s.");
                return;
            }

            uint32_t current_time = (uint32_t)current_time_signed;

            if (current_time >= expiration)
            {
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Lockout period has expired (Ecodan Time: %lu, Expiration: %lu). Restoring operations.", current_time, expiration);
                this->restore_pre_lockout_state();
            }
            else
            {
                if (this->state_.status_short_cycle_lockout != nullptr && !this->state_.status_short_cycle_lockout->state)
                {
                    ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Booted during active lockout. Re-enabling lockout sensor. (Ecodan Time: %lu, Expiration: %lu)", current_time, expiration);
                    this->state_.status_short_cycle_lockout->publish_state(true);
                }
            }
        }

    } // namespace optimizer
} // namespace esphome