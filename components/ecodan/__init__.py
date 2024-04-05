import math
import esphome.codegen as cg
import esphome.cpp_generator as cppg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_VERSION, CONF_NAME, UNIT_PERCENT, CONF_RX_PIN, CONF_TX_PIN
from esphome.components import text_sensor, binary_sensor, sensor
from enum import Enum

CODEOWNERS = ["@gekkekoe"]
#DEPENDENCIES = ["uart"]

AUTO_LOAD = ["text_sensor", "binary_sensor", "sensor"]
MULTI_CONF = True

CONF_HUB_ID = 'ecodan'

empty_sensor_hub_ns = cg.esphome_ns.namespace(CONF_HUB_ID)

Ecodan = empty_sensor_hub_ns.class_('EcodanHeatpump', cg.Component)

sensors = {
    "boiler_flow_temp":             { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "boiler_return_temp":           { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "compressor_frequency":         { "unit": "Hz", "icon": "mdi:fan"},

    "cool_cop":                     { "unit": "COP", "accuracy": 2, "icon": "mdi:heat-pump-outline"},
    "cool_consumed":                { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},
    "cool_delivered":               { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},
    "dhw_cop":                      { "unit": "COP", "accuracy": 2, "icon": "mdi:heat-pump-outline"},
    "dhw_consumed":                 { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},
    "dhw_delivered":                { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},
    "heating_cop":                  { "unit": "COP", "accuracy": 2, "icon": "mdi:heat-pump-outline"},
    "heating_consumed":             { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},
    "heating_delivered":            { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},

    #"dhw_flow_temp":                { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "dhw_flow_temp_target":         { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "dhw_flow_temp_drop":           { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "dhw_temp":                     { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},

    "flow_rate":                    { "unit": "L/min", "icon": "mdi:pump"},
    "hp_feed_temp":                 { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "hp_return_temp":               { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "legionella_prevention_temp":   { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},

    "output_power":                 { "unit": "kWh", "accuracy": 2, "icon": "mdi:lightning-bolt"},
    "outside_temp":                 { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},

    "sh_flow_temp_target":          { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "z1_flow_temp_target":          { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "z1_room_temp":                 { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "z1_room_temp_target":          { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},

    "z2_flow_temp_target":          { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "z2_room_temp":                 { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},
    "z2_room_temp_target":          { "unit": "C", "accuracy": 1, "icon": "mdi:thermometer"},

}

textSensors = {    
    "mode_defrost":                 { "icon": "mdi:radiobox-blank"},
    "mode_dhw":                     {},
    "mode_dhw_forced":              { "icon": "mdi:radiobox-blank" },
    "mode_heating":                 {},
    "mode_power":                   {},
    "mode_operation":               {},
}

GEN_SENSORS_SCHEMA = {
    cv.Optional(key, default=key): cv.maybe_simple_value(
        sensor.sensor_schema(unit_of_measurement=value['unit'],
                             accuracy_decimals=value['accuracy'] if 'accuracy' in value else 0, 
                             icon=value['icon'] if 'icon' in value else ''), key=CONF_NAME, accuracy_decimals=2 )
    for key, value in sensors.items()
}

GEN_TEXTSENSORS_SCHEMA = {
    cv.Optional(key, default=key): cv.maybe_simple_value(
        text_sensor.text_sensor_schema(icon=value['icon'] if 'icon' in value else ''), key=CONF_NAME, accuracy_decimals=2 )
    for key, value in textSensors.items()
}

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(Ecodan),
        cv.Optional(CONF_RX_PIN, default=2): pins.internal_gpio_input_pin_number,
        cv.Optional(CONF_TX_PIN, default=1): pins.internal_gpio_output_pin_number,
    })
    .extend(GEN_SENSORS_SCHEMA)
    .extend(GEN_TEXTSENSORS_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    )

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_rx(config[CONF_RX_PIN]))
    cg.add(var.set_tx(config[CONF_TX_PIN]))

    for key, value in sensors.items():
        sens = await sensor.new_sensor(config[key])
        cg.add(var.register_sensor(sens, key))

    for key, value in textSensors.items():
        sens = await text_sensor.new_text_sensor(config[key])
        cg.add(var.register_textSensor(sens, key))

