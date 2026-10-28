import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_TYPE

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge"]

BmsTextSensor = bms_bridge_ns.class_("BmsTextSensor", text_sensor.TextSensor)

TYPES = ["profile", "model", "hardware_version", "software_version", "status",
         "scan_results", "output", "settings_raw", "alarms_raw", "status_tail", "crc_capture"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema(BmsTextSensor).extend(
    {
        cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
        cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    cg.add(var.set_type(config[CONF_TYPE]))
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(hub.register_text_sensor(var))
