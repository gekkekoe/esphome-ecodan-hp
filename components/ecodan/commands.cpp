#include "ecodan.h"

#include "esphome.h"

#include <functional>
#include <queue>

namespace esphome {
namespace ecodan 
{
    void EcodanHeatpump::set_room_temperature(float newTemp, esphome::ecodan::Zone zone)
    {
        Message cmd{MsgType::SET_CMD, SetType::ROOM_SETTINGS};

        if (zone == Zone::ZONE_1) {
            cmd[1] = 0x02;
            cmd.set_float16(newTemp, 4);
        }
        else if (zone == Zone::ZONE_2) {
            cmd[1] = 0x08;
            cmd.set_float16(newTemp, 6);
        }

        cmd[3]= status.HeatingCoolingMode == Status::HpMode::COOL_ROOM_TEMP || status.HeatingCoolingMode == Status::HpMode::COOL_FLOW_TEMP 
            ? 1 : 0;

        // cmd[1] = SET_SETTINGS_FLAG_ZONE_TEMPERATURE;


        // auto flag = status.HeatingCoolingMode == Status::HpMode::COOL_ROOM_TEMP
        //     ? static_cast<uint8_t>(Status::HpMode::COOL_ROOM_TEMP) : static_cast<uint8_t>(Status::HpMode::HEAT_ROOM_TEMP);

        // if (zone == Zone::BOTH || zone == Zone::ZONE_1) {
        //     cmd[6] = flag;
        //     cmd.set_float16(newTemp, 10);
        // }
            
        // if (zone == Zone::BOTH || zone == Zone::ZONE_2) {
        //     cmd[7] = flag;
        //     cmd.set_float16(newTemp, 12);
        // }
        //ESP_LOGE(TAG, cmd.debug_dump_packet().c_str());

        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_flow_target_temperature(float newTemp, esphome::ecodan::Zone zone)
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

        if (zone == Zone::ZONE_1) {
            cmd[1] = 0x80;
            cmd[6] = flag;
            cmd.set_float16(newTemp, 10);
        }
        else if (zone == Zone::ZONE_2) {
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

    void EcodanHeatpump::set_hp_mode(uint8_t mode, esphome::ecodan::Zone zone)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};

        if (zone == Zone::ZONE_1) {
            cmd[1] = SET_SETTINGS_FLAG_HP_MODE_ZONE1;
            cmd[6] = mode;
        }
        else if (zone == Zone::ZONE_2) {
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
        cmd[2] = 0;

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

        if ((flag & CONTROLLER_FLAG::SERVER_CONTROL) == CONTROLLER_FLAG::SERVER_CONTROL) {
            // when we enter SCM, we need to explicitly set all the prohibit flags (0xFC as flag)
            cmd[1] = static_cast<uint8_t>(flag | CONTROLLER_FLAG::PROHIBIT_DHW | CONTROLLER_FLAG::PROHIBIT_Z1_HEATING | CONTROLLER_FLAG::PROHIBIT_Z1_COOLING | CONTROLLER_FLAG::PROHIBIT_Z2_HEATING | CONTROLLER_FLAG::PROHIBIT_Z2_COOLING);
            cmd[10] = value;
        }

        //ESP_LOGW(TAG, cmd.debug_dump_packet().c_str());
        schedule_cmd(cmd);
    }

    void EcodanHeatpump::set_mrc_mode(Status::MRC_FLAG flag)
    {
        Message cmd{MsgType::SET_CMD, SetType::BASIC_SETTINGS};
        cmd[1] = 0;
        cmd[2] = 0x08; // MRC prohibit flag

        cmd[14] = static_cast<uint8_t>(flag);

        //ESP_LOGE(TAG, cmd.debug_dump_packet().c_str());
        schedule_cmd(cmd);
    }

    bool EcodanHeatpump::schedule_cmd(Message& cmd)
    {   
        cmdQueue.emplace(QueuedCommand{std::move(cmd), 0, 0});
        return dispatch_next_cmd();
    }

    #define MAX_INITIAL_CMD_SIZE 2
    Message initialCmdQueue[MAX_INITIAL_CMD_SIZE] = {
        Message{MsgType::GET_CONFIGURATION, GetType::HARDWARE_CONFIGURATION},
        Message{MsgType::GET_CMD, GetType::DIP_SWITCHES}
    };

    #define MAX_STATUS_CMD_SIZE 20
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
        Message{MsgType::GET_CMD, GetType::ENERGY_DELIVERY}
    };

    struct ServiceCodeRuntime {
        Status::REQUEST_CODE Request;
        bool IsOutdoorExtendedThermistor{false};
        uint32_t SecondsBetweenCalls{0};
        std::chrono::time_point<std::chrono::steady_clock> LastAttempt{};
    };

    #define MAX_SERVICE_CODE_CMD_SIZE 5
    ServiceCodeRuntime serviceCodeCmdQueue[MAX_SERVICE_CODE_CMD_SIZE] = {
        ServiceCodeRuntime{Status::REQUEST_CODE::COMPRESSOR_STARTS, false, 30*60, std::chrono::steady_clock::now() - std::chrono::seconds(60*60)},
        ServiceCodeRuntime{Status::REQUEST_CODE::TH4_DISCHARGE_TEMP, true, 0, std::chrono::steady_clock::time_point{}},
        ServiceCodeRuntime{Status::REQUEST_CODE::TH3_LIQUID_PIPE1_TEMP, true, 0, std::chrono::steady_clock::time_point{}},
        ServiceCodeRuntime{Status::REQUEST_CODE::TH6_2_PHASE_PIPE_TEMP, true, 0, std::chrono::steady_clock::time_point{}},
        //ServiceCodeRuntime{Status::REQUEST_CODE::TH32_SUCTION_PIPE_TEMP, true, 0, std::chrono::steady_clock::time_point{}},
        //ServiceCodeRuntime{Status::REQUEST_CODE::TH8_HEAT_SINK_TEMP, true, 0, std::chrono::steady_clock::time_point{}},
        //ServiceCodeRuntime{Status::REQUEST_CODE::DISCHARGE_SUPERHEAT, true, 0, std::chrono::steady_clock::time_point{}},
        //ServiceCodeRuntime{Status::REQUEST_CODE::SUB_COOL, true, 0, std::chrono::steady_clock::time_point{}},
        ServiceCodeRuntime{Status::REQUEST_CODE::FAN_SPEED, false, 0, std::chrono::steady_clock::time_point{}}
    };

    bool EcodanHeatpump::dispatch_next_status_cmd()
    {
        if (proxy_available() || !handle_active_request_codes())
            return true;
        
        auto static cmdIndex = 0;
        auto static serviceCodeCmdIndex = 0;
        auto static loopIndex = 0;

        loopIndex = (loopIndex + 1) % MAX_STATUS_CMD_SIZE;

        // only execute when we have sensors and a ftc version is known, since ftc7 gets a lot for free
        if (hasRequestCodeSensors && requestCodesEnabled && initialCmdCompleted() && loopIndex == 0) {
            int counter = 0;
            while (counter <= MAX_SERVICE_CODE_CMD_SIZE && activeRequestCode == Status::REQUEST_CODE::NONE) {
                counter++;
                serviceCodeCmdIndex = (serviceCodeCmdIndex + 1) % MAX_SERVICE_CODE_CMD_SIZE;
                auto& request = serviceCodeCmdQueue[serviceCodeCmdIndex];

                // check if svc code should be executed
                if (status.ReportsExtendedOutdoorUnitThermistors && request.IsOutdoorExtendedThermistor)
                    continue;

                if (request.SecondsBetweenCalls > 0) {
                    auto allow_run = std::chrono::steady_clock::now() - request.LastAttempt > std::chrono::seconds(request.SecondsBetweenCalls);
                    if (!allow_run) // skip and handle next service call 
                        continue;

                    serviceCodeCmdQueue[serviceCodeCmdIndex].LastAttempt = std::chrono::steady_clock::now();
                } 
                activeRequestCode = serviceCodeCmdQueue[serviceCodeCmdIndex].Request;
            }
            //ESP_LOGE(TAG, "Active svc: %d", static_cast<int16_t>(activeRequestCode));
            return true;
        }

        cmdIndex = (cmdIndex + 1) % (!initialCmdCompleted() ? MAX_INITIAL_CMD_SIZE : MAX_STATUS_CMD_SIZE);
        auto& cmd = !initialCmdCompleted() ? initialCmdQueue[cmdIndex] : statusCmdQueue[cmdIndex];
        if (!serial_tx(cmd))
        {
            ESP_LOGI(TAG, "Unable to dispatch status update request, flushing queued requests...");
            cmdIndex = 0;
            serviceCodeCmdIndex = 0;
            reset_connection();
            return false;
        }
        return true;
    }

    bool EcodanHeatpump::handle_active_request_codes() {

        const unsigned long REQUEST_RETRY_INTERVAL = 1*1000; 
        static unsigned long last_svc_request_time = 0;

        if (activeRequestCode != Status::REQUEST_CODE::NONE) {
            unsigned long current_time = millis();
            if (current_time - last_svc_request_time >= REQUEST_RETRY_INTERVAL) {
                ESP_LOGD(TAG, "Sending active service request (code %d)...", activeRequestCode);
                Message svc_cmd{MsgType::GET_CMD, GetType::SERVICE_REQUEST_CODE, static_cast<int16_t>(activeRequestCode)};
                if (!serial_tx(svc_cmd))
                {
                    ESP_LOGI(TAG, "Unable to dispatch status update request, flushing queued requests...");
                    reset_connection();
                    return false;
                }
                last_svc_request_time = current_time;
            }

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
        
        QueuedCommand& pending_cmd = cmdQueue.front();
        const unsigned long CMD_TIMEOUT_MS = 2*1000;
        if (pending_cmd.last_sent_time != 0 && (millis() - pending_cmd.last_sent_time < CMD_TIMEOUT_MS)) {
            return true;
        }
        if (pending_cmd.retries >= 10) {
            ESP_LOGE(TAG, "Command failed after 10 retries. Discarding.");
            cmdQueue.pop();
            return true;
        }

        if (pending_cmd.last_sent_time != 0) {
            ESP_LOGW(TAG, "Command timed out. Retrying (attempt %d/10)...", pending_cmd.retries + 1);
        }

        if (!serial_tx(pending_cmd.message)) {
            ESP_LOGE(TAG, "Failed to send command to serial.");
            reset_connection();
            return false;
        }

        pending_cmd.retries++;
        pending_cmd.last_sent_time = millis();
        
        return true;
    }

    bool EcodanHeatpump::begin_connect()
    {
        if (proxy_available())
            return true;

        Message cmd{MsgType::CONNECT_CMD};
        uint8_t payload[2] = {0xCA, 0x01};
        cmd.write_payload(payload, sizeof(payload));

        ESP_LOGI(TAG, "Attempt to tx CONNECT_CMD!");
        if (!serial_tx(cmd))
        {
            ESP_LOGI(TAG, "Failed to tx CONNECT_CMD!");
            return false;
        }

        return true;
    }

    bool EcodanHeatpump::disconnect()
    {
        if (proxy_available())
            return true;

        Message cmd{MsgType::CONNECT_CMD};
        uint8_t payload[2] = {0xCA, 0x02};
        cmd.write_payload(payload, sizeof(payload));

        ESP_LOGI(TAG, "Attempt to tx DISCONNECT_CMD!");
        if (!serial_tx(cmd))
        {
            ESP_LOGI(TAG, "Failed to tx DISCONNECT_CMD!");
            return false;
        }

        return true;
    }

}}
