import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID
from esphome.const import (
    ENTITY_CATEGORY_NONE
)

from . import ECODAN, CONF_ECODAN_ID, ECODAN_CLIMATE
AUTO_LOAD = ["ecodan"]

CONFIG_SCHEMA = cv.Schema(
    {
        #cv.GenerateID(CONF_ECODAN_ID): cv.use_id(ECODAN), 
        cv.Optional("heatpump_climate_z1"): climate.CLIMATE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
            }).extend(cv.polling_component_schema('100ms')),
        cv.Optional("heatpump_climate_room_z1"): climate.CLIMATE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("thermostat_climate_mode"): cv.boolean,
            }).extend(cv.polling_component_schema('100ms')),            
        cv.Optional("heatpump_climate_z2"): climate.CLIMATE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
            }).extend(cv.polling_component_schema('100ms')),
        cv.Optional("heatpump_climate_room_z2"): climate.CLIMATE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("thermostat_climate_mode"): cv.boolean,
            }).extend(cv.polling_component_schema('100ms')),     
        cv.Optional("heatpump_climate_dhw"): climate.CLIMATE_SCHEMA.extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("dhw_climate_mode"): cv.boolean,
            }).extend(cv.polling_component_schema('100ms')),                               
    })


async def to_code(config):
    #hp = await cg.get_variable(config[CONF_ECODAN_ID])

    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf.get("id")
        if id and id.type == climate.Climate:
            inst = cg.new_Pvariable(conf[CONF_ID])
            cg.add(inst.set_status(cg.RawExpression(f'[=](void) -> const ecodan::Status& {{ {conf["get_status_func"]} }}')))
            cg.add(inst.set_target_temp_func(cg.RawExpression(f'[=](float x){{ {conf["target_temp_func"]} }}')))
            cg.add(inst.set_get_target_temp_func(cg.RawExpression(f'[=](void) -> float {{ {conf["get_target_temp_func"]} }}')))
            
            if "current_temp_func" in conf:
                cg.add(inst.set_get_current_temp_func(cg.RawExpression(f'[=](void) -> float {{ {conf["current_temp_func"]} }}')))
            if "dhw_climate_mode" in conf:
                cg.add(inst.set_dhw_climate_mode(True))
            if "thermostat_climate_mode" in conf:
                cg.add(inst.set_thermostat_climate_mode(True))            
            await cg.register_component(inst, conf)
            await climate.register_climate(inst, conf)
