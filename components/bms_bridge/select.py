import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge", "select"]

BmsSelect = bms_bridge_ns.class_("BmsSelect", select.Select)
CONF_TYPE = "type"

# Order matters: 0x51 reports 1..6 and 0x63 expects 2 x that value.
CAN_PROTOCOLS = ["Pylontech", "Growatt", "Victron", "SMA", "GINL", "Studer"]

CONFIG_SCHEMA = select.select_schema(BmsSelect).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Optional(CONF_TYPE, default="can_protocol"): cv.one_of("can_protocol", lower=True),
    }
)


async def to_code(config):
    var = await select.new_select(config, options=CAN_PROTOCOLS)
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(hub.register_select(var))
