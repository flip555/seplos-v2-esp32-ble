import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge"]

BmsButton = bms_bridge_ns.class_("BmsButton", button.Button)

CONF_TYPE = "type"

CONFIG_SCHEMA = button.button_schema(BmsButton).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Optional(CONF_TYPE, default="scan"): cv.one_of("scan", "run", "settings", "alarms", lower=True),
    }
)

MODE_IDS = {"scan": 0, "run": 1, "settings": 2, "alarms": 3}


async def to_code(config):
    var = await button.new_button(config)
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_mode(MODE_IDS[config[CONF_TYPE]]))
