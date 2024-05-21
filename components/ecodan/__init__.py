import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN

CODEOWNERS = ["@gekkekoe"]

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]

CONF_ECODAN_ID = "ecodan_id"

empty_sensor_hub_ns = cg.esphome_ns.namespace('ecodan')

ECODAN = empty_sensor_hub_ns.class_('EcodanHeatpump', cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ECODAN),
        cv.Optional(CONF_RX_PIN, default=2): pins.internal_gpio_input_pin_number,
        cv.Optional(CONF_TX_PIN, default=1): pins.internal_gpio_output_pin_number,
    }
    ).extend(cv.polling_component_schema('500ms'))    

async def to_code(config):
    hp = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hp, config)
    cg.add(hp.set_rx(config[CONF_RX_PIN]))
    cg.add(hp.set_tx(config[CONF_TX_PIN]))

