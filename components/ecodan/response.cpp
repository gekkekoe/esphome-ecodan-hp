#include "ecodan.h"
#include <string>
#include <algorithm>

namespace esphome {
namespace ecodan 
{

    #define MAX_FIRST_LETTER_SIZE 8
    char FaultCodeFirstChar[MAX_FIRST_LETTER_SIZE] = {
        'A', 'b', 'E', 'F', 'J', 'L', 'P', 'U'
    };

    #define MAX_SECOND_LETTER_SIZE 21
    char FaultCodeSecondChar[MAX_SECOND_LETTER_SIZE] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F', 'O', 'H', 'J', 'L', 'P', 'U'
    };

    std::string decode_error(uint8_t first, uint8_t second, uint16_t code) {
        std::string fault_code = std::string("No error");
        if (code == 0x6999) {
            fault_code = std::string("Bad communication with unit");
        } else if (code != 0x8000) {
            uint16_t translated_code = (code >> 8) * 100 + (code & 0xff);
            char result[256];
            snprintf(result, 256, "%c%c %u", 
                    FaultCodeFirstChar[std::max<u_int8_t>(0, first % MAX_FIRST_LETTER_SIZE)], 
                    FaultCodeSecondChar[std::max<u_int8_t>(0, second - 1) % MAX_SECOND_LETTER_SIZE], 
                    translated_code);
            fault_code = std::string(result);
        }

        return fault_code;
    }

    void EcodanHeatpump::handle_get_response(Message& res)
    {
        switch (res.payload_type<GetType>())
        {
        case GetType::SERVICE_REQUEST_CODE:
            if (res[3] != 0 && (activeRequestCode != Status::REQUEST_CODE::NONE || proxy_available())) {
                auto res_request_code = static_cast<Status::REQUEST_CODE>(res.get_int16(1));
                if (activeRequestCode == res_request_code) {
                    activeRequestCode = Status::REQUEST_CODE::NONE;
                }

                if (res[3] == 2 || res[3] == 1) {
                    switch(res_request_code) {
                        case Status::REQUEST_CODE::COMPRESSOR_STARTS:
                            status.RcCompressorStarts = res.get_uint16_v2(4) * 100;
                            publish_state("compressor_starts", static_cast<float>(status.RcCompressorStarts));
                        break;
                        case Status::REQUEST_CODE::TH4_DISCHARGE_TEMP:
                            status.RcDischargeTemp = res.get_int16_v2(4);
                            publish_state("discharge_temp", status.RcDischargeTemp);
                        break;
                        case Status::REQUEST_CODE::TH3_LIQUID_PIPE1_TEMP:
                            status.RcOuLiquidPipeTemp = res.get_int16_v2(4);
                            publish_state("ou_liquid_pipe_temp", status.RcOuLiquidPipeTemp); 
                        break;
                        case Status::REQUEST_CODE::TH6_2_PHASE_PIPE_TEMP:
                            status.RcOuTwoPhasePipeTemp = res.get_int16_v2(4);
                            publish_state("ou_two_phase_pipe_temp", status.RcOuTwoPhasePipeTemp); 
                        break;
                        case Status::REQUEST_CODE::TH32_SUCTION_PIPE_TEMP:
                            status.RcOuSuctionPipeTemp = res.get_int16_v2(4);
                            publish_state("ou_suction_pipe_temp", status.RcOuSuctionPipeTemp); 
                        break;
                        case Status::REQUEST_CODE::TH8_HEAT_SINK_TEMP:
                            status.RcOuHeatSinkTemp = res.get_int16_v2(4);
                            publish_state("ou_heatsink_temp", status.RcOuHeatSinkTemp); 
                        break;
                        case Status::REQUEST_CODE::TH33_SURFACE_TEMP:
                            status.RcOuCompressorSurfaceTemp = res.get_int16_v2(4);
                            publish_state("ou_compressor_surface_temp", status.RcOuCompressorSurfaceTemp); 
                        break;
                        case Status::REQUEST_CODE::DISCHARGE_SUPERHEAT:
                            status.RcDischargeSuperHeatTemp = res.get_int16_v2(4);
                            publish_state("super_heat_temp", status.RcDischargeSuperHeatTemp);
                        break;
                        case Status::REQUEST_CODE::SUB_COOL:
                            status.RcSubCoolTemp = res.get_int16_v2(4);
                            publish_state("sub_cool_temp", status.RcSubCoolTemp);
                        break;
                        case Status::REQUEST_CODE::FAN_SPEED:
                            status.RcFanSpeedRpm = res.get_int16_v2(4);
                            publish_state("fan_speed", static_cast<float>(status.RcFanSpeedRpm));
                        break;

                        default:
                        break;
                    }
                }
            }
            break;
        case GetType::DATETIME_FIRMWARE:
            {
                status.ControllerDateTime.tm_year = 100 + res[1];
                status.ControllerDateTime.tm_mon = res[2] - 1;
                status.ControllerDateTime.tm_mday = res[3];
                status.ControllerDateTime.tm_hour = res[4];
                status.ControllerDateTime.tm_min = res[5];
                status.ControllerDateTime.tm_sec = res[6];                    
                status.ControllerDateTime.tm_isdst = -1; 
                mktime(&status.ControllerDateTime);

                char firmware[6];
                snprintf(firmware, 6, "%02X.%02X", res[7], res[8]);
                publish_state("controller_firmware_text", std::string(firmware));
            }
            break;               
        case GetType::DEFROST_STATE:
            status.MasterZone1 = res[1];
            status.MasterZone2 = res[2];
            status.DefrostActive = res[3] != 0;
            publish_state("status_defrost", status.DefrostActive);
            break;
        case GetType::ERROR_STATE:
            // 1 = refrigerant error code
            // 2+3 = fault code, [2]*100+[3]
            // 4+5 = fault code: letter 2, 0x00 0x03 = A3
            status.RefrigerantErrorCode = res[1];
            status.FaultCodeNumeric = res.get_u16(2);
            status.FaultCodeLetters = res.get_u16(4);
            status.MultiZoneStatus = res[8];

            publish_state("refrigerant_error_code", static_cast<float>(status.RefrigerantErrorCode));
            publish_state("fault_code_text", decode_error(res[4], res[5], status.FaultCodeNumeric));
            publish_state("status_multi_zone", static_cast<float>(status.MultiZoneStatus));
            break;
        case GetType::COMPRESSOR_FREQUENCY:
            status.CompressorFrequency = res[1];
            // 0 = normal, 1 = min, 2 = max 
            // status.CompressorOperatingLevel = res[8];
            publish_state("compressor_frequency", static_cast<float>(status.CompressorFrequency));
            break;
        case GetType::DHW_STATE:
            // 6 = heat source , 0x0 = heatpump, 0x1 = screw in heater, 0x2 = electric heater..
            status.HeatSource = res[6];
            // 0 = normal, 1 = hp phase, 2 = heater phase
            //status.DhwStage = res[7];
            publish_state("heat_source", static_cast<float>(status.HeatSource));
            break;
        case GetType::HEATING_POWER:
            status.OutputPower = res[6];
            status.EnergyConsumedIncreasing = res.get_u16(11) / 10.0f;
            //status.BoosterActive = res[4] == 2;
            publish_state("output_power", static_cast<float>(status.OutputPower));
            publish_state("energy_consumed_increasing", status.EnergyConsumedIncreasing);
            //publish_state("status_booster", status.BoosterActive ? "On" : "Off");
            break;
        case GetType::TEMPERATURE_CONFIG:
            status.Zone1SetTemperature = res.get_float16(1);
            status.Zone2SetTemperature = res.get_float16(3);
            status.Zone1FlowTemperatureSetPoint = res.get_float16(5);
            status.Zone2FlowTemperatureSetPoint = res.get_float16(7);
            status.LegionellaPreventionSetPoint = res.get_float16(9);
            status.DhwTemperatureDrop = res.get_float8_v2(11);
            status.MaximumFlowTemperature = res.get_float8_v2(12);
            status.MinimumFlowTemperature = res.get_float8_v2(13);

            publish_state("z1_room_temp_target", status.Zone1SetTemperature);
            publish_state("z2_room_temp_target", status.Zone2SetTemperature);
            publish_state("z1_flow_temp_target", status.Zone1FlowTemperatureSetPoint);
            publish_state("z2_flow_temp_target", status.Zone2FlowTemperatureSetPoint);

            publish_state("legionella_prevention_temp", status.LegionellaPreventionSetPoint);
            publish_state("dhw_flow_temp_drop", status.DhwTemperatureDrop);
            //ESP_LOGW(TAG, res.debug_dump_packet().c_str());
            // min/max flow

            break;
        case GetType::SH_TEMPERATURE_STATE:
            // 0xF0 seems to be a sentinel value for "not reported in the current system"
            status.Zone1RoomTemperature = res[1] != 0xF0 ? res.get_float16(1) : 0.0f;
            status.Zone2RoomTemperature = res[3] != 0xF0 ? res.get_float16(3) : 0.0f;
            status.OutsideTemperature = res.get_float8(11);
            status.HpRefrigerantLiquidTemperature = res.get_float16_signed(8);

            publish_state("z1_room_temp", status.Zone1RoomTemperature);
            publish_state("z2_room_temp", status.Zone2RoomTemperature);
            publish_state("outside_temp", status.OutsideTemperature);
            publish_state("hp_refrigerant_temp", status.HpRefrigerantLiquidTemperature);
            //ESP_LOGE(TAG, "0x0b offset 10: \t%f (v1), \t%f (v2), \t%f (v3)", res.get_float8(10), res.get_float8_v2(10), res.get_float8_v3(10));
            break;
        case GetType::TEMPERATURE_STATE_A:
            status.HpFeedTemperature = res.get_float16(1);
            status.HpReturnTemperature = res.get_float16(4);
            status.DhwTemperature = res.get_float16(7);
            status.DhwSecondaryTemperature = res.get_float16(10); 

            publish_state("hp_feed_temp", status.HpFeedTemperature);
            publish_state("hp_return_temp", status.HpReturnTemperature);
            publish_state("dhw_temp", status.DhwTemperature);
            publish_state("dhw_secondary_temp", status.DhwSecondaryTemperature);
            status.update_output_power_estimation(specificHeatConstantOverride);
            publish_state("computed_output_power", status.ComputedOutputPower);
            break;
        case GetType::TEMPERATURE_STATE_B:
            status.Z1FeedTemperature = res.get_float16(1);
            status.Z1ReturnTemperature = res.get_float16(4);

            status.Z2FeedTemperature = res.get_float16(7);
            status.Z2ReturnTemperature = res.get_float16(10);

            publish_state("z1_feed_temp", status.Z1FeedTemperature);
            publish_state("z1_return_temp", status.Z1ReturnTemperature);
            publish_state("z2_feed_temp", status.Z2FeedTemperature);
            publish_state("z2_return_temp", status.Z2ReturnTemperature);                                  
            break;
        case GetType::TEMPERATURE_STATE_C:
            status.BoilerFlowTemperature = res.get_float16(1);
            status.BoilerReturnTemperature = res.get_float16(4);
            publish_state("boiler_flow_temp", status.BoilerFlowTemperature);
            publish_state("boiler_return_temp", status.BoilerReturnTemperature);   
            break;
        case GetType::TEMPERATURE_STATE_D:
            status.MixingTankTemperature = res.get_float16(1);
            if (res[4] == 0x0f && res[5] == 0xd9) {
                // stuck at 40.57 -> read condensing temp at byte 6
                status.HpRefrigerantCondensingTemperature = res.get_float8_v3(6);
            }
            else {
                status.HpRefrigerantCondensingTemperature = res.get_float16_signed(4);
            }
            publish_state("mixing_tank_temp", status.MixingTankTemperature);
            publish_state("hp_refrigerant_condensing_temp", status.HpRefrigerantCondensingTemperature);
            // static float last_temp = 0.0f;
            // if (last_temp != status.HpRefrigerantCondensingTemperature) {
            //     last_temp = status.HpRefrigerantCondensingTemperature;
            //     ESP_LOGW(TAG, "0x0f offset 4+5: \t0x%02X = %f, offset 6: \t%f", res[6], res.get_float16_signed(4), res.get_float8_v3(6));
            // }
            //ESP_LOGW(TAG, res.debug_dump_packet().c_str());
 
            // detect if unit reports extended 0x0f messages
            if (!status.ReportsExtendedOutdoorUnitThermistors) {
                for (auto i = 7; i < 15; i++) {
                    if (res[i] != 0) {
                        status.ReportsExtendedOutdoorUnitThermistors = true;
                        break;
                    }
                }
            } else {
                status.RcDischargeTemp = static_cast<uint8_t>(res[7]);
                publish_state("discharge_temp", status.RcDischargeTemp);

                status.RcOuLiquidPipeTemp = res.get_float8(8, 39.0f);
                publish_state("ou_liquid_pipe_temp", status.RcOuLiquidPipeTemp); 

                status.RcOuTwoPhasePipeTemp = res.get_float8(9, 39.0f);
                publish_state("ou_two_phase_pipe_temp", status.RcOuTwoPhasePipeTemp); 

                status.RcOuSuctionPipeTemp = res.get_float8(10, 39.0f);
                publish_state("ou_suction_pipe_temp", status.RcOuSuctionPipeTemp); 

                status.RcOuHeatSinkTemp = static_cast<uint8_t>(res[11]) - 40.0f;
                publish_state("ou_heatsink_temp", status.RcOuHeatSinkTemp); 

                status.RcOuCompressorSurfaceTemp = static_cast<uint8_t>(res[12]) - 40.0f;
                publish_state("ou_compressor_surface_temp", status.RcOuCompressorSurfaceTemp); 
                
                status.RcDischargeSuperHeatTemp = static_cast<uint8_t>(res[13]);
                publish_state("super_heat_temp", status.RcDischargeSuperHeatTemp);

                status.RcSubCoolTemp =  res.get_float8(14, 39.0f);
                publish_state("sub_cool_temp", status.RcSubCoolTemp);
            }
            break;  
        case GetType::EXTERNAL_STATE:
            // 1 = IN1 Thermostat heat/cool request
            // 2 = IN6 Thermostat 2
            // 3 = IN5 outdoor thermostat
            status.In1ThermostatRequest = res[1] != 0;
            status.In6ThermostatRequest = res[2] != 0;
            status.In5ThermostatRequest = res[3] != 0;
            publish_state("status_in1_request", status.In1ThermostatRequest);
            publish_state("status_in6_request", status.In6ThermostatRequest);
            publish_state("status_in5_request", status.In5ThermostatRequest);
            break;
        case GetType::ACTIVE_TIME:
            status.Runtime = res.get_float24_v2(3);
            status.CompressorOn = res[1] != 0;
            publish_state("runtime", status.Runtime);
            publish_state("status_compressor", status.CompressorOn);
            //ESP_LOGI(TAG, res.debug_dump_packet().c_str());
            break;
        case GetType::PUMP_STATUS_A:
        {
            // byte 1 = pump running on/off
            // byte 4 = pump 2
            // byte 5 = pump 3
            // byte 6 = 3 way valve on/off
            // byte 7 - 3 way valve 2   
            // byte 10 - Mixing valve step         
            // byte 11 - Mixing valve status
            status.WaterPumpActive = res[1] != 0;
            status.PumpPWM = res[2];
            status.PumpFeedback = res[3];
            status.ThreeWayValveActive = res[6] != 0;
            status.WaterPump2Active = res[4] != 0;
            status.ThreeWayValve2Active = res[7] != 0;         
            status.WaterPump3Active = res[5] != 0;       
            status.MixingValveStep = res[10];   
            status.MixingValveStatus = res[11];   
            float mapped_pump_speed = 0.0f;
            switch (status.PumpPWM) {
                case 52:
                    mapped_pump_speed = 1;
                    break;
                case 41:
                    mapped_pump_speed = 2;
                    break;
                case 31:
                    mapped_pump_speed = 3;
                    break;
                case 20:
                    mapped_pump_speed = 4;
                    break;
                case 0:
                    mapped_pump_speed = 5;
                    break;
                case 100:
                default:
                    mapped_pump_speed = 0;
                break;
            }
            publish_state("pump_speed", static_cast<float>(mapped_pump_speed));
            publish_state("pump_feedback", static_cast<float>((status.PumpFeedback == 100 | status.PumpFeedback == 255) ? 0 : status.PumpFeedback));
            publish_state("status_water_pump", status.WaterPumpActive);
            publish_state("status_three_way_valve", status.ThreeWayValveActive);
            publish_state("status_water_pump_2", status.WaterPump2Active);
            publish_state("status_three_way_valve_2", status.ThreeWayValve2Active);
            publish_state("status_water_pump_3", status.WaterPump3Active);
            publish_state("status_mixing_valve", static_cast<float>(status.MixingValveStatus));
            publish_state("mixing_valve_step", static_cast<float>(status.MixingValveStep));
            //ESP_LOGI(TAG, res.debug_dump_packet().c_str());
        }
            break;
        case GetType::PUMP_STATUS_B:
        {   
            // byte 8 - Z1  Mixing valve step
            status.MixingValveStep = res[8];   
            publish_state("mixing_valve_step_z1", static_cast<float>(status.MixingValveStepZ1));
            //ESP_LOGI(TAG, res.debug_dump_packet().c_str());
        }
            break;              
        case GetType::FLOW_RATE:
            // booster = 2, 
            // emmersion = 5
            status.BoosterActive = res[2] != 0;
            status.Booster2Active = res[3] != 0;
            status.ImmersionActive = res[5] != 0;
            status.FlowRate = res[12];
            publish_state("flow_rate", static_cast<float>(status.FlowRate));
            publish_state("status_booster", status.BoosterActive);
            publish_state("status_booster_2", status.Booster2Active);
            publish_state("status_immersion", status.ImmersionActive);
            status.update_output_power_estimation(specificHeatConstantOverride);
            publish_state("computed_output_power", status.ComputedOutputPower);
            break;
        case GetType::MODE_FLAGS_A:
            //ESP_LOGE(TAG, res.debug_dump_packet().c_str());
            status.set_power_mode(res[3]);
            status.set_operation_mode(res[4]);
            status.set_dhw_mode(res[5]);

            status.MRCFlag = static_cast<Status::MRC_FLAG>(res[14]);
            status.HeatingCoolingMode = static_cast<Status::HpMode>(res[6]);
            status.HeatingCoolingModeZone2 = static_cast<Status::HpMode>(res[7]);
            status.DhwFlowTemperatureSetPoint = res.get_float16(8);
            //status.RadiatorFlowTemperatureSetPoint = res.get_float16(12);

            publish_state("status_power", status.Power == Status::PowerMode::ON);
            publish_state("status_dhw_eco", status.HotWaterMode == Status::DhwMode::ECO);
            publish_state("status_operation", static_cast<float>(status.Operation));
            // publish numeric operation mode for callbacks
            publish_state("operation_mode", static_cast<float>(status.Operation));

            publish_state("dhw_flow_temp_target", status.DhwFlowTemperatureSetPoint);
            //publish_state("sh_flow_temp_target", status.RadiatorFlowTemperatureSetPoint);
            break;
        case GetType::MODE_FLAGS_B:
            status.DhwForcedActive = res[3] != 0;
            status.HolidayMode = res[4] != 0;
            status.ProhibitDhw = res[5] != 0;
            status.ProhibitHeatingZ1 = res[6] != 0;
            status.ProhibitCoolingZ1 = res[7] != 0;
            status.ProhibitHeatingZ2 = res[8] != 0;
            status.ProhibitCoolingZ2 = res[9] != 0;
            
            publish_state("status_dhw_forced", status.DhwForcedActive);
            publish_state("status_holiday", status.HolidayMode);
            publish_state("status_prohibit_dhw", status.ProhibitDhw);
            publish_state("status_prohibit_heating_z1", status.ProhibitHeatingZ1);
            publish_state("status_prohibit_cool_z1", status.ProhibitCoolingZ1);
            publish_state("status_prohibit_heating_z2", status.ProhibitHeatingZ2);
            publish_state("status_prohibit_cool_z2", status.ProhibitCoolingZ2);

            status.ServerControl = res[10] != 0;
            publish_state("status_server_control", status.ServerControl);

            // set status for svc switches
            publish_state("status_server_control_prohibit_dhw", status.ServerControl ? status.ProhibitDhw : false);
            publish_state("status_server_control_prohibit_heating_z1", status.ServerControl ? status.ProhibitHeatingZ1 : false);
            publish_state("status_server_control_prohibit_cool_z1", status.ServerControl ? status.ProhibitCoolingZ1 : false);
            publish_state("status_server_control_prohibit_heating_z2", status.ServerControl ? status.ProhibitHeatingZ2 : false);
            publish_state("status_server_control_prohibit_cool_z2", status.ServerControl ? status.ProhibitCoolingZ2 : false);
            break;
        case GetType::ENERGY_USAGE:
            status.EnergyConsumedHeating = res.get_float24(4);
            status.EnergyConsumedCooling = res.get_float24(7);
            status.EnergyConsumedDhw = res.get_float24(10);

            publish_state("heating_consumed", status.EnergyConsumedHeating);
            publish_state("cool_consumed", status.EnergyConsumedCooling);
            publish_state("dhw_consumed", status.EnergyConsumedDhw);
            break;
        case GetType::ENERGY_DELIVERY:
            status.EnergyDeliveredHeating = res.get_float24(4);
            status.EnergyDeliveredCooling = res.get_float24(7);
            status.EnergyDeliveredDhw = res.get_float24(10);

            publish_state("heating_delivered", status.EnergyDeliveredHeating);
            publish_state("cool_delivered", status.EnergyDeliveredCooling);
            publish_state("dhw_delivered", status.EnergyDeliveredDhw);
            
            publish_state("heating_cop", status.EnergyConsumedHeating > 0.0f ? status.EnergyDeliveredHeating / status.EnergyConsumedHeating : 0.0f);
            publish_state("cool_cop", status.EnergyConsumedCooling > 0.0f ? status.EnergyDeliveredCooling / status.EnergyConsumedCooling : 0.0f);
            publish_state("dhw_cop", status.EnergyConsumedDhw > 0.0f ? status.EnergyDeliveredDhw / status.EnergyConsumedDhw : 0.0f);

            break;
        case GetType::HARDWARE_CONFIGURATION:
            // byte 6 = ftc, ft2b , ftc4, ftc5, ftc6
            status.Controller = res[6];
            publish_state("controller_version", static_cast<float>(status.Controller));

            // byte 10 = R410A, R32, R290
            // status.RefrigerantCode = res[10];
            initialCount |= 1;

            break;
        case GetType::DIP_SWITCHES:
            status.DipSwitch1 = res[1];
            status.DipSwitch2 = res[3];
            status.DipSwitch3 = res[5];
            status.DipSwitch4 = res[7];
            status.DipSwitch5 = res[9];
            status.DipSwitch6 = res[11];
            status.DipSwitch7 = res[13];
            initialCount |= 2;
            break;
        default:
            ESP_LOGI(TAG, "Unknown response type received on serial port: %u", static_cast<uint8_t>(res.payload_type<GetType>()));
            //ESP_LOGE(TAG, res.debug_dump_packet().c_str());
            break;
        }
    }

    void EcodanHeatpump::handle_connect_response(Message& res)
    {
        ESP_LOGI(TAG, "connection reply received from heat pump");

        connected = true;
    }

    void EcodanHeatpump::handle_response(Message& res) {
        switch (res.type())
        {
        case MsgType::SET_RES:
            handle_set_response(res);
            break;
        case MsgType::GET_RES:
        case MsgType::CONFIGURATION_RES:
            handle_get_response(res);
            break;
        case MsgType::CONNECT_RES:
            handle_connect_response(res);
            break;
        default:
            ESP_LOGI(TAG, "Unknown serial message type received: %#x", static_cast<uint8_t>(res.type()));
            break;
        }
    }

    void EcodanHeatpump::handle_set_response(Message& res)
    {
        //ESP_LOGW(TAG, res.debug_dump_packet().c_str());
        if (!cmdQueue.empty())
            cmdQueue.pop();

        if (res.type() != MsgType::SET_RES)
        {
            ESP_LOGI(TAG, "Unexpected set response type: %#x", static_cast<uint8_t>(res.type()));
        }          
    }
}
}
