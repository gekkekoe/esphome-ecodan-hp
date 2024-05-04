#include "ecodan.h"

#include "esp_log.h"
#include "esphome.h"

#include <HardwareSerial.h>
#include <freertos/task.h>
#include <functional>

#include <mutex>
#include <queue>
#include <thread>

namespace esphome {
namespace ecodan 
{

#pragma region Commands
    void EcodanHeatpump::set_room_temperature(float newTemp, esphome::ecodan::SetZone zone)
    {
        Message cmd{MsgType::SET_CMD, SetType::ROOM_SETTINGS};

        if (zone == SetZone::ZONE_1) {
            cmd[1] = 0x02;
            cmd.set_float16(newTemp, 4);
        }
        else if (zone == SetZone::ZONE_2) {
            cmd[1] = 0x08;
            cmd.set_float16(newTemp, 6);
        }

        cmd[3]= status.HeatingCoolingMode == Status::HpMode::COOL_ROOM_TEMP || status.HeatingCoolingMode == Status::HpMode::COOL_FLOW_TEMP 
            ? 1 : 0;

        // cmd[1] = SET_SETTINGS_FLAG_ZONE_TEMPERATURE;


        // auto flag = status.HeatingCoolingMode == Status::HpMode::COOL_ROOM_TEMP
        //     ? static_cast<uint8_t>(Status::HpMode::COOL_ROOM_TEMP) : static_cast<uint8_t>(Status::HpMode::HEAT_ROOM_TEMP);

        // if (zone == SetZone::BOTH || zone == SetZone::ZONE_1) {
        //     cmd[6] = flag;
        //     cmd.set_float16(newTemp, 10);
        // }
            
        // if (zone == SetZone::BOTH || zone == SetZone::ZONE_2) {
        //     cmd[7] = flag;
        //     cmd.set_float16(newTemp, 12);
        // }
        //ESP_LOGE(TAG, cmd.debug_dump_packet().c_str());

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_flow_target_temperature(float newTemp, esphome::ecodan::SetZone zone)
    {

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        
        if (zone == SetZone::ZONE_1) {
            cmd[1] = 0x80;
            cmd.set_float16(newTemp, 10);
        }
        else if (zone == SetZone::ZONE_2) {
            cmd[2] = 0x02;
            cmd.set_float16(newTemp, 12);
        }
        
        switch (status.HeatingCoolingMode)
        {
        case Status::HpMode::HEAT_FLOW_TEMP:
        case Status::HpMode::HEAT_ROOM_TEMP:
            cmd[6] = 1;
            break;
        case Status::HpMode::COOL_ROOM_TEMP:
            cmd[6] = 3;
            break;
        case Status::HpMode::COOL_FLOW_TEMP:
            cmd[6] = 4;
            break;
        default:
            cmd[6] = 0;
            break;
        }

        // cmd[1] = SET_SETTINGS_FLAG_ZONE_TEMPERATURE;
        // cmd[2] = static_cast<uint8_t>(zone);
        
        // auto flag = status.HeatingCoolingMode == Status::HpMode::COOL_FLOW_TEMP
        //     ? static_cast<uint8_t>(Status::HpMode::COOL_FLOW_TEMP) : static_cast<uint8_t>(Status::HpMode::HEAT_FLOW_TEMP);

        // if (zone == SetZone::BOTH || zone == SetZone::ZONE_1) {
        //     cmd[6] = flag;
        //     cmd.set_float16(newTemp, 10);
        // }
            
        // if (zone == SetZone::BOTH || zone == SetZone::ZONE_2) {
        //     cmd[7] = flag;
        //     cmd.set_float16(newTemp, 12);
        // }
        // ESP_LOGE(TAG, cmd.debug_dump_packet().c_str());
        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_dhw_target_temperature(float newTemp)
    {

        if (newTemp > get_max_dhw_temperature())
        {
            ESP_LOGI(TAG, "DHW setting exceeds maximum allowed (%s)!", get_max_dhw_temperature());
            return;
        }

        if (newTemp < get_min_dhw_temperature())
        {
            ESP_LOGI(TAG, "DHW setting is lower than minimum allowed (%s)!", get_min_dhw_temperature());
            return;
        }

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_DHW_TEMPERATURE;
        cmd.set_float16(newTemp, 8);

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_dhw_mode(std::string mode)
    {
        Status::DhwMode dhwMode = Status::DhwMode::NORMAL;

        if (mode == "Off")
            return set_dhw_force(false);
        else if (mode == "Normal")
            dhwMode = Status::DhwMode::NORMAL;
        else if (mode == "Eco")
            dhwMode = Status::DhwMode::ECO;
        else
            return;

        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_DHW_MODE;
        cmd[5] = static_cast<uint8_t>(dhwMode);

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_dhw_force(bool on)
    {   
        Message cmd{MsgType::SET_CMD, SetType::DHW_SETTING};
        cmd[1] = SET_SETTINGS_FLAG_MODE_TOGGLE;
        cmd[3] = on ? 1 : 0; // bit[3] of payload is DHW force, bit[2] is Holiday mode.

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_holiday(bool on)
    {   
        Message cmd{MsgType::SET_CMD, SetType::DHW_SETTING};
        cmd[1] = SET_SETTINGS_FLAG_HOLIDAY_MODE_TOGGLE;
        cmd[4] = on ? 1 : 0;

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_power_mode(bool on)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_MODE_TOGGLE;
        cmd[3] = on ? 1 : 0;

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_hp_mode(int mode)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_HP_MODE;
        cmd[6] = mode;

        schedule_cmd(cmd);
    }

    bool EcodanHeatpump::schedule_cmd(Message& cmd)
    {
        bool emplaced = false;
        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};
            if (!cmdQueue.empty()) {
                cmdQueue.emplace_back(std::move(cmd));
                emplaced = true;
            }
        }
        
        if (!emplaced && !serial_tx(cmd)) {
            ESP_LOGI(TAG, "Unable enqueue cmd, flushing queued requests...");
            connected = false;
            return false;
        }

        return true;
    }

    void EcodanHeatpump::clear_command_queue()
    {
        std::lock_guard<std::mutex> lock{cmdQueueMutex};

        while (!cmdQueue.empty())
            cmdQueue.pop_back();
    }

    bool EcodanHeatpump::dispatch_next_cmd()
    {
        Message msg;
        {
            std::lock_guard<std::mutex> lock{cmdQueueMutex};

            if (cmdQueue.empty())
            {
                return true;
            }

            msg = std::move(cmdQueue.front());
            cmdQueue.pop_front();
        }
        
        //ESP_LOGI(TAG, msg.debug_dump_packet().c_str());

        if (!serial_tx(msg))
        {
            ESP_LOGI(TAG, "Unable to dispatch status update request, flushing queued requests...");

            clear_command_queue();

            connected = false;
            return false;
        }

        return true;
    }

    bool EcodanHeatpump::begin_connect()
    {
        Message cmd{MsgType::CONNECT_CMD};
        char payload[3] = {0xCA, 0x01};
        cmd.write_payload(payload, sizeof(payload));

        ESP_LOGI(TAG, "Attempt to tx CONNECT_CMD!");
        if (!serial_tx(cmd))
        {
            ESP_LOGI(TAG, "Failed to tx CONNECT_CMD!");
            return false;
        }

        return true;
    }

    bool EcodanHeatpump::begin_get_status()
    {
        {
            std::unique_lock<std::mutex> lock{cmdQueueMutex, std::try_to_lock};

            if (!lock)
            {
                ESP_LOGI(TAG, "Unable to acquire lock for status query, owned by another thread!");
                delay(1);
            }

            if (!cmdQueue.empty())
            {
                ESP_LOGI(TAG, "command queue was not empty when queueing status query: %u", cmdQueue.size());

                // while (!cmdQueue.empty())
                //     cmdQueue.pop();
            }

            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::DEFROST_STATE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::ERROR_STATE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::COMPRESSOR_FREQUENCY);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::FORCED_DHW_STATE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::HEATING_POWER);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::TEMPERATURE_CONFIG);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::SH_TEMPERATURE_STATE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::DHW_TEMPERATURE_STATE_A);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::DHW_TEMPERATURE_STATE_B);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::EXTERNAL_STATE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::ACTIVE_TIME);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::PUMP_STATUS);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::FLOW_RATE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::MODE_FLAGS_A);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::MODE_FLAGS_B);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::ENERGY_USAGE);
            cmdQueue.emplace_front(MsgType::GET_CMD, GetType::ENERGY_DELIVERY);
            cmdQueue.emplace_front(MsgType::GET_CONFIGURATION, GetType::HARDWARE_CONFIGURATION);
        }

        return dispatch_next_cmd();
    }

#pragma endregion Commands


}}