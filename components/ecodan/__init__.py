import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome import pins
from esphome.const import CONF_ID

CODEOWNERS = ["@gekkekoe"]

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor", "uart"]

CONF_ECODAN_ID = "ecodan_id"

hub_ns = cg.esphome_ns.namespace('ecodan')

ECODAN = hub_ns.class_('EcodanHeatpump', cg.PollingComponent)
ECODAN_CLIMATE = hub_ns.class_('EcodanClimate', climate.Climate, cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ECODAN),
    }
    ).extend(cv.polling_component_schema('1000ms')
    .extend(uart.UART_DEVICE_SCHEMA))


async def to_code(config):
    hp = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hp, config)
    await uart.register_uart_device(hp, config)
