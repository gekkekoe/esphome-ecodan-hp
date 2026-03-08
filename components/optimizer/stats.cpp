#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"
#include "esphome/core/log.h"
#include <cmath>

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        // Raw Data Collection Model
        void Optimizer::update_heat_model()
        {
            if (this->state_.ecodan_instance == nullptr) return;
            auto &status = this->state_.ecodan_instance->get_status();
            uint32_t now = millis();
            bool is_running = (status.CompressorFrequency > 0) || status.CompressorOn;
            bool is_heating_active = status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;

            if (this->last_check_ms_ != 0) {    
                float minutes_passed = (now - this->last_check_ms_) / 60000.0f;

                if (is_running && is_heating_active) {
                    this->daily_runtime_global += minutes_passed;
                }
            }
            this->last_check_ms_ = now;

            // Track outside temperature periodically
            if (!isnan(status.OutsideTemperature)) {
                this->daily_outside_temp_sum_ += status.OutsideTemperature;
                this->daily_outside_temp_count_++;
            }

            // Track room temperature periodically
            float z1_temp = this->get_room_current_temp(OptimizerZone::ZONE_1);
            float current_room_temp = z1_temp;

            // If the system has 2 zones, calculate the average of both rooms
            if (status.has_2zones()) {
                float z2_temp = this->get_room_current_temp(OptimizerZone::ZONE_2);
                
                // Safely average them if both are valid floats
                if (!isnan(z1_temp) && !isnan(z2_temp)) {
                    current_room_temp = (z1_temp + z2_temp) / 2.0f;
                } else if (isnan(z1_temp) && !isnan(z2_temp)) {
                    // Fallback to Zone 2 if Zone 1 is missing for some reason
                    current_room_temp = z2_temp;
                }
            }

            if (!isnan(current_room_temp)) {
                this->daily_room_temp_sum_ += current_room_temp;
                this->daily_room_temp_count_++;
                
                // Track daily min/max to determine the temperature delta
                if (current_room_temp < this->daily_room_temp_min_) this->daily_room_temp_min_ = current_room_temp;
                if (current_room_temp > this->daily_room_temp_max_) this->daily_room_temp_max_ = current_room_temp;
            }

            // Track max output power periodically
            if (this->state_.computed_output_power != nullptr) {
                float out_pwr = this->state_.computed_output_power->state;
                if (!isnan(out_pwr) && out_pwr > this->daily_max_output_power_) {
                    this->daily_max_output_power_ = out_pwr;
                }
            }

            int current_day = status.ControllerDateTime.tm_yday;
            
            // Initialize on boot to prevent jump
            if (this->last_processed_day_ == -1) {
                this->last_processed_day_ = current_day;
                return;
            }

            // Check if day changed (e.g. 303 -> 304)
            if (current_day != this->last_processed_day_) {
                ESP_LOGI(OPTIMIZER_TAG, "Raw Data Collection: Day transition detected (%d -> %d). Saving stats...", 
                         this->last_processed_day_, current_day);
                
                this->update_learning_model(this->last_processed_day_);

                // Reset variables for the new day
                this->last_processed_day_ = current_day;
                
                this->daily_outside_temp_sum_ = 0.0f;
                this->daily_outside_temp_count_ = 0;
                
                this->daily_room_temp_sum_ = 0.0f;
                this->daily_room_temp_count_ = 0;
                this->daily_room_temp_min_ = 99.0f;
                this->daily_room_temp_max_ = -99.0f;
                this->daily_max_output_power_ = 0.0f;
                
                this->daily_runtime_global = 0.0f;
            }

            // Set latest energy snapshots
            if (is_running && is_heating_active) {
                this->last_total_heating_produced_ = this->state_.daily_heating_produced->state;

                if (this->state_.solver_kwh_meter_feedback_source == nullptr || 
                    this->state_.solver_kwh_meter_feedback_source->active_index().value_or(0) == 0) {
                    this->last_total_heating_consumed_ = this->state_.daily_heating_consumed->state;
                }
                else {
                    // Ensure the feedback number is also not null before reading
                    this->last_total_heating_consumed_ = (this->state_.solver_kwh_meter_feedback != nullptr) ? 
                                                        this->state_.solver_kwh_meter_feedback->state : 0.0f;
                }
            }
        }

        void Optimizer::update_learning_model(int day_of_year)
        {
            // Calculate average outside temperature
            float avg_outside = 7.0f; // Safe fallback
            if (this->daily_outside_temp_count_ > 0) {
                avg_outside = this->daily_outside_temp_sum_ / this->daily_outside_temp_count_;
            }

            // Calculate average room temperature and the daily delta
            float avg_room = 20.0f; // Safe fallback
            float delta_room = 0.0f;
            if (this->daily_room_temp_count_ > 0) {
                avg_room = this->daily_room_temp_sum_ / this->daily_room_temp_count_;
                if (this->daily_room_temp_max_ >= this->daily_room_temp_min_) {
                    delta_room = this->daily_room_temp_max_ - this->daily_room_temp_min_;
                }
            }

            // Retrieve runtime (convert minutes to hours)
            float runtime_hours = this->daily_runtime_global / 60.0f;
            float heat_produced_kwh = this->last_total_heating_produced_;
            float elec_consumed_kwh = this->last_total_heating_consumed_;
            float max_out_kw = this->daily_max_output_power_;

            ESP_LOGI(OPTIMIZER_TAG, "Daily Raw Stats: Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh, MaxOut=%.1fkW, AvgOut=%.1fC, AvgRoom=%.1fC, DeltaRoom=%.1fC",
                     heat_produced_kwh, elec_consumed_kwh, runtime_hours, max_out_kw, avg_outside, avg_room, delta_room);
            
            // Only update the globals if significant heating occurred (> 2 kWh produced, > 1 hour run)
            if (heat_produced_kwh < 2.0f || runtime_hours < 1.0f) {
                ESP_LOGD(OPTIMIZER_TAG, "Stats Collection: Skipped. Insufficient heating data today.");
                return;
            }

            // Helper for Exponential Moving Average (EMA) on Number components
            auto update_ema_num = [](esphome::number::Number *comp, float observed, float alpha) {
                if (comp == nullptr) return;
                float current = comp->state;
                if (current <= 0.01f || std::isnan(current)) current = observed; // Initialize if empty
                else current = (alpha * observed) + ((1.0f - alpha) * current);
                
                // Publish back to ESPHome so the dashboard and NVS flash are updated
                comp->publish_state(current); 
            };

            const float ALPHA = 0.15f; 

            update_ema_num(this->state_.num_raw_heat_produced, heat_produced_kwh, ALPHA);
            update_ema_num(this->state_.num_raw_elec_consumed, elec_consumed_kwh, ALPHA);
            update_ema_num(this->state_.num_raw_runtime_hours, runtime_hours, ALPHA);
            update_ema_num(this->state_.num_raw_avg_outside_temp, avg_outside, ALPHA);
            update_ema_num(this->state_.num_raw_avg_room_temp, avg_room, ALPHA);
            update_ema_num(this->state_.num_raw_delta_room_temp, delta_room, ALPHA);
            update_ema_num(this->state_.num_raw_max_output, max_out_kw, ALPHA);

            ESP_LOGI(OPTIMIZER_TAG, "Numbers updated (15%% EMA): Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh, AvgOut=%.1fC, AvgRoom=%.1fC, DeltaRoom=%.1fC",
                     this->state_.num_raw_heat_produced->state, this->state_.num_raw_elec_consumed->state, 
                     this->state_.num_raw_runtime_hours->state, this->state_.num_raw_avg_outside_temp->state,
                     this->state_.num_raw_avg_room_temp->state, this->state_.num_raw_delta_room_temp->state);
        }

    } // namespace optimizer
} // namespace esphome