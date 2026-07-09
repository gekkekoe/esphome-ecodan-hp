#include "optimizer.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;

        Optimizer::Optimizer(OptimizerState state) : state_(state) {

            this->odin_mutex_ = xSemaphoreCreateMutex();

            auto update_if_changed = [this](float &storage, float new_val, auto callback) {
                if (std::isnan(new_val)) return;
                if (std::isnan(storage) || std::abs(storage - new_val) > 0.01f) {
                    auto previous = storage;
                    storage = new_val;
                    callback(new_val, previous);
                }
            };

            if (this->state_.hp_feed_temp != nullptr) {
                this->state_.hp_feed_temp->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_hp_feed_temp_, x, [this](float new_v, float) {
                        auto &status = this->state_.ecodan_instance->get_status();
                        if (this->is_dhw_active(status) || this->is_post_dhw_window(status))
                            this->on_feed_temp_change(new_v, OptimizerZone::SINGLE);
                    });
                });
            }

            if (this->state_.z1_feed_temp != nullptr) {
                this->state_.z1_feed_temp->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_z1_feed_temp_, x, [this](float new_v, float) {
                        auto &status = this->state_.ecodan_instance->get_status();
                        if (this->is_dhw_active(status) || this->is_post_dhw_window(status))
                            this->on_feed_temp_change(new_v, OptimizerZone::ZONE_1);
                    });
                });
            }

            if (this->state_.z2_feed_temp != nullptr) {
                this->state_.z2_feed_temp->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_z2_feed_temp_, x, [this](float new_v, float) {
                        auto &status = this->state_.ecodan_instance->get_status();
                        if (this->is_dhw_active(status) || this->is_post_dhw_window(status))
                            this->on_feed_temp_change(new_v, OptimizerZone::ZONE_2);
                    });
                });
            }

            if (this->state_.operation_mode != nullptr) {
                this->state_.operation_mode->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_operation_mode_, x, [this](float new_v, float old_v) {
                        this->on_operation_mode_change(static_cast<uint8_t>(new_v), static_cast<uint8_t>(old_v));
                    });
                });
            }

            if (this->state_.status_compressor != nullptr) {
                this->state_.status_compressor->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_compressor_status_, x, [this](float new_v, float old_v) {
                        this->on_compressor_state_change(static_cast<bool>(new_v), static_cast<bool>(old_v));
                    });
                });
            }

            if (this->state_.status_defrost != nullptr) {
                this->state_.status_defrost->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_defrost_status_, x, [this](float new_v, float old_v) {
                        this->on_defrost_state_change(static_cast<bool>(new_v), static_cast<bool>(old_v));
                    });
                });
            }
        }

    } // namespace optimizer
} // namespace esphome
