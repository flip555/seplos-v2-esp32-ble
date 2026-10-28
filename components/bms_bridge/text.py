import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge"]

BmsCommandText = bms_bridge_ns.class_("BmsCommandText", text.Text)

CONF_TYPE = "type"

CONFIG_SCHEMA = text.text_schema(BmsCommandText).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Optional(CONF_TYPE, default="command"): cv.one_of("command", "mac", lower=True),
    }
)


async def to_code(config):
    var = await text.new_text(config)
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    if config[CONF_TYPE] == "mac":
        cg.add(var.set_hub(hub))
        cg.add(var.set_mac_mode(True))
        cg.add(hub.register_mac_text(var))
    else:
        cg.add(hub.register_command_text(var))
