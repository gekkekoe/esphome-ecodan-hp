import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_WATER,
    ENTITY_CATEGORY_NONE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_HOUR,
    UNIT_WATT,
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
        cv.Optional("computed_output_power"): sensor.sensor_schema(
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
        cv.Optional("hp_refrigerant_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("hp_refrigerant_condensing_temp"): sensor.sensor_schema(
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
        cv.Optional("dhw_secondary_temp"): sensor.sensor_schema(
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
            unit_of_measurement="L/min",
            icon="mdi:waves-arrow-right",
            device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("energy_consumed_increasing"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional("cool_cop"): sensor.sensor_schema(
            icon="mdi:heat-pump-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=2,
        ),        
        cv.Optional("cool_consumed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("cool_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-import",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("heating_cop"): sensor.sensor_schema(
            icon="mdi:heat-pump-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=2,
        ),        
        cv.Optional("heating_consumed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("heating_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-import",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("dhw_cop"): sensor.sensor_schema(
            icon="mdi:heat-pump-outline",
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=2,
        ),         
        cv.Optional("dhw_consumed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-export",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
        ),
        cv.Optional("dhw_delivered"): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            icon="mdi:transmission-tower-import",
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL,
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
        cv.Optional("z1_feed_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("z1_return_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("z2_feed_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("z2_return_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
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
        cv.Optional("mixing_tank_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:home-thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),     
        cv.Optional("runtime"): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            icon="mdi:clock",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("controller_version"): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional("heat_source"): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional("refrigerant_error_code"): sensor.sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional("status_mixing_valve"): sensor.sensor_schema(
            icon="mdi:valve",
            state_class=ENTITY_CATEGORY_NONE,
        ), 
        cv.Optional("mixing_valve_step"): sensor.sensor_schema(
            icon="mdi:valve",
            state_class=STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("mixing_valve_step_z1"): sensor.sensor_schema(
            icon="mdi:valve",
            state_class=STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("status_multi_zone"): sensor.sensor_schema(
            icon="mdi:thermostat",
            state_class=ENTITY_CATEGORY_NONE
        ),
        cv.Optional("compressor_starts"): sensor.sensor_schema(
            icon="mdi:counter",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("discharge_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("ou_liquid_pipe_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("ou_two_phase_pipe_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("ou_suction_pipe_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("ou_heatsink_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("ou_compressor_surface_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("super_heat_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("sub_cool_temp"): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:coolant-temperature",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("fan_speed"): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            icon="mdi:fan",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("pump_speed"): sensor.sensor_schema(
            icon="mdi:gauge",
            state_class=STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("pump_feedback"): sensor.sensor_schema(
            icon="mdi:meter-electric",
            accuracy_decimals=0,
            unit_of_measurement=UNIT_WATT,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("operation_mode"): sensor.sensor_schema(
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
            if key in ["compressor_starts", "discharge_temp", "ou_liquid_pipe_temp", "ou_two_phase_pipe_temp", "fan_speed"]:
                cg.add(hp.enable_request_code_sensors())