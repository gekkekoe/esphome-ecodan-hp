#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;
        // callbacks to monitor step down, need to keep within 1.0C else compressor will halt
        void Optimizer::on_feed_temp_change(float actual_flow_temp, OptimizerZone zone) {            
            if (std::isnan(actual_flow_temp) 
                || this->state_.status_short_cycle_lockout->state
                || !this->state_.auto_adaptive_control_enabled->state) {
                return;
            }

            auto &status = this->state_.ecodan_instance->get_status();

            if (status.has_independent_zone_temps()) 
            {
                if (zone == OptimizerZone::SINGLE) {
                    return;
                }
            } 
            else 
            {
                if (zone != OptimizerZone::SINGLE) {
                    return;
                }
            }

            float current_flow_setpoint = (zone == OptimizerZone::ZONE_2) ? status.Zone2FlowTemperatureSetPoint : status.Zone1FlowTemperatureSetPoint;
            float adjusted_flow = actual_flow_temp;

            time_t current_timestamp = status.timestamp();
            bool post_dhw_window = this->is_post_dhw_window(status);

            if (this->is_dhw_active(status)) {
                if (status.DhwFlowTemperatureSetPoint > actual_flow_temp)
                    return;

                // adjust during first part of heating
                adjusted_flow += 0.5f;
                // Each time we adjust for dhw, set the post dhw timer expiration
                time_t current_timestamp = status.timestamp();
                if (status.CompressorOn && current_timestamp > 0) {
                    const uint32_t after_dhw_monitoring_duration_s = 5 * 60UL;
                    this->dhw_post_run_expiration_ = (uint32_t)(current_timestamp + after_dhw_monitoring_duration_s);
                    ESP_LOGD(OPTIMIZER_TAG, "Setting monitor expiration to: %d", this->dhw_post_run_expiration_);
                }
            }
            else if (post_dhw_window) {
                if (!this->is_heating_active(status)) {
                    // no demand, restore saved setpoint
                    float restore_val = (zone == OptimizerZone::ZONE_2) ? this->dhw_old_z2_setpoint_ : this->dhw_old_z1_setpoint_;
                    
                    if (!std::isnan(restore_val)) {
                        adjusted_flow = restore_val;
                        
                        // Also stop timer
                        this->dhw_post_run_expiration_ = 0; 
                        this->dhw_old_z1_setpoint_ = NAN;
                        this->dhw_old_z2_setpoint_ = NAN;
                        ESP_LOGD(OPTIMIZER_TAG, "Post-DHW: Heat demand gone. Restoring original setpoint %.1f and clearing timer.", adjusted_flow);
                    }
                }
                else {
                    // also add 0.5 during post dhw while heating
                    adjusted_flow += 0.5f;
                }
            }   
            else {
                adjusted_flow = enforce_step_down(status, actual_flow_temp, current_flow_setpoint);
            }
    
            if (adjusted_flow != current_flow_setpoint)
            {
                set_flow_temp(adjusted_flow, zone);
            }
        }

        bool Optimizer::set_flow_temp(float flow, OptimizerZone zone) {
            auto &status = this->state_.ecodan_instance->get_status();
            
            if (status.has_independent_zone_temps())
            {
                if (zone == OptimizerZone::ZONE_1) {
                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) && status.Zone1FlowTemperatureSetPoint != flow)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z1 Heat Flow -> %.1f°C (%.1f°C)", flow, status.Zone1FlowTemperatureSetPoint);
                        this->state_.ecodan_instance->set_flow_target_temperature(flow, esphome::ecodan::Zone::ZONE_1);
                        return true;
                    }
                    else if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_1) && status.Zone1FlowTemperatureSetPoint != flow)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z1 Cool Flow -> %.1f°C (%.1f°C)", flow, status.Zone1FlowTemperatureSetPoint);
                        this->state_.ecodan_instance->set_flow_target_temperature(flow, esphome::ecodan::Zone::ZONE_1);
                        return true;
                    }

                } else if (zone == OptimizerZone::ZONE_2) {
                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) && status.Zone2FlowTemperatureSetPoint != flow)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z2 Heat Flow -> %.1f°C (%.1f°C)", flow, status.Zone2FlowTemperatureSetPoint);
                        this->state_.ecodan_instance->set_flow_target_temperature(flow, esphome::ecodan::Zone::ZONE_2);
                        return true;
                    }
                    else if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_2) && status.Zone2FlowTemperatureSetPoint != flow)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z2 Cool Flow -> %.1f°C (%.1f°C)", flow, status.Zone2FlowTemperatureSetPoint);
                        this->state_.ecodan_instance->set_flow_target_temperature(flow, esphome::ecodan::Zone::ZONE_2);
                       return true;
                    }
                }
            }
            else
            {
                if (status.Zone1FlowTemperatureSetPoint != flow) {
                    if (status.HeatingCoolingMode == esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Dependent Heat Flow -> %.1f°C (%.1f°C)", flow, status.Zone1FlowTemperatureSetPoint);
                        this->state_.ecodan_instance->set_flow_target_temperature(flow, esphome::ecodan::Zone::ZONE_1);
                        return true;
                    }
                    else if (status.HeatingCoolingMode == esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Dependent Cool Flow -> %.1f°C (%.1f°C)", flow, status.Zone1FlowTemperatureSetPoint);
                        this->state_.ecodan_instance->set_flow_target_temperature(flow, esphome::ecodan::Zone::ZONE_1);
                        return true;
                    }
                }
            }
            return false;
        }

        float Optimizer::enforce_step_down(const ecodan::Status &status, float actual_flow_temp, float calculated_flow) 
        {
            const float MAX_FEED_STEP_DOWN = 1.0f;
            const float MAX_FEED_STEP_DOWN_ADJUSTMENT = 0.5f;
            
            if ((actual_flow_temp - calculated_flow) > MAX_FEED_STEP_DOWN)
            {
                ESP_LOGW(OPTIMIZER_TAG, "Flow adjust: %.2f°C to prevent compressor stop! (setpoint: %.2f°C is %.2f°C below actual feed temp)",
                        actual_flow_temp - MAX_FEED_STEP_DOWN_ADJUSTMENT, calculated_flow, (actual_flow_temp - calculated_flow));

                return actual_flow_temp - MAX_FEED_STEP_DOWN_ADJUSTMENT;
            }
            return calculated_flow;
        }

        void Optimizer::on_compressor_stop()
        {
            auto &status = this->state_.ecodan_instance->get_status();
            bool stand_alone_predictive_active = !this->state_.auto_adaptive_control_enabled->state && this->state_.predictive_short_cycle_control_enabled->state;

            ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Compressor stop event: stand-alone-cycle prevention: %d, saved z1 flow setpoint: %.1f, saved z2 flow setpoint: %.1f"
                , stand_alone_predictive_active, this->pcp_old_z1_setpoint_, this->pcp_old_z2_setpoint_);

            // don't restore feed temp when defrost is active
            if (!status.DefrostActive && stand_alone_predictive_active && (!isnan(this->pcp_old_z1_setpoint_) || !isnan(this->pcp_old_z2_setpoint_)))
            {
                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Restoring flow setpoint after predictive boost.");

                if (!isnan(this->pcp_old_z1_setpoint_)) {
                    this->state_.ecodan_instance->set_flow_target_temperature(this->pcp_old_z1_setpoint_, esphome::ecodan::Zone::ZONE_1);
                    this->pcp_old_z1_setpoint_ = NAN;
                    this->pcp_adjustment_z1_ = 0.0f;
                }

                if (status.has_2zones() && !isnan(this->pcp_old_z2_setpoint_)) {
                    this->state_.ecodan_instance->set_flow_target_temperature(this->pcp_old_z2_setpoint_, esphome::ecodan::Zone::ZONE_2);
                    this->pcp_old_z2_setpoint_ = NAN;
                    this->pcp_adjustment_z2_ = 0.0f;
                }
            }

            if (this->state_.lockout_duration->active_index().value_or(0) == 0)
            {
                this->compressor_start_time_ = 0;
                return;
            }

            if (this->compressor_start_time_ == 0 || this->state_.status_short_cycle_lockout->state || status.DefrostActive)
                return;

            uint32_t min_duration_ms = this->state_.minimum_compressor_on_time->state * 60000UL;
            if (min_duration_ms == 0)
                return;

            if (status.is_heating(esphome::ecodan::Zone::ZONE_1) || status.is_cooling(esphome::ecodan::Zone::ZONE_1) || status.is_heating(esphome::ecodan::Zone::ZONE_2) || status.is_cooling(esphome::ecodan::Zone::ZONE_2))
            {

                uint32_t run_duration_ms = millis() - this->compressor_start_time_;
                if (run_duration_ms < min_duration_ms)
                {
                    this->compressor_start_time_ = 0;
                    this->state_.status_short_cycle_lockout->publish_state(true);
                    this->start_lockout();
                }
            }
        }
        
        void Optimizer::on_operation_mode_change(uint8_t new_mode, uint8_t previous_mode)
        {
            if (new_mode == previous_mode) 
                return;

            auto heating_mode = static_cast<uint8_t>(esphome::ecodan::Status::OperationMode::HEAT_ON);
            auto dhw_mode = static_cast<uint8_t>(esphome::ecodan::Status::OperationMode::DHW_ON);
            auto &status = this->state_.ecodan_instance->get_status();

            if (new_mode == dhw_mode && previous_mode == heating_mode) {
                this->dhw_old_z1_setpoint_ = status.Zone1FlowTemperatureSetPoint;
                this->dhw_old_z2_setpoint_ = status.Zone2FlowTemperatureSetPoint;

                ESP_LOGD(OPTIMIZER_TAG, "Switching to DHW. Saved heating setpoints: Z1=%.1f, Z2=%.1f", 
                    this->dhw_old_z1_setpoint_, this->dhw_old_z2_setpoint_);
            }

            if (new_mode == heating_mode && previous_mode != heating_mode && this->state_.auto_adaptive_control_enabled->state) {
                ESP_LOGD(OPTIMIZER_TAG, "Operation Mode Changed to heating: %d -> %d", previous_mode, new_mode);
                this->run_auto_adaptive_loop();
            }
        }

        void Optimizer::on_defrost_state_change(bool x, bool x_previous) 
        {      
            if (x_previous && !x)
            {
                ESP_LOGD(OPTIMIZER_TAG, "Defrost stop: triggering auto-adaptive loop.");
                this->run_auto_adaptive_loop();
            }
        }

        void Optimizer::on_compressor_state_change(bool x, bool x_previous)
        {
            if (millis() < 60000) 
                return;

            if (x_previous && !x)
            {
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Compressor STOP detected");
                this->on_compressor_stop();
            }
            else if (!x_previous && x)
            {
                auto &status = this->state_.ecodan_instance->get_status();
                if (!status.DefrostActive)
                {
                    ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Compressor START detected");
                    this->compressor_start_time_ = millis();
                    this->last_check_ms_ = this->compressor_start_time_;
                }
                if (this->state_.auto_adaptive_control_enabled->state)
                {
                    ESP_LOGD(OPTIMIZER_TAG, "Compressor start: triggering auto-adaptive loop.");
                    this->run_auto_adaptive_loop();
                }
            }
        }

    } // namespace optimizer
} // namespace esphome