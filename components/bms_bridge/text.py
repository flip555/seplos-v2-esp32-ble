import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge"]

BmsCommandText = bms_bridge_ns.class_("BmsCommandText", text.Text)

CONFIG_SCHEMA = text.text_schema(BmsCommandText).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
    }
)


async def to_code(config):
    var = await text.new_text(config)
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(hub.register_command_text(var))
