import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from esphome.const import (
    ENTITY_CATEGORY_NONE,
    DEVICE_CLASS_RUNNING,
)

from . import ECODAN, CONF_ECODAN_ID


AUTO_LOAD = ["ecodan"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECODAN_ID): cv.use_id(ECODAN),
        cv.Optional("status_defrost"): binary_sensor.binary_sensor_schema(
            icon="mdi:snowflake-melt",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_dhw_forced"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-boiler-alert",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_holiday"): binary_sensor.binary_sensor_schema(
            icon="mdi:beach",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_booster"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-boiler",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_immersion"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-boiler",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_in1_request"): binary_sensor.binary_sensor_schema(
            icon="mdi:thermostat",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_in6_request"): binary_sensor.binary_sensor_schema(
            icon="mdi:thermostat",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_in5_request"): binary_sensor.binary_sensor_schema(
            icon="mdi:thermostat",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_water_pump"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-pump",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_three_way_valve"): binary_sensor.binary_sensor_schema(
            icon="mdi:valve",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_water_pump_2"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-pump",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_water_pump_3"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-pump",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_three_way_valve_2"): binary_sensor.binary_sensor_schema(
            icon="mdi:valve",
            entity_category=ENTITY_CATEGORY_NONE,
            device_class=DEVICE_CLASS_RUNNING,
        ),
        cv.Optional("status_prohibit_dhw"): binary_sensor.binary_sensor_schema(
            icon="mdi:water-boiler-off",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_prohibit_heating_z1"): binary_sensor.binary_sensor_schema(
            icon="mdi:radiator-off",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_prohibit_cool_z1"): binary_sensor.binary_sensor_schema(
            icon="mdi:hvac-off",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_prohibit_heating_z2"): binary_sensor.binary_sensor_schema(
            icon="mdi:radiator-off",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_prohibit_cool_z2"): binary_sensor.binary_sensor_schema(
            icon="mdi:hvac-off",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("status_dhw_eco"): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_NONE,
            icon="mdi:water-boiler-off",
        ),
        cv.Optional("status_power"): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_NONE,
            icon="mdi:power-off",
        ),
    }).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    hp = await cg.get_variable(config[CONF_ECODAN_ID])

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == binary_sensor.BinarySensor:
            sens = await binary_sensor.new_binary_sensor(conf)
            cg.add(hp.register_binarySensor(sens, key))
