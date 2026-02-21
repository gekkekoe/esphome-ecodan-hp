import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.const import CONF_ID

DEPENDENCIES = ["web_server_base", "network"]
AUTO_LOAD = ["web_server_base"]

CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

from esphome.components import sensor, binary_sensor, text_sensor, climate, number, switch, select

asgard_dashboard_ns = cg.esphome_ns.namespace("asgard_dashboard")
EcodanDashboard = asgard_dashboard_ns.class_("EcodanDashboard", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EcodanDashboard),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),

        cv.Optional("hp_feed_temp_id"):                    cv.use_id(sensor.Sensor),
        cv.Optional("hp_return_temp_id"):                  cv.use_id(sensor.Sensor),
        cv.Optional("outside_temp_id"):                    cv.use_id(sensor.Sensor),
        cv.Optional("compressor_frequency_id"):            cv.use_id(sensor.Sensor),
        cv.Optional("flow_rate_id"):                       cv.use_id(sensor.Sensor),
        cv.Optional("computed_output_power_id"):           cv.use_id(sensor.Sensor),
        cv.Optional("daily_computed_output_power_id"):     cv.use_id(sensor.Sensor),
        cv.Optional("daily_total_energy_consumption_id"):  cv.use_id(sensor.Sensor),
        cv.Optional("compressor_starts_id"):               cv.use_id(sensor.Sensor),
        cv.Optional("runtime_id"):                         cv.use_id(sensor.Sensor),
        cv.Optional("wifi_signal_db_id"):                  cv.use_id(sensor.Sensor),
        cv.Optional("dhw_temp_id"):                        cv.use_id(sensor.Sensor),
        cv.Optional("dhw_flow_temp_target_id"):            cv.use_id(sensor.Sensor),
        cv.Optional("dhw_flow_temp_drop_id"):              cv.use_id(sensor.Sensor),
        cv.Optional("dhw_consumed_id"):                    cv.use_id(sensor.Sensor),
        cv.Optional("dhw_delivered_id"):                   cv.use_id(sensor.Sensor),
        cv.Optional("dhw_cop_id"):                         cv.use_id(sensor.Sensor),
        cv.Optional("heating_consumed_id"):                cv.use_id(sensor.Sensor),
        cv.Optional("heating_produced_id"):                cv.use_id(sensor.Sensor),
        cv.Optional("heating_cop_id"):                     cv.use_id(sensor.Sensor),
        cv.Optional("cooling_consumed_id"):                cv.use_id(sensor.Sensor),
        cv.Optional("cooling_produced_id"):                cv.use_id(sensor.Sensor),
        cv.Optional("cooling_cop_id"):                     cv.use_id(sensor.Sensor),

        cv.Optional("z1_flow_temp_target_id"):             cv.use_id(sensor.Sensor),
        cv.Optional("z2_flow_temp_target_id"):             cv.use_id(sensor.Sensor),

        cv.Optional("operation_mode_id"):                  cv.use_id(sensor.Sensor),

        cv.Optional("status_compressor_id"):               cv.use_id(binary_sensor.BinarySensor),
        cv.Optional("status_booster_id"):                  cv.use_id(binary_sensor.BinarySensor),
        cv.Optional("status_defrost_id"):                  cv.use_id(binary_sensor.BinarySensor),
        cv.Optional("status_water_pump_id"):               cv.use_id(binary_sensor.BinarySensor),
        cv.Optional("status_in1_request_id"):              cv.use_id(binary_sensor.BinarySensor),
        cv.Optional("status_in6_request_id"):              cv.use_id(binary_sensor.BinarySensor),
        cv.Optional("zone2_enabled_id"):                   cv.use_id(binary_sensor.BinarySensor),

        cv.Optional("heating_system_type_id"):             cv.use_id(text_sensor.TextSensor),
        cv.Optional("room_temp_source_z1_id"):             cv.use_id(text_sensor.TextSensor),
        cv.Optional("room_temp_source_z2_id"):             cv.use_id(text_sensor.TextSensor),
        cv.Optional("version_id"):                         cv.use_id(text_sensor.TextSensor),

        cv.Optional("sw_auto_adaptive_id"):                cv.use_id(switch.Switch),
        cv.Optional("sw_defrost_mit_id"):                  cv.use_id(switch.Switch),
        cv.Optional("sw_smart_boost_id"):                  cv.use_id(switch.Switch),
        cv.Optional("sw_force_dhw_id"):                    cv.use_id(switch.Switch),

        cv.Optional("sel_heating_system_type_id"):         cv.use_id(select.Select),
        cv.Optional("sel_room_temp_source_z1_id"):         cv.use_id(select.Select),
        cv.Optional("sel_room_temp_source_z2_id"):         cv.use_id(select.Select),
        cv.Optional("operating_mode_z1_id"):               cv.use_id(select.Select),
        cv.Optional("operating_mode_z2_id"):               cv.use_id(select.Select),
        cv.Optional("sel_temp_source_z1_id"):              cv.use_id(select.Select),
        cv.Optional("sel_temp_source_z2_id"):              cv.use_id(select.Select),

        cv.Optional("num_aa_setpoint_bias_id"):            cv.use_id(number.Number),
        cv.Optional("num_max_flow_temp_id"):               cv.use_id(number.Number),
        cv.Optional("num_min_flow_temp_id"):               cv.use_id(number.Number),
        cv.Optional("num_max_flow_temp_z2_id"):            cv.use_id(number.Number),
        cv.Optional("num_min_flow_temp_z2_id"):            cv.use_id(number.Number),
        cv.Optional("num_hysteresis_z1_id"):               cv.use_id(number.Number),
        cv.Optional("num_hysteresis_z2_id"):               cv.use_id(number.Number),

        cv.Optional("dhw_climate_id"):                     cv.use_id(climate.Climate),
        cv.Optional("virtual_climate_z1_id"):              cv.use_id(climate.Climate),
        cv.Optional("virtual_climate_z2_id"):              cv.use_id(climate.Climate),
        cv.Optional("heatpump_climate_z1_id"):             cv.use_id(climate.Climate),
        cv.Optional("heatpump_climate_z2_id"):             cv.use_id(climate.Climate),
        cv.Required("flow_climate_z1_id"):                 cv.use_id(climate.Climate),
        cv.Optional("flow_climate_z2_id"):                 cv.use_id(climate.Climate),

        cv.Optional("predictive_short_cycle_control_enabled_id"):      cv.use_id(switch.Switch),
        cv.Optional("predictive_short_cycle_high_delta_time_window_id"): cv.use_id(number.Number),
        cv.Optional("predictive_short_cycle_high_delta_threshold_id"):   cv.use_id(number.Number),

    }
).extend(cv.COMPONENT_SCHEMA)


async def _wire(config, var, conf_key, setter):
    if conf_key in config:
        ent = await cg.get_variable(config[conf_key])
        cg.add(getattr(var, setter)(ent))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    wsb = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    cg.add(var.set_web_server_base(wsb))

    pairs = [
        ("version_id",                        "set_version"),
        ("hp_feed_temp_id",                   "set_hp_feed_temp"),
        ("hp_return_temp_id",                 "set_hp_return_temp"),
        ("outside_temp_id",                   "set_outside_temp"),
        ("compressor_frequency_id",           "set_compressor_frequency"),
        ("flow_rate_id",                      "set_flow_rate"),
        ("computed_output_power_id",          "set_computed_output_power"),
        ("daily_computed_output_power_id",    "set_daily_computed_output_power"),
        ("daily_total_energy_consumption_id", "set_daily_total_energy_consumption"),
        ("compressor_starts_id",              "set_compressor_starts"),
        ("runtime_id",                        "set_runtime"),
        ("wifi_signal_db_id",                 "set_wifi_signal_db"),
        ("dhw_temp_id",                       "set_dhw_temp"),
        ("dhw_flow_temp_target_id",           "set_dhw_flow_temp_target"),
        ("dhw_flow_temp_drop_id",             "set_dhw_flow_temp_drop"),
        ("dhw_consumed_id",                   "set_dhw_consumed"),
        ("dhw_delivered_id",                  "set_dhw_delivered"),
        ("dhw_cop_id",                        "set_dhw_cop"),
        ("heating_consumed_id",               "set_heating_consumed"),
        ("heating_produced_id",               "set_heating_produced"),
        ("heating_cop_id",                    "set_heating_cop"),
        ("cooling_consumed_id",               "set_cooling_consumed"),
        ("cooling_produced_id",               "set_cooling_produced"),
        ("cooling_cop_id",                    "set_cooling_cop"),
        ("z1_flow_temp_target_id",            "set_z1_flow_temp_target"),
        ("z2_flow_temp_target_id",            "set_z2_flow_temp_target"),
        ("status_compressor_id",              "set_status_compressor"),
        ("status_booster_id",                 "set_status_booster"),
        ("status_defrost_id",                 "set_status_defrost"),
        ("status_water_pump_id",              "set_status_water_pump"),
        ("status_in1_request_id",             "set_status_in1_request"),
        ("status_in6_request_id",             "set_status_in6_request"),
        ("zone2_enabled_id",                  "set_status_zone2_enabled"),
        ("operation_mode_id",                 "set_operation_mode"),
        ("heating_system_type_id",            "set_heating_system_type"),
        ("room_temp_source_z1_id",            "set_room_temp_source_z1"),
        ("room_temp_source_z2_id",            "set_room_temp_source_z2"),
        ("sw_auto_adaptive_id",               "set_sw_auto_adaptive"),
        ("sw_defrost_mit_id",                 "set_sw_defrost_mit"),
        ("sw_smart_boost_id",                 "set_sw_smart_boost"),
        ("sw_force_dhw_id",                   "set_sw_force_dhw"),
        ("sel_heating_system_type_id",        "set_sel_heating_system_type"),
        ("sel_room_temp_source_z1_id",        "set_sel_room_temp_source_z1"),
        ("sel_room_temp_source_z2_id",        "set_sel_room_temp_source_z2"),
        ("operating_mode_z1_id",              "set_sel_operating_mode_z1"),
        ("operating_mode_z2_id",              "set_sel_operating_mode_z2"),
        ("sel_temp_source_z1_id",             "set_sel_temp_source_z1"),
        ("sel_temp_source_z2_id",             "set_sel_temp_source_z2"),
        ("num_aa_setpoint_bias_id",           "set_num_aa_setpoint_bias"),
        ("num_max_flow_temp_id",              "set_num_max_flow_temp"),
        ("num_min_flow_temp_id",              "set_num_min_flow_temp"),
        ("num_max_flow_temp_z2_id",           "set_num_max_flow_temp_z2"),
        ("num_min_flow_temp_z2_id",           "set_num_min_flow_temp_z2"),
        ("num_hysteresis_z1_id",              "set_num_hysteresis_z1"),
        ("num_hysteresis_z2_id",              "set_num_hysteresis_z2"),
        ("dhw_climate_id",                    "set_dhw_climate"),
        ("virtual_climate_z1_id",             "set_virtual_climate_z1"),
        ("virtual_climate_z2_id",             "set_virtual_climate_z2"),
        ("heatpump_climate_z1_id",            "set_heatpump_climate_z1"),
        ("heatpump_climate_z2_id",            "set_heatpump_climate_z2"),
        ("flow_climate_z1_id",                "set_flow_climate_z1"),
        ("flow_climate_z2_id",                "set_flow_climate_z2"),
        ("predictive_short_cycle_control_enabled_id",    "set_pred_sc_switch"),
        ("predictive_short_cycle_high_delta_time_window_id", "set_pred_sc_time"),
        ("predictive_short_cycle_high_delta_threshold_id",   "set_pred_sc_delta"),
    ]

    for conf_key, setter in pairs:
        await _wire(config, var, conf_key, setter)