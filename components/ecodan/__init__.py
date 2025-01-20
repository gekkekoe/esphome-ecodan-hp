import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uart
from esphome import pins
from esphome.const import CONF_ID

CODEOWNERS = ["@gekkekoe"]

AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor", "uart"]

CONF_ECODAN_ID = "ecodan_id"
CONF_PROXY_UART_ID = "proxy_uart_id"
CONF_SPECIFIC_HEAT_CONSTANT = "specific_heat_constant_override"
CONF_POLLING_INTERVAL_OVERRIDE = "polling_interval_override"

uart_ns = cg.esphome_ns.namespace("uart")
UARTComponent = uart_ns.class_("UARTComponent")

hub_ns = cg.esphome_ns.namespace('ecodan')

ECODAN = hub_ns.class_('EcodanHeatpump', cg.PollingComponent)
ECODAN_CLIMATE = hub_ns.class_('EcodanClimate', climate.Climate, cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ECODAN),
        cv.Optional(CONF_PROXY_UART_ID): cv.use_id(UARTComponent),
        cv.Optional(CONF_SPECIFIC_HEAT_CONSTANT): cv.float_,
        cv.Optional(CONF_POLLING_INTERVAL_OVERRIDE): cv.uint32_t,
    }
    ).extend(cv.polling_component_schema('500ms')
    .extend(uart.UART_DEVICE_SCHEMA))


async def to_code(config):
    hp = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(hp, config)
    await uart.register_uart_device(hp, config)
    if proxy_uart_id := config.get(CONF_PROXY_UART_ID):
        proxy_uart = await cg.get_variable(proxy_uart_id)
        cg.add(hp.set_proxy_uart(proxy_uart))
    if CONF_SPECIFIC_HEAT_CONSTANT in config:
        cg.add(hp.set_specific_heat_constant(config[CONF_SPECIFIC_HEAT_CONSTANT]))
    if CONF_POLLING_INTERVAL_OVERRIDE in config:
        cg.add(hp.set_polling_interval(config[CONF_POLLING_INTERVAL_OVERRIDE]))
