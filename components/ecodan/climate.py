import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.components.thermostat import climate as thermostat_cli
from esphome.const import CONF_ID
from esphome.const import (
    ENTITY_CATEGORY_NONE
)


from . import ECODAN, CONF_ECODAN_ID, ECODAN_CLIMATE
AUTO_LOAD = ["ecodan", "thermostat"]
ecodan_ns = cg.esphome_ns.namespace("ecodan")
EcodanClimate = ecodan_ns.class_("EcodanClimate", climate.Climate, cg.Component)
EcodanVirtualThermostat = ecodan_ns.class_('EcodanVirtualThermostat', thermostat_cli.ThermostatClimate)
CONF_VARIANT = "variant"

base_schema = thermostat_cli.CONFIG_SCHEMA.validators[0]
# extend with id
extended_schema = base_schema.extend({
    cv.GenerateID(): cv.declare_id(EcodanVirtualThermostat),
})
extra_validators = thermostat_cli.CONFIG_SCHEMA.validators[1:]
VIRTUAL_SCHEMA = cv.All(extended_schema, *extra_validators)

HARDWARE_SCHEMA = cv.Schema(
    {
        #cv.GenerateID(CONF_ECODAN_ID): cv.use_id(ECODAN), 
        cv.Optional("heatpump_climate_z1"): climate.climate_schema(EcodanClimate).extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("zone_identifier"): cv.uint8_t,
            }).extend(cv.polling_component_schema('250ms')),
        cv.Optional("heatpump_climate_room_z1"): climate.climate_schema(EcodanClimate).extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("zone_identifier"): cv.uint8_t,
                cv.Optional("thermostat_climate_mode"): cv.boolean,
            }).extend(cv.polling_component_schema('250ms')),            
        cv.Optional("heatpump_climate_z2"): climate.climate_schema(EcodanClimate).extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("zone_identifier"): cv.uint8_t,
            }).extend(cv.polling_component_schema('250ms')),
        cv.Optional("heatpump_climate_room_z2"): climate.climate_schema(EcodanClimate).extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("zone_identifier"): cv.uint8_t,
                cv.Optional("thermostat_climate_mode"): cv.boolean,
            }).extend(cv.polling_component_schema('250ms')),     
        cv.Optional("heatpump_climate_dhw"): climate.climate_schema(EcodanClimate).extend(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(ECODAN_CLIMATE),
                cv.Required("get_status_func"): cv.string,
                cv.Required("target_temp_func"): cv.string,
                cv.Required("get_target_temp_func"): cv.string,
                cv.Optional("current_temp_func"): cv.string,
                cv.Optional("dhw_climate_mode"): cv.boolean,
            }).extend(cv.polling_component_schema('250ms')),                               
    })


CONFIG_SCHEMA = cv.typed_schema({
    "hardware": HARDWARE_SCHEMA,
    "virtual": VIRTUAL_SCHEMA,
}, key=CONF_VARIANT, default_type="hardware")


async def to_code(config):
    if config[CONF_VARIANT] == "virtual":
        # use parent code gen
        await thermostat_cli.to_code(config)        
    else:
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
                if "zone_identifier" in conf:
                    cg.add(inst.set_zone_identifier(conf["zone_identifier"]))

                await cg.register_component(inst, conf)
                await climate.register_climate(inst, conf)
