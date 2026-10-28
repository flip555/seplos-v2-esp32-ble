import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor", "ble_client", "esp32_ble_tracker", "switch", "number"]
# One hub per battery pack; several packs may share an ESP32.
MULTI_CONF = True

CONF_BMS_BRIDGE_ID = "bms_bridge_id"
CONF_TRANSPORT = "transport"
CONF_PROFILE = "profile"
CONF_BLE_CLIENT_ID = "ble_client_id"
CONF_ADDRESS = "address"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_ALLOW_WRITES = "allow_writes"
CONF_POLL_OFFSET = "poll_offset"
CONF_READ_SETTINGS = "read_settings_on_connect"
CONF_SETTINGS_INTERVAL = "settings_interval"
CONF_MAC_SLOT = "mac_slot"
CONF_AUTO_REBOOT = "auto_reboot"
CONF_AUTO_REBOOT_TIMEOUT = "auto_reboot_timeout"

bms_bridge_ns = cg.esphome_ns.namespace("bms_bridge")
BmsBridge = bms_bridge_ns.class_("BmsBridge", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BmsBridge),
        cv.Required(CONF_TRANSPORT): cv.one_of("ble", lower=True),
        cv.Required(CONF_PROFILE): cv.string,
        cv.Optional(CONF_BLE_CLIENT_ID): cv.use_id(ble_client.BLEClient),
        cv.Optional(CONF_ADDRESS, default=0x00): cv.hex_uint8_t,
        cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.update_interval,
        cv.Optional(CONF_ALLOW_WRITES, default=False): cv.boolean,
        cv.Optional(CONF_POLL_OFFSET, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_READ_SETTINGS, default=False): cv.boolean,
        cv.Optional(CONF_SETTINGS_INTERVAL, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAC_SLOT, default=1): cv.uint8_t,
        cv.Optional(CONF_AUTO_REBOOT, default=False): cv.boolean,
        cv.Optional(CONF_AUTO_REBOOT_TIMEOUT, default="15min"): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_transport(config[CONF_TRANSPORT]))
    cg.add(var.set_profile(config[CONF_PROFILE]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_update_interval_ms(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_allow_writes(config[CONF_ALLOW_WRITES]))
    cg.add(var.set_poll_offset(config[CONF_POLL_OFFSET].total_milliseconds))
    cg.add(var.set_read_settings_on_connect(config[CONF_READ_SETTINGS]))
    cg.add(var.set_settings_interval(config[CONF_SETTINGS_INTERVAL].total_milliseconds))
    cg.add(var.set_mac_slot(config[CONF_MAC_SLOT]))
    cg.add(var.set_auto_reboot(config[CONF_AUTO_REBOOT]))
    cg.add(var.set_auto_reboot_timeout(config[CONF_AUTO_REBOOT_TIMEOUT].total_milliseconds))

    if CONF_BLE_CLIENT_ID in config:
        client = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
        cg.add(var.set_ble_client(client))
