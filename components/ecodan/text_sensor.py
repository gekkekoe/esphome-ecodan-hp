import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from esphome.const import (
    ENTITY_CATEGORY_NONE, ENTITY_CATEGORY_DIAGNOSTIC
)

from . import ECODAN, CONF_ECODAN_ID


AUTO_LOAD = ["ecodan"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECODAN_ID): cv.use_id(ECODAN),
        cv.Optional("controller_version_text"): text_sensor.text_sensor_schema(
            icon="mdi:new-box",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("controller_firmware_text"): text_sensor.text_sensor_schema(
            icon="mdi:new-box",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),        
        cv.Optional("heat_source_text"): text_sensor.text_sensor_schema(
            icon="mdi:fire",
            entity_category=ENTITY_CATEGORY_NONE,
        ),
        cv.Optional("fault_code_text"): text_sensor.text_sensor_schema(
            icon="mdi:fire",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional("refrigerant_error_code_text"): text_sensor.text_sensor_schema(
            icon="mdi:fire",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    hp = await cg.get_variable(config[CONF_ECODAN_ID])

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == text_sensor.TextSensor:
            sens = await text_sensor.new_text_sensor(conf)
            cg.add(hp.register_textSensor(sens, key))
