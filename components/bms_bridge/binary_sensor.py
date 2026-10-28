import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_INDEX, CONF_TYPE

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge"]

BmsBinarySensor = bms_bridge_ns.class_("BmsBinarySensor", binary_sensor.BinarySensor)

# charge_switch / discharge_switch come from the 0x61 status-tail switch byte.
TYPES = ["charge_switch", "discharge_switch", "balancing", "alarm", "cell_balancing"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(BmsBinarySensor).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
        cv.Optional(CONF_INDEX): cv.uint8_t,
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_index(config.get(CONF_INDEX, 0)))
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(hub.register_binary_sensor(var))
