import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge", "number"]

BmsNumber = bms_bridge_ns.class_("BmsNumber", number.Number)
CONF_TYPE = "type"
CONF_MIN = "min_value"
CONF_MAX = "max_value"
CONF_STEP = "step"

CONFIG_SCHEMA = number.number_schema(BmsNumber).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Optional(CONF_TYPE, default="auto_reboot_timeout"): cv.one_of("auto_reboot_timeout", lower=True),
        cv.Optional(CONF_MIN, default=5): cv.float_,
        cv.Optional(CONF_MAX, default=120): cv.float_,
        cv.Optional(CONF_STEP, default=5): cv.float_,
    }
)


async def to_code(config):
    var = await number.new_number(config, min_value=config[CONF_MIN], max_value=config[CONF_MAX],
                                  step=config[CONF_STEP])
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(var.set_hub(hub))
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(hub.register_number(var))
