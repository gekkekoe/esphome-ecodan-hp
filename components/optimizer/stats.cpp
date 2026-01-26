#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"
#include "esphome/core/log.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        // Self-Learning Physics Model
        void Optimizer::update_heat_model()
        {
            if (this->state_.ecodan_instance == nullptr) return;
            auto &status = this->state_.ecodan_instance->get_status();
            uint32_t now = millis();

            if (this->last_check_ms_ != 0) {    
                float minutes_passed = (now - this->last_check_ms_) / 60000.0f;
                bool is_running = (status.CompressorFrequency > 0) || status.CompressorOn;
                bool is_heating_active = status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;

                if (is_running && is_heating_active) {
                    this->daily_runtime_global += minutes_passed;
                }
            }
            this->last_check_ms_ = now;

            // We assume this function is called periodically (e.g., every 30s).
            if (!isnan(status.OutsideTemperature)) {
                this->daily_temp_sum_ += status.OutsideTemperature;
                this->daily_temp_count_++;
            }

            int current_day = status.ControllerDateTime.tm_yday;
            
            // Initialize on boot (prevent jump)
            if (this->last_processed_day_ == -1) {
                this->last_processed_day_ = current_day;
                return;
            }

            // Check if day changed (e.g. 303 -> 304)
            if (current_day != this->last_processed_day_) {
                ESP_LOGI(OPTIMIZER_TAG, "Learning: Day transition detected (%d -> %d). Processing model...", 
                         this->last_processed_day_, current_day);
                
                this->update_learning_model(this->last_processed_day_);

                // Reset for new day
                this->last_processed_day_ = current_day;
                this->daily_temp_sum_ = 0.0f;
                this->daily_temp_count_ = 0;
                
                // Reset daily runtime counter via global
                this->daily_runtime_global = 0;
            }

            // set latest snapshot
            this->last_total_heating_produced_ = this->state_.daily_heating_produced->state;
            if (this->state_.solver_kwh_meter_feedback_source->active_index().value_or(0) == 0) {
                this->last_total_heating_consumed_ = this->state_.daily_heating_consumed->state;
            }
            else {
                this->last_total_heating_consumed_ = this->state_.solver_kwh_meter_feedback->state;
            }
        }

        void Optimizer::update_learning_model(int day_of_year)
        {
            if (this->daily_temp_count_ == 0) return;
            
            // Calculate Average Temperature of the past day
            float avg_temp_day = this->daily_temp_sum_ / this->daily_temp_count_;
            
            // Retrieve runtime (minutes -> hours)
            float runtime_hours = this->daily_runtime_global / 60.0f;
            
            float heat_produced_kwh = this->last_total_heating_produced_;
            float elec_consumed_kwh = this->last_total_heating_consumed_;

            ESP_LOGI(OPTIMIZER_TAG, "Learning Stats: AvgTemp=%.1fC, Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh",
                     avg_temp_day, heat_produced_kwh, elec_consumed_kwh, runtime_hours);
            
            // Only learn if significant heating occurred (> 5 kWh produced, > 2 hours run)
            if (heat_produced_kwh < 5.0f || runtime_hours < 2.0f) {
                ESP_LOGD(OPTIMIZER_TAG, "Learning: Skipped. Insufficient data/runtime.");
                return;
            }
            // Only learn Heat Loss if it was cold enough (Delta T > 5 degrees)
            float delta_t = 20.0f - avg_temp_day;
            if (delta_t < 5.0f) {
                 ESP_LOGD(OPTIMIZER_TAG, "Learning: Skipped. Outside temp too high (%.1fC).", avg_temp_day);
                 return;
            }

            // House Factor (Heat Loss)
            // Formula: kW per Degree = (Total kWh / 24h) / Delta T
            float observed_heat_loss = (heat_produced_kwh / 24.0f) / delta_t;

            // Base COP (Normalized to 7°C)
            float measured_cop = heat_produced_kwh / elec_consumed_kwh;
            if (std::isinf(measured_cop) || measured_cop > 10.0f) measured_cop = 0.0f;

            // Correction: Normalize COP to 7C using standard curve (0.1 per degree)
            // If temp was 2C, we add (5 * 0.1) to estimate what COP would be at 7C.
            float temp_diff_from_7 = 7.0f - avg_temp_day;
            float observed_base_cop = measured_cop + (temp_diff_from_7 * 0.1f);

            // Thermal Output (Average kW)
            float observed_thermal_output = heat_produced_kwh / runtime_hours;

            // Electrical Power (Average kW)
            float observed_elec_power = elec_consumed_kwh / runtime_hours;

            // Alpha is weight for moving average
            const float ALPHA = 0.05f;
            const float ALPHA_HW = 0.1f;

            // Helper for EMA
            auto update_ema = [](float &current, float observed, float alpha) {
                if (current <= 0.01f) current = observed; // Init if empty
                else current = (alpha * observed) + ((1.0f - alpha) * current);
            };

            // Update Globals (Writes to NVS)
            update_ema(this->state_.learned_heat_loss_global, observed_heat_loss, ALPHA);
            update_ema(this->state_.learned_base_cop_global, observed_base_cop, ALPHA);
            update_ema(this->state_.learned_thermal_output_global, observed_thermal_output, ALPHA_HW);
            update_ema(this->state_.learned_elec_power_global, observed_elec_power, ALPHA_HW);

            ESP_LOGI(OPTIMIZER_TAG, "Learning Updated: HeatLoss=%.3f (kW/K), BaseCOP=%.2f, Output=%.1f kW, Power=%.1f kW",
                     this->state_.learned_heat_loss_global,
                     this->state_.learned_base_cop_global,
                     this->state_.learned_thermal_output_global,
                     this->state_.learned_elec_power_global);
        }

    } // namespace optimizer
} // namespace esphome