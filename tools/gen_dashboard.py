#!/usr/bin/env python3
"""Generate Home Assistant dashboard card templates from the example entity
names. Requires custom:button-card. Writes dashboards/*-packs.yaml.

Sections per pack: summary, status strip (system state, FETs, alarms, invalid
cells), temperature gauges, metrics grid, cell grid with min/max colouring and
a balancing highlight.
"""
import os

METRICS = [("State of Charge","soc"),("State of Health","soh"),("Cycles","cycles"),
           ("Pack Voltage","pack_voltage"),("Port Voltage","port_voltage"),("Cell Deviation","cell_delta"),
           ("Battery Watts","battery_power_port"),("Watts Remaining","remaining_energy"),("Watts Used","full_charge_remaining_wh"),
           ("Battery Amps","current"),("Ah Remaining","remaining_capacity"),("Ah Used","full_charge_remaining")]

TEMPS = [("Temp 1","temp_1"),("Temp 2","temp_2"),("Temp 3","temp_3"),
         ("Temp 4","temp_4"),("Ambient","environment_temperature"),("Power","power_temperature")]


def sid(dev, suffix):
    return f"sensor.{dev}_{suffix}"


def summary(dev):
    L=[]; a=L.append
    a("      - type: custom:button-card")
    a("        variables:")
    for k,s in [("soc","soc"),("voltage","pack_voltage"),("current","current"),("power","battery_power_port"),
                ("cycles","cycles"),("cell_min","min_cell"),("cell_max","max_cell"),("ah_used","full_charge_remaining")]:
        a(f"          {k}: {sid(dev, s)}")
    for i in range(1,5): a(f"          t{i}: {sid(dev, f'temp_{i}')}")
    a(f"          tamb: {sid(dev, 'environment_temperature')}")
    a(f"          tpow: {sid(dev, 'power_temperature')}")
    a("        show_name: false")
    a(f"        tap_action:\n          action: more-info\n          entity: {sid(dev, 'soc')}")
    a("        styles:")
    a("          card:")
    a("            - background-color: rgba(61,61,61,0.8)")
    a("            - color: ivory")
    a("            - font-size: 13px")
    a("            - text-shadow: 0px 0px 5px black")
    a("            - text-transform: capitalize")
    a("            - border-radius: 10px")
    for style in ("border","box-shadow"):
        a(f"            - {style}: |")
        a("                [[[")
        a("                  const s = states[variables.soc];")
        a("                  const c = s ? parseFloat(s.state) : NaN;")
        a("                  const col = isNaN(c) ? 'grey' : c > 75 ? 'lime' : c > 50 ? 'yellow' : c > 25 ? 'orange' : c > 15 ? 'red' : 'darkred';")
        a(f"                  return `{'1px solid' if style=='border' else '0px 0px 10px 3px'} ${{col}}`;")
        a("                ]]]")
    a("          grid:")
    a("            - grid-template-areas: '\"i bat cells\"'")
    a("            - grid-template-columns: 0fr 1fr 1fr")
    a("            - align-items: start")
    a("          icon:")
    a("            - color: |")
    a("                [[[")
    a("                  const s = states[variables.soc];")
    a("                  const c = s ? parseFloat(s.state) : NaN;")
    a("                  return isNaN(c) ? 'grey' : c > 75 ? 'lime' : c > 50 ? 'yellow' : c > 25 ? 'orange' : c > 15 ? 'red' : 'darkred';")
    a("                ]]]")
    a("            - width: 70%")
    a("            - margin-top: 0%")
    a("          custom_fields:")
    a("            bat:\n              - justify-content: start\n              - padding-left: 10px")
    a("            cells:\n              - justify-content: start\n              - padding-left: 10px")
    a("        custom_fields:")
    a("          bat: |")
    a("            [[[")
    a("              const num = (e, d) => {")
    a("                const s = states[e];")
    a("                if (!s || s.state === 'unknown' || s.state === 'unavailable') return '—';")
    a("                const n = parseFloat(s.state);")
    a("                return isNaN(n) ? '—' : n.toFixed(d);")
    a("              };")
    a("              return `")
    a("                <div><ha-icon icon=\"mdi:battery-high\" style=\"width:14px;height:14px;color:yellow\"></ha-icon><span style=\"margin-left:4px\">SOC: ${num(variables.soc, 1)}%</span></div>")
    a("                <div><ha-icon icon=\"mdi:flash\" style=\"width:14px;height:14px;color:green\"></ha-icon><span style=\"margin-left:4px\">Voltage: ${num(variables.voltage, 2)}V</span></div>")
    a("                <div><ha-icon icon=\"mdi:current-dc\" style=\"width:14px;height:14px;color:#3db7e4\"></ha-icon><span style=\"margin-left:4px\">Usage: ${num(variables.current, 2)}A/${num(variables.power, 0)}W</span></div>")
    a("                <div><ha-icon icon=\"mdi:power-plug\" style=\"width:14px;height:14px;color:white\"></ha-icon><span style=\"margin-left:4px\">Cycles: ${num(variables.cycles, 0)}</span></div>")
    a("              `;")
    a("            ]]]")
    a("          cells: |")
    a("            [[[")
    a("              const v = (e) => states[e] ? parseFloat(states[e].state) : NaN;")
    a("              const num = (e, d) => {")
    a("                const s = states[e];")
    a("                if (!s || s.state === 'unknown' || s.state === 'unavailable') return '—';")
    a("                const n = parseFloat(s.state);")
    a("                return isNaN(n) ? '—' : n.toFixed(d);")
    a("              };")
    a("              const ts = [variables.t1, variables.t2, variables.t3, variables.t4, variables.tamb, variables.tpow]")
    a("                .map(v).filter(x => !isNaN(x));")
    a("              const minT = ts.length ? Math.min(...ts).toFixed(1) : '—';")
    a("              const maxT = ts.length ? Math.max(...ts).toFixed(1) : '—';")
    a("              const cm = v(variables.cell_min), cx = v(variables.cell_max);")
    a("              const mv = (x) => isNaN(x) ? '—' : (x * 1000).toFixed(0);")
    a("              return `")
    a("                <div><ha-icon icon=\"mdi:arrow-down-bold\" style=\"width:14px;height:14px;color:#3db7e4\"></ha-icon><span style=\"margin-left:4px\">Lowest: ${mv(cm)}mV</span></div>")
    a("                <div><ha-icon icon=\"mdi:arrow-up-bold\" style=\"width:14px;height:14px;color:#f53d3d\"></ha-icon><span style=\"margin-left:4px\">Highest: ${mv(cx)}mV</span></div>")
    a("                <div><ha-icon icon=\"mdi:thermometer-lines\" style=\"width:14px;height:14px;color:#f5b041\"></ha-icon><span style=\"margin-left:4px\">Temp: ${minT}°C/${maxT}°C</span></div>")
    a("                <div><ha-icon icon=\"mdi:battery-charging-100\" style=\"width:14px;height:14px;color:#8c8c8c\"></ha-icon><span style=\"margin-left:4px\">Ah Used: ${num(variables.ah_used, 2)}Ah</span></div>")
    a("              `;")
    a("            ]]]")
    return L


def header(dev, title):
    L=[]; a=L.append
    a("      - type: custom:button-card")
    a(f"        entity: {sid(dev, 'pack_voltage')}")
    a(f"        variables:\n          title: \"{title}\"")
    a("        show_name: false\n        show_state: false\n        show_icon: false")
    a("        custom_fields:")
    a("          bat: |")
    a("            [[[")
    a("              if (!entity || !entity.last_updated) return 'no data';")
    a("              const t = new Date(entity.last_updated).getTime();")
    a("              const s = Math.max(0, Math.floor((Date.now() - t) / 1000));")
    a("              let rel;")
    a("              if (s < 60) rel = `${s}s ago`;")
    a("              else { const m = Math.floor(s / 60);")
    a("                if (m < 60) rel = `${m}m ${s % 60}s ago`;")
    a("                else { const h = Math.floor(m / 60);")
    a("                  if (h < 24) rel = `${h}h ${m % 60}m ago`;")
    a("                  else { const d = Math.floor(h / 24); rel = `${d}d ${h % 24}h ago`; } } }")
    a("              return `<span style=\"font-size:14px;font-weight:600\">${variables.title}</span> <span style=\"font-size:12px;color:var(--secondary-text-color)\">[ Updated ${rel} ]</span>`;")
    a("            ]]]")
    a("        styles:")
    a("          card:\n            - background: none\n            - border: none\n            - box-shadow: none\n            - padding: 0")
    a("          grid:\n            - grid-template-areas: '\"bat\"'\n            - grid-template-columns: 1fr")
    a("          custom_fields:\n            bat:\n              - justify-self: center\n              - text-align: left")
    return L


def status(dev):
    L=[]; a=L.append
    a("      - type: custom:button-card")
    a("        show_name: false\n        show_state: false\n        show_icon: false")
    a("        variables:")
    a(f"          state: text_sensor.{dev}_system_state")
    a(f"          alarms: text_sensor.{dev}_active_alarms")
    a(f"          charge: binary_sensor.{dev}_charge_switch")
    a(f"          discharge: binary_sensor.{dev}_discharge_switch")
    a(f"          bal: {sid(dev, 'balancing_cells')}")
    a(f"          inv: {sid(dev, 'invalid_cells')}")
    a("        styles:")
    a("          card:")
    a("            - background-color: rgba(61,61,61,0.8)")
    a("            - border-radius: 10px")
    a("            - padding: 8px 12px")
    a("            - font-size: 12px")
    a("        custom_fields:")
    a("          info: |")
    a("            [[[")
    a("              const st = (e) => states[e] ? states[e].state : '—';")
    a("              const s = st(variables.state);")
    a("              const on = (e) => st(e) === 'on';")
    a("              const c = on(variables.charge), d = on(variables.discharge);")
    a("              const bal = parseFloat(st(variables.bal));")
    a("              const inv = parseFloat(st(variables.inv));")
    a("              const alarms = st(variables.alarms);")
    a("              const good = (!alarms || alarms === 'none');")
    a("              const stIcon = s === 'Charge' ? 'mdi:battery-charging' : s === 'Discharge' ? 'mdi:battery-arrow-down' : s === 'Standby' ? 'mdi:battery-clock' : s === 'Float' ? 'mdi:battery-heart' : s === 'Fully charged' ? 'mdi:battery-check' : 'mdi:battery';")
    a("              const chip = (icon, text, col) => `<span style=\"display:inline-flex;align-items:center;margin-right:14px\"><ha-icon icon=\"${icon}\" style=\"width:15px;height:15px;color:${col}\"></ha-icon><span style=\"margin-left:3px\">${text}</span></span>`;")
    a("              return '<div style=\"display:flex;flex-wrap:wrap;gap:2px\">' +")
    a("                chip(stIcon, s, '#7fd1ff') +")
    a("                chip('mdi:alpha-c-box', c ? 'FET C on' : 'FET C off', c ? '#8bc34a' : '#ff5252') +")
    a("                chip('mdi:alpha-d-box', d ? 'FET D on' : 'FET D off', d ? '#8bc34a' : '#ff5252') +")
    a("                chip('mdi:scale-balance', isNaN(bal) ? 'bal —' : `bal ${bal}`, bal > 0 ? '#00e5ff' : '#9e9e9e') +")
    a("                chip('mdi:alert-circle-outline', isNaN(inv) ? 'inv —' : `inv ${inv}`, inv > 0 ? '#ff5252' : '#9e9e9e') +")
    a("                chip(good ? 'mdi:shield-check' : 'mdi:alert', good ? 'alarms: none' : alarms, good ? '#8bc34a' : '#ff5252') +")
    a("                '</div>';")
    a("            ]]]")
    return L


def gauges(dev):
    L=[]; a=L.append
    a("      - type: grid\n        square: false\n        columns: 3\n        cards:")
    for label, suf in TEMPS:
        a("          - type: gauge")
        a(f"            entity: {sid(dev, suf)}")
        a(f"            name: {label}")
        a("            needle: true")
        a("            min: 0")
        a("            max: 60")
        a("            severity:\n              yellow: 15\n              green: 40\n              red: 50")
    return L


def metrics(dev):
    L=[]; a=L.append
    a("      - type: grid\n        square: false\n        columns: 3\n        cards:")
    for name,suf in METRICS:
        a("          - type: custom:button-card")
        a(f"            entity: {sid(dev, suf)}")
        a(f"            name: \"{name}\"")
        a("            show_name: true\n            show_state: true\n            show_icon: false\n            show_units: true")
        a("            tap_action:\n              action: more-info")
        a("            styles:\n              card:\n                - \"--card-background-color\": null\n              name:\n                - font-size: 0.6em\n              state:\n                - font-size: 0.8em")
    return L


def cells(dev):
    L=[]; a=L.append
    a("      - type: grid\n        square: false\n        columns: 4\n        cards:")
    for i in range(1,17):
        e = sid(dev, f"cell_{i}")
        b = f"binary_sensor.{dev}_cell_{i}_balancing"
        a("          - type: custom:button-card")
        a(f"            entity: {e}")
        a("            name: |")
        a("              [[[")
        a(f"                const s = states['{e}'];")
        a(f"                const b = states['{b}'];")
        a(f"                const sl = states['{sid(dev, 'min_cell')}']; const sh = states['{sid(dev, 'max_cell')}'];")
        a(f"                if (!s) return 'Cell {i}';")
        a("                const v = parseFloat(s.state);")
        a("                const lv = sl ? parseFloat(sl.state) : NaN;")
        a("                const hv = sh ? parseFloat(sh.state) : NaN;")
        a(f"                let n = 'Cell {i}';")
        a("                if (b && b.state === 'on') n += ' ⚡';")
        a("                if (!isNaN(lv) && v === lv) n += ' (L)';")
        a("                if (!isNaN(hv) && v === hv) n += ' (H)';")
        a("                return n;")
        a("              ]]]")
        a("            state_display: >")
        a("              [[[ const v = parseFloat(entity && entity.state); return isNaN(v)")
        a("              ? '—' : (v * 1000).toFixed(0) + ' mV'; ]]]")
        a("            show_name: true\n            show_state: true\n            show_icon: false")
        a("            tap_action:\n              action: more-info")
        a("            styles:")
        a("              card:\n                - \"--card-background-color\": |")
        a("                    [[[")
        a(f"                      const s = states['{e}'];")
        a(f"                      const sl = states['{sid(dev, 'min_cell')}']; const sh = states['{sid(dev, 'max_cell')}'];")
        a("                      if (!s) return 'grey';")
        a("                      const v = parseFloat(s.state);")
        a("                      const lv = sl ? parseFloat(sl.state) : NaN;")
        a("                      const hv = sh ? parseFloat(sh.state) : NaN;")
        a("                      return (!isNaN(lv) && v === lv) ? '#f9a825' : (!isNaN(hv) && v === hv) ? '#1b5e20' : '#2e7d32';")
        a("                    ]]]")
        a("                - color: |")
        a("                    [[[")
        a(f"                      const s = states['{e}'];")
        a(f"                      const sl = states['{sid(dev, 'min_cell')}'];")
        a("                      if (!s) return 'white';")
        a("                      const v = parseFloat(s.state);")
        a("                      const lv = sl ? parseFloat(sl.state) : NaN;")
        a("                      return (!isNaN(lv) && v === lv) ? '#1a1a1a' : 'white';")
        a("                    ]]]")
        a("                - border: |")
        a("                    [[[")
        a(f"                      const b = states['{b}'];")
        a("                      return (b && b.state === 'on') ? '3px solid #00e5ff' : '1px solid rgba(255,255,255,0.12)';")
        a("                    ]]]")
        a("                - box-shadow: |")
        a("                    [[[")
        a(f"                      const b = states['{b}'];")
        a("                      return (b && b.state === 'on') ? '0px 0px 8px 2px #00e5ff' : 'none';")
        a("                    ]]]")
        a("              name:\n                - font-size: 0.6em\n              state:\n                - font-size: 0.7em")
    return L


def pack_card(dev, title):
    L = ["  - type: vertical-stack", "    cards:"]
    L += summary(dev)
    L += header(dev, title)
    L += status(dev)
    L += gauges(dev)
    L += metrics(dev)
    L += cells(dev)
    return "\n".join(L) + "\n"


def gen(packs):
    L = []
    a = L.append
    a("# Requires custom:button-card. Paste into a dashboard as one card.")
    a("# Entity ids assume the example naming: device pack1_live, entity 'SOC'")
    a("# -> sensor.pack1_live_soc. Per-cell balancing uses")
    a("# binary_sensor.pack1_live_cell_1_balancing.")
    a("type: grid")
    a("square: false")
    a(f"columns: {packs}")
    a("cards:")
    for i in range(1, packs + 1):
        L.append(pack_card(f"pack{i}_live", f"Pack {i}"))
    return "\n".join(L) + "\n"


if __name__ == "__main__":
    names = {1: "one-pack.yaml", 2: "two-packs.yaml", 3: "three-packs.yaml", 4: "four-packs.yaml"}
    for n, fn in names.items():
        open(os.path.join("dashboards", fn), "w").write(gen(n))
        print("dashboards/" + fn)
