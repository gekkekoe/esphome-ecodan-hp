import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, switch, text, binary_sensor, climate, number, globals, select
from esphome.const import CONF_ID

CODEOWNERS = ["@gekkekoe"]

AUTO_LOAD = ["mqtt"]

odin_ns = cg.esphome_ns.namespace("odin")
ODIN_FORWARDER = odin_ns.class_("OdinForwarder", cg.PollingComponent)

ecodan_ns = cg.esphome_ns.namespace("ecodan")
EcodanHeatpump = ecodan_ns.class_("EcodanHeatpump", cg.PollingComponent)

CONF_HP_ID = "hp_id"
CONF_TOPIC_PREFIX = "topic_prefix"
CONF_ECODAN_ID = "ecodan_id"
CONF_THERMAL_TOTAL_ID = "thermal_total_id"
CONF_DAILY_CONSUMPTION_ID = "daily_consumption_id"
CONF_METER_SOURCE_ID = "meter_source_id"
CONF_METER_FEEDBACK_ID = "meter_feedback_id"
CONF_RELAY_Z1_ID = "relay_z1_id"
CONF_RELAY_Z2_ID = "relay_z2_id"
CONF_OVERRIDE_Z1_ID = "override_z1_id"
CONF_OVERRIDE_Z2_ID = "override_z2_id"
CONF_ACTIVE_SENSOR_ID = "active_sensor_id"
CONF_TAKEOVER_SENSOR_ID = "takeover_sensor_id"
CONF_LOCKOUT_SENSOR_ID = "lockout_sensor_id"
CONF_BROKER_IP_ID = "broker_ip_id"
CONF_BROKER_PORT_ID = "broker_port_id"
CONF_MQTT_USER_ID = "mqtt_user_id"
CONF_MQTT_PASSWORD_ID = "mqtt_password_id"
CONF_MQTT_TOPIC_PREFIX_ID = "mqtt_topic_prefix_id"
CONF_CLIMATE_ID = "climate_id"
CONF_CLIMATE_Z2_ID = "climate_z2_id"
CONF_DHW_FLOW_TEMP_TARGET_ID = "dhw_flow_temp_target_id"
CONF_DHW_FLOW_TEMP_DROP_ID = "dhw_flow_temp_drop_id"
CONF_REGULAR_DHW_SWITCH_ID = "regular_dhw_switch_id"
CONF_FORCE_DHW_SWITCH_ID = "force_dhw_switch_id"
CONF_LEGIONELLA_SAVED_SETPOINT_ID = "legionella_saved_dhw_setpoint_id"
CONF_DHW_CLIMATE_ID = "dhw_climate_id"

_OPTIONAL_ENTITIES = [
    (CONF_THERMAL_TOTAL_ID, "set_thermal_total", sensor.Sensor),
    (CONF_DAILY_CONSUMPTION_ID, "set_daily_consumption", sensor.Sensor),
    (CONF_METER_SOURCE_ID, "set_meter_source", select.Select),
    (CONF_METER_FEEDBACK_ID, "set_meter_feedback", number.Number),
    (CONF_RELAY_Z1_ID, "set_relay_z1", switch.Switch),
    (CONF_RELAY_Z2_ID, "set_relay_z2", switch.Switch),
    (CONF_OVERRIDE_Z1_ID, "set_override_z1", switch.Switch),
    (CONF_OVERRIDE_Z2_ID, "set_override_z2", switch.Switch),
    (CONF_BROKER_IP_ID, "set_broker_ip", text.Text),
    (CONF_BROKER_PORT_ID, "set_broker_port", text.Text),
    (CONF_MQTT_USER_ID, "set_mqtt_user", text.Text),
    (CONF_MQTT_PASSWORD_ID, "set_mqtt_password", text.Text),
    (CONF_MQTT_TOPIC_PREFIX_ID, "set_mqtt_topic_prefix", text.Text),
    (CONF_CLIMATE_ID, "set_climate", climate.Climate),
    (CONF_CLIMATE_Z2_ID, "set_climate_z2", climate.Climate),
    (CONF_DHW_FLOW_TEMP_TARGET_ID, "set_dhw_flow_temp_target", sensor.Sensor),
    (CONF_DHW_FLOW_TEMP_DROP_ID, "set_dhw_flow_temp_drop", sensor.Sensor),
    (CONF_REGULAR_DHW_SWITCH_ID, "set_regular_dhw_switch", switch.Switch),
    (CONF_FORCE_DHW_SWITCH_ID, "set_force_dhw_switch", switch.Switch),
    (CONF_ACTIVE_SENSOR_ID, "set_active_sensor", binary_sensor.BinarySensor),
    (CONF_TAKEOVER_SENSOR_ID, "set_takeover_sensor", binary_sensor.BinarySensor),
    (CONF_LOCKOUT_SENSOR_ID, "set_lockout_sensor", binary_sensor.BinarySensor),
    (CONF_LEGIONELLA_SAVED_SETPOINT_ID, "set_legionella_saved_setpoint", globals.GlobalsComponent),
    (CONF_DHW_CLIMATE_ID, "set_dhw_climate", climate.Climate),
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ODIN_FORWARDER),
        cv.Required(CONF_HP_ID): cv.string,
        cv.Optional(CONF_TOPIC_PREFIX, default="hp"): cv.string,
        cv.Required(CONF_ECODAN_ID): cv.use_id(EcodanHeatpump),
        **{cv.Optional(key): cv.use_id(cls) for key, _, cls in _OPTIONAL_ENTITIES},
    }
).extend(cv.polling_component_schema("10s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_hp_id(config[CONF_HP_ID]))
    cg.add(var.set_topic_prefix(config[CONF_TOPIC_PREFIX]))
    ecodan = await cg.get_variable(config[CONF_ECODAN_ID])
    cg.add(var.set_ecodan(ecodan))
    for key, setter, _cls in _OPTIONAL_ENTITIES:
        if key in config:
            entity = await cg.get_variable(config[key])
            cg.add(getattr(var, setter)(entity))
