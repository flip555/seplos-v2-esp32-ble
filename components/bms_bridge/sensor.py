import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_INDEX,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
)

from . import BmsBridge, CONF_BMS_BRIDGE_ID, bms_bridge_ns

DEPENDENCIES = ["bms_bridge"]

BmsSensor = bms_bridge_ns.class_("BmsSensor", sensor.Sensor)

# type -> (unit, decimals). Adapters emit values in these units.
TYPES = {
    "total_voltage": ("V", 2),
    "current": ("A", 2),
    "power": ("W", 1),
    "soc": ("%", 1),
    "soh": ("%", 1),
    "capacity": ("Ah", 2),
    "remaining_capacity": ("Ah", 2),
    "rated_capacity": ("Ah", 2),
    "cycles": ("", 0),
    "cell_voltage": ("V", 3),
    "cell_voltage_min": ("V", 3),
    "cell_voltage_max": ("V", 3),
    "cell_voltage_delta": ("V", 3),
    "temperature": ("°C", 1),
    "temperature_min": ("°C", 1),
    "temperature_max": ("°C", 1),
    "ambient_temperature": ("°C", 1),
    "power_temperature": ("°C", 1),
    "mos_temperature": ("°C", 1),
    "sys_state": ("", 0),
    "balancing_cells": ("", 0),
    "invalid_cells": ("", 0),
    "active_events": ("", 0),
    "cell_warning": ("", 0),
    "temp_warning": ("", 0),
    "port_voltage": ("V", 2),
    "full_charge": ("Ah", 1),
    "full_charge_watts": ("Wh", 0),
    "remaining_watts": ("Wh", 0),
    "capacity_watts": ("Wh", 0),
    "battery_watts": ("W", 0),
    "cell_max_number": ("", 0),
    "cell_min_number": ("", 0),
    "temp_max_number": ("", 0),
    "temp_min_number": ("", 0),
    "temp_delta": ("°C", 1),
    "setting_cell_ov_alarm": ("V", 3),
    "setting_cell_ov_rec": ("V", 3),
    "setting_cell_uv_alarm": ("V", 3),
    "setting_cell_uv_rec": ("V", 3),
    "setting_cell_ovp": ("V", 3),
    "setting_cell_ovp_rec": ("V", 3),
    "setting_cell_uvp": ("V", 3),
    "setting_cell_uvp_rec": ("V", 3),
    "setting_balance_start": ("V", 3),
    "setting_charge_forbid": ("V", 3),
    "setting_pack_ov_alarm": ("V", 2),
    "setting_pack_ov_rec": ("V", 2),
    "setting_pack_uv_alarm": ("V", 2),
    "setting_pack_uv_rec": ("V", 2),
    "setting_pack_ovp": ("V", 2),
    "setting_pack_ovp_rec": ("V", 2),
    "setting_pack_uvp": ("V", 2),
    "setting_pack_uvp_rec": ("V", 2),
    "setting_charge_ovp": ("V", 2),
    "setting_charge_ovp_rec": ("V", 2),
    "setting_charge_temp_high_warn": ("°C", 1),
    "setting_charge_temp_high_rec": ("°C", 1),
    "setting_charge_temp_low_warn": ("°C", 1),
    "setting_charge_temp_low_rec": ("°C", 1),
    "setting_charge_temp_high_prot": ("°C", 1),
    "setting_charge_temp_high_prot_rec": ("°C", 1),
    "setting_charge_temp_low_prot": ("°C", 1),
    "setting_charge_temp_low_prot_rec": ("°C", 1),
    "setting_discharge_temp_high_warn": ("°C", 1),
    "setting_discharge_temp_high_rec": ("°C", 1),
    "setting_discharge_temp_low_warn": ("°C", 1),
    "setting_discharge_temp_low_rec": ("°C", 1),
    "setting_discharge_temp_high_prot": ("°C", 1),
    "setting_discharge_temp_high_prot_rec": ("°C", 1),
    "setting_discharge_temp_low_prot": ("°C", 1),
    "setting_discharge_temp_low_prot_rec": ("°C", 1),
    "setting_cell_heat": ("°C", 1),
    "setting_cell_heat_rec": ("°C", 1),
}


def _validate(config):
    if config[CONF_TYPE] in ("cell_voltage", "temperature", "cell_warning", "temp_warning") and CONF_INDEX not in config:
        raise cv.Invalid(f"'{config[CONF_TYPE]}' requires 'index'")
    unit, decimals = TYPES[config[CONF_TYPE]]
    if unit:
        config[CONF_UNIT_OF_MEASUREMENT] = unit
    config[CONF_ACCURACY_DECIMALS] = decimals
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(BmsSensor).extend(
        {
            cv.Required(CONF_BMS_BRIDGE_ID): cv.use_id(BmsBridge),
            cv.Required(CONF_TYPE): cv.one_of(*TYPES, lower=True),
            cv.Optional(CONF_INDEX): cv.uint8_t,
        }
    ),
    _validate,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_index(config.get(CONF_INDEX, 0)))
    hub = await cg.get_variable(config[CONF_BMS_BRIDGE_ID])
    cg.add(hub.register_sensor(var))
