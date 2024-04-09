import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID
from esphome.const import (
    ENTITY_CATEGORY_NONE
)

from . import ECODAN, CONF_ECODAN_ID


AUTO_LOAD = ["ecodan"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ECODAN_ID): cv.use_id(ECODAN),
        cv.Optional("mode_defrost"): text_sensor.text_sensor_schema(
            icon="mdi:radiobox-blank",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_dhw"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_dhw_forced"): text_sensor.text_sensor_schema(
            icon="mdi:radiobox-blank",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_heating"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_power"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_operation"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_holiday"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_booster"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_prohibit_dhw"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_prohibit_heating_z1"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_prohibit_cool_z1"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_prohibit_heating_z2"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
        ),        
        cv.Optional("mode_prohibit_cool_z2"): text_sensor.text_sensor_schema(
            icon="mdi:alert",            
            entity_category=ENTITY_CATEGORY_NONE,
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
