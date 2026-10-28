import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge", "switch"]

BmsSwitch = bms_bridge_ns.class_("BmsSwitch", switch.Switch)
CONF_TYPE = "type"

CONFIG_SCHEMA = switch.switch_schema(BmsSwitch).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Optional(CONF_TYPE, default="auto_reboot"): cv.one_of("auto_reboot", lower=True),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(hub.register_switch(var))
