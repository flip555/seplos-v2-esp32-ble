#!/usr/bin/env python3
"""Generate the example configurations from the component's type tables.

Usage: python3 tools/gen_examples.py
Writes examples/one-pack.yaml ... examples/four-packs.yaml.
"""
import os

# Keep in sync with components/bms_bridge/sensor.py
BASE = [
    ("total_voltage","Pack voltage"),("current","Current"),("power","Power"),
    ("soc","SOC"),("soh","SOH"),("cycles","Cycles"),("capacity","Capacity"),
    ("remaining_capacity","Remaining capacity"),("rated_capacity","Rated capacity"),
    ("port_voltage","Port voltage"),
    ("cell_voltage_min","Min cell"),("cell_voltage_max","Max cell"),("cell_voltage_delta","Cell delta"),
    ("temperature_min","Min temp"),("temperature_max","Max temp"),
    ("ambient_temperature","Ambient temp"),("power_temperature","Power temp"),
    ("balancing_cells","Balancing cells"),("broken_cells","Broken cells"),
    ("active_events","Active events"),("sys_state","System state code"),
    ("full_charge","Full charge"),("full_charge_watts","Full charge (Wh)"),
    ("remaining_watts","Remaining (Wh)"),("capacity_watts","Capacity (Wh)"),("battery_watts","Battery power"),
    ("cell_max_number","Max cell #"),("cell_min_number","Min cell #"),
    ("temp_max_number","Max temp #"),("temp_min_number","Min temp #"),("temp_delta","Temp spread"),
]
SETTINGS = [
    ("setting_cell_ov_alarm","Cell OV alarm"),("setting_cell_ov_rec","Cell OV recovery"),
    ("setting_cell_uv_alarm","Cell UV alarm"),("setting_cell_uv_rec","Cell UV recovery"),
    ("setting_cell_ovp","Cell OVP"),("setting_cell_ovp_rec","Cell OVP recovery"),
    ("setting_cell_uvp","Cell UVP"),("setting_cell_uvp_rec","Cell UVP recovery"),
    ("setting_balance_start","Balance start"),("setting_charge_forbid","Charge forbidden below"),
    ("setting_pack_ov_alarm","Pack OV alarm"),("setting_pack_ov_rec","Pack OV recovery"),
    ("setting_pack_uv_alarm","Pack UV alarm"),("setting_pack_uv_rec","Pack UV recovery"),
    ("setting_pack_ovp","Pack OVP"),("setting_pack_ovp_rec","Pack OVP recovery"),
    ("setting_pack_uvp","Pack UVP"),("setting_pack_uvp_rec","Pack UVP recovery"),
    ("setting_charge_ovp","Charging OVP"),("setting_charge_ovp_rec","Charging OVP recovery"),
    ("setting_charge_temp_high_warn","Charge temp high warn"),("setting_charge_temp_high_rec","Charge temp high rec"),
    ("setting_charge_temp_low_warn","Charge temp low warn"),("setting_charge_temp_low_rec","Charge temp low rec"),
    ("setting_charge_temp_high_prot","Charge temp high prot"),("setting_charge_temp_high_prot_rec","Charge temp high prot rec"),
    ("setting_charge_temp_low_prot","Charge temp low prot"),("setting_charge_temp_low_prot_rec","Charge temp low prot rec"),
    ("setting_discharge_temp_high_warn","Discharge temp high warn"),("setting_discharge_temp_high_rec","Discharge temp high rec"),
    ("setting_discharge_temp_low_warn","Discharge temp low warn"),("setting_discharge_temp_low_rec","Discharge temp low rec"),
    ("setting_discharge_temp_high_prot","Discharge temp high prot"),("setting_discharge_temp_high_prot_rec","Discharge temp high prot rec"),
    ("setting_discharge_temp_low_prot","Discharge temp low prot"),("setting_discharge_temp_low_prot_rec","Discharge temp low prot rec"),
    ("setting_cell_heat","Cell heating"),("setting_cell_heat_rec","Cell heating recovery"),
]
TEXTS = ["profile","model","hardware_version","software_version","status","settings_raw",
         "alarms_raw","status_tail","crc_capture","scan_results","output","system_state"]
BINARIES = [("charge_switch","Charge switch"),("discharge_switch","Discharge switch"),
            ("balancing","Balancing"),("alarm","Alarm")]

def mac(n): return "60:6E:41:00:00:%02X" % n

def gen(n):
    L=[]; a=L.append
    a("# %d pack%s. Replace the MACs, the API key and the WiFi secrets." % (n, "" if n==1 else "s"))
    a("substitutions:\n  name: seplos-bms\n  friendly_name: Seplos BMS\n")
    a("esphome:\n  name: ${name}\n  friendly_name: ${friendly_name}\n  devices:")
    for i in range(1,n+1):
        a(f'    - id: pack{i}_live\n      name: "Pack {i} Live"')
        a(f'    - id: pack{i}_settings\n      name: "Pack {i} Settings"')
        a(f'    - id: pack{i}_diag\n      name: "Pack {i} Diagnostics"')
    a("")
    a("external_components:\n  - source: github://flip555/seplos-v2-esp32-ble@main\n    components: [bms_bridge]\n")
    a("esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf\n")
    if n > 3:
        a(f"esp32_ble:\n  max_connections: {n}\n")
    a("logger:\n  level: INFO\n  logs:\n    esp32_ble_client: WARN\n    bms_bridge.ble: WARN\n")
    a('api:\n  encryption:\n    key: "REPLACE_WITH_A_BASE64_KEY"\n')
    a("ota:\n  - platform: esphome\n")
    a("wifi:\n  ssid: !secret wifi_ssid\n  password: !secret wifi_password\n")
    a("esp32_ble_tracker:\n  scan_parameters:\n    interval: 320ms\n    window: 30ms\n  on_ble_advertise:\n    - lambda: |-")
    for i in range(1,n+1):
        a(f"        id(hub{i})->note_advert(x.address_str(), x.get_rssi(), x.get_name(), (int) x.get_service_uuids().size());")
    a("")
    a("ble_client:")
    for i in range(1,n+1):
        a(f'  - mac_address: "{mac(i)}"\n    id: pack{i}_client')
    a("")
    a("bms_bridge:")
    interval = 12 if n >= 3 else 10
    step = max(1, interval // n)
    for i in range(1,n+1):
        a(f"  - id: hub{i}\n    transport: ble\n    ble_client_id: pack{i}_client\n    profile: seplos_v2_ble\n    address: 0x00\n    update_interval: {interval}s")
        if i>1: a(f"    poll_offset: {(i-1)*step}s")
    a("")
    a("sensor:")
    for i in range(1,n+1):
        def s(t,label,idx=None,dev="live"):
            a(f"  - platform: bms_bridge\n    bms_bridge_id: hub{i}\n    device_id: pack{i}_{dev}\n    type: {t}")
            if idx is not None: a(f"    index: {idx}")
            a(f'    name: "{label}"')
        for t,nm in BASE: s(t,nm)
        for c in range(1,17): s("cell_voltage", f"Cell {c}", c)
        for c in range(1,5): s("temperature", f"Temp {c}", c)
        for t,nm in SETTINGS: s(t,nm,dev="settings")
    a("")
    a("binary_sensor:")
    for i in range(1,n+1):
        for t,nm in BINARIES:
            a(f'  - platform: bms_bridge\n    bms_bridge_id: hub{i}\n    device_id: pack{i}_live\n    type: {t}\n    name: "{nm}"')
    a("")
    a("text_sensor:")
    for i in range(1,n+1):
        for t in TEXTS:
            dev="settings" if t in ("settings_raw","alarms_raw") else "diag"
            a(f'  - platform: bms_bridge\n    bms_bridge_id: hub{i}\n    device_id: pack{i}_{dev}\n    type: {t}\n    name: "{t.replace("_"," ").capitalize()}"')
    a("")
    a("button:")
    for i in range(1,n+1):
        for t,nm in (("scan","Scan"),("settings","Read settings"),("alarms","Read alarms"),("run","Run command")):
            a(f'  - platform: bms_bridge\n    bms_bridge_id: hub{i}\n    device_id: pack{i}_diag\n    type: {t}\n    name: "{nm}"')
    a("")
    a("text:")
    for i in range(1,n+1):
        a(f'  - platform: bms_bridge\n    bms_bridge_id: hub{i}\n    device_id: pack{i}_diag\n    mode: text\n    name: "BLE command"')
    return "\n".join(L)+"\n"

if __name__ == "__main__":
    names={1:"one-pack.yaml",2:"two-packs.yaml",3:"three-packs.yaml",4:"four-packs.yaml"}
    for n,fn in names.items():
        open(os.path.join("examples",fn),"w").write(gen(n))
        print(fn, "written")
