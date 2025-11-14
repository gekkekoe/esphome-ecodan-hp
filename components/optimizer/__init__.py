import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

optimizer_ns = cg.esphome_ns.namespace("optimizer")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(cg.Component), 
    }
)

async def to_code(config):
    cg.add_global(optimizer_ns.using)
    pass