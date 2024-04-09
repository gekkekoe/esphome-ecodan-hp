import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_WATER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_HOUR,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
)
from . import ECODAN, CONF_ECODAN_ID

AUTO_LOAD = ["ecodan"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECODAN_ID): cv.use_id(ECODAN),
        cv.Optional("compressor_frequency"): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            icon="mdi:sine-wave",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("output_power"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            icon="mdi:home-lightning-bolt",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("outside_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:sun-thermometer",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("hp_feed_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("hp_return_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("boiler_flow_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("boiler_return_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),        
        cv.Optional("dhw_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:hand-water",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("dhw_flow_temp_target"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:hand-water",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("dhw_flow_temp_drop"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:hand-water",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("legionella_prevention_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:hand-water",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),        
        cv.Optional("flow_rate"): sensor.sensor_schema(
            unit_of_measurement="l/m",
            icon="mdi:waves-arrow-right",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("cool_cop"): sensor.sensor_schema(
            icon="mdi:heat-pump-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),        
        cv.Optional("cool_consumed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("cool_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-import",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("heating_cop"): sensor.sensor_schema(
            icon="mdi:heat-pump-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),        
        cv.Optional("heating_consumed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("heating_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-import",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("dhw_cop"): sensor.sensor_schema(
            icon="mdi:heat-pump-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=3,
        ),         
        cv.Optional("dhw_consumed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("dhw_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-import",
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("sh_flow_temp_target"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("z1_flow_temp_target"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("z1_room_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),                        
        cv.Optional("z1_room_temp_target"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),            
        cv.Optional("z2_flow_temp_target"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("z2_room_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),                        
        cv.Optional("z2_room_temp_target"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("runtime"): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            icon="mdi:clock",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    hp = await cg.get_variable(config[CONF_ECODAN_ID])

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == sensor.Sensor:
            sens = await sensor.new_sensor(conf)
            cg.add(hp.register_sensor(sens, key))

#    for key, conf in config.items():
#        sens = await sensor.new_sensor(conf)
#        cg.add(hp.register_sensor(sens, key))
        