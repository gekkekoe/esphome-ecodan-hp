#include "ecodan.h"

#include "esphome.h"

#include <functional>
#include <queue>

namespace esphome {
namespace ecodan 
{
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
        
        uint8_t flag = 0;
        switch (status.HeatingCoolingMode)
        {
        case Status::HpMode::HEAT_FLOW_TEMP:
        case Status::HpMode::HEAT_ROOM_TEMP:
            flag = 1;
            break;
        case Status::HpMode::COOL_ROOM_TEMP:
            flag = 3;
            break;
        case Status::HpMode::COOL_FLOW_TEMP:
            flag = 4;
            break;
        default:
            flag = 0;
            break;
        }

        if (zone == SetZone::ZONE_1) {
            cmd[1] = 0x80;
            cmd[6] = flag;
            cmd.set_float16(newTemp, 10);
        }
        else if (zone == SetZone::ZONE_2) {
            cmd[2] = 0x02;
            cmd[7] = flag;
            cmd.set_float16(newTemp, 12);
        }
        
        // ESP_LOGW(TAG, cmd.debug_dump_packet().c_str());
        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_dhw_target_temperature(float newTemp)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_DHW_TEMPERATURE;
        cmd.set_float16(newTemp, 8);

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_dhw_mode(Status::DhwMode dhwMode)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = SET_SETTINGS_FLAG_DHW_MODE;
        cmd[5] = static_cast<uint8_t>(dhwMode);

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_dhw_force(bool on)
    {   
        Message cmd{MsgType::SET_CMD, SetType::CONTROLLER_SETTING};
        cmd[1] = SET_SETTINGS_FLAG_MODE_TOGGLE;
        cmd[3] = on ? 1 : 0; // bit[3] of payload is DHW force, bit[2] is Holiday mode.

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_holiday(bool on)
    {   
        Message cmd{MsgType::SET_CMD, SetType::CONTROLLER_SETTING};
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

    void EcodanHeatpump::set_hp_mode(uint8_t mode, esphome::ecodan::SetZone zone)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};

        if (zone == SetZone::ZONE_1) {
            cmd[1] = SET_SETTINGS_FLAG_HP_MODE_ZONE1;
            cmd[6] = mode;
        }
        else if (zone == SetZone::ZONE_2) {
            cmd[1] = SET_SETTINGS_FLAG_HP_MODE_ZONE2;
            cmd[7] = mode;
        }
        else {
            cmd[1] = SET_SETTINGS_FLAG_HP_MODE_ZONE1 | SET_SETTINGS_FLAG_HP_MODE_ZONE2;
            cmd[6] = mode;
            cmd[7] = mode;
        }

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_controller_mode(CONTROLLER_FLAG flag, bool on)
    {
        Message cmd{MsgType::SET_CMD, SetType::CONTROLLER_SETTING};
        cmd[1] = static_cast<uint8_t>(flag);
        uint8_t value = on ? 1 : 0;

        if ((flag & CONTROLLER_FLAG::FORCED_DHW) == CONTROLLER_FLAG::FORCED_DHW)
            cmd[3] = value;

        if ((flag & CONTROLLER_FLAG::HOLIDAY_MODE) == CONTROLLER_FLAG::HOLIDAY_MODE)
            cmd[4] = value;

        if ((flag & CONTROLLER_FLAG::PROHIBIT_DHW) == CONTROLLER_FLAG::PROHIBIT_DHW)
            cmd[5] = value;

        if ((flag & CONTROLLER_FLAG::PROHIBIT_Z1_HEATING) == CONTROLLER_FLAG::PROHIBIT_Z1_HEATING)
            cmd[6] = value;

        if ((flag & CONTROLLER_FLAG::PROHIBIT_Z1_COOLING) == CONTROLLER_FLAG::PROHIBIT_Z1_COOLING)
            cmd[7] = value;

        if ((flag & CONTROLLER_FLAG::PROHIBIT_Z2_HEATING) == CONTROLLER_FLAG::PROHIBIT_Z2_HEATING)
            cmd[8] = value;

        if ((flag & CONTROLLER_FLAG::PROHIBIT_Z2_COOLING) == CONTROLLER_FLAG::PROHIBIT_Z2_COOLING)
            cmd[9] = value;

        if ((flag & CONTROLLER_FLAG::SERVER_CONTROL) == CONTROLLER_FLAG::SERVER_CONTROL)
            cmd[10] = value;

        //ESP_LOGW(TAG, cmd.debug_dump_packet().c_str());
        schedule_cmd(cmd);
    }

    bool EcodanHeatpump::schedule_cmd(Message& cmd)
    {   
        cmdQueue.emplace(std::move(cmd));
        return dispatch_next_cmd();
    }

    #define MAX_STATUS_CMD_SIZE 21
    Message statusCmdQueue[MAX_STATUS_CMD_SIZE] = {
        Message{MsgType::GET_CMD, GetType::DATETIME_FIRMWARE},
        Message{MsgType::GET_CMD, GetType::DEFROST_STATE},
        Message{MsgType::GET_CMD, GetType::ERROR_STATE},
        Message{MsgType::GET_CMD, GetType::COMPRESSOR_FREQUENCY},
        Message{MsgType::GET_CMD, GetType::DHW_STATE},
        Message{MsgType::GET_CMD, GetType::HEATING_POWER},
        Message{MsgType::GET_CMD, GetType::TEMPERATURE_CONFIG},
        Message{MsgType::GET_CMD, GetType::SH_TEMPERATURE_STATE},
        Message{MsgType::GET_CMD, GetType::TEMPERATURE_STATE_A},
        Message{MsgType::GET_CMD, GetType::TEMPERATURE_STATE_B},
        Message{MsgType::GET_CMD, GetType::TEMPERATURE_STATE_C},
        Message{MsgType::GET_CMD, GetType::TEMPERATURE_STATE_D},
        Message{MsgType::GET_CMD, GetType::EXTERNAL_STATE},
        Message{MsgType::GET_CMD, GetType::ACTIVE_TIME},
        Message{MsgType::GET_CMD, GetType::PUMP_STATUS},
        Message{MsgType::GET_CMD, GetType::FLOW_RATE},
        Message{MsgType::GET_CMD, GetType::MODE_FLAGS_A},
        Message{MsgType::GET_CMD, GetType::MODE_FLAGS_B},
        Message{MsgType::GET_CMD, GetType::ENERGY_USAGE},
        Message{MsgType::GET_CMD, GetType::ENERGY_DELIVERY},
        Message{MsgType::GET_CONFIGURATION, GetType::HARDWARE_CONFIGURATION}
    };

    bool EcodanHeatpump::dispatch_next_status_cmd()
    {
        if (proxy_available())
            return true;
        
        auto static cmdIndex = 0;
        Message& cmd = statusCmdQueue[cmdIndex];
        cmdIndex = (cmdIndex + 1) % MAX_STATUS_CMD_SIZE;

        if (!serial_tx(uart_, cmd))
        {
            ESP_LOGI(TAG, "Unable to dispatch status update request, flushing queued requests...");
            cmdIndex = 0;
            connected = false;
            return false;
        }

        return true;
    }

    bool EcodanHeatpump::dispatch_next_cmd()
    {
        if (cmdQueue.empty())
        {
            return true;
        }
        
        //ESP_LOGI(TAG, msg.debug_dump_packet().c_str());

        if (!serial_tx(uart_, cmdQueue.front()))
        {
            ESP_LOGI(TAG, "Unable to dispatch status update request, flushing queued requests...");
            connected = false;
            return false;
        }

        return true;
    }

    bool EcodanHeatpump::begin_connect()
    {
        Message cmd{MsgType::CONNECT_CMD};
        uint8_t payload[3] = {0xCA, 0x01};
        cmd.write_payload(payload, sizeof(payload));

        ESP_LOGI(TAG, "Attempt to tx CONNECT_CMD!");
        if (!serial_tx(uart_, cmd))
        {
            ESP_LOGI(TAG, "Failed to tx CONNECT_CMD!");
            return false;
        }

        return true;
    }

}}
