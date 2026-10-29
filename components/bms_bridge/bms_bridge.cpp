#include "bms_bridge.h"

#include "ble_transport.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace esphome {
namespace bms_bridge {

static const char *const TAG = "bms_bridge";
static const uint32_t HEARTBEAT_MS = 300000;
static const uint32_t STALL_MS = 60000;
static const uint32_t STALL_RECOVER_MS = 3000;  // republish unchanged values every 5 min
static const uint32_t STALL_HARD_MS = 30000;    // repeated stalls get a longer disconnect

// Last time any hub received bytes; used by the optional auto-reboot so a
// single dead pack can never trigger it.
static uint32_t g_last_frame_ms = 0;
static uint32_t g_good_since_ms = 0;
static bool g_reboot_pending = false;
static uint8_t g_active_hubs = 0;
static uint32_t g_slots[8] = {0};
static uint8_t g_slot_count = 0;

// "AA:BB:CC:DD:EE:FF" -> 0xAABBCCDDEEFF (the byte order ESPHome stores).
static bool parse_mac(const std::string &s, uint64_t &out) {
  unsigned b[6];
  if (sscanf(s.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
    return false;
  out = ((uint64_t) b[0] << 40) | ((uint64_t) b[1] << 32) | ((uint64_t) b[2] << 24) | ((uint64_t) b[3] << 16) |
        ((uint64_t) b[4] << 8) | (uint64_t) b[5];
  return out != 0;
}


// Extract CID2 from a hex frame: 7E len_hi len_lo addr cid2 ...
static int frame_cid2(const std::string &hex) {
  uint8_t b[8];
  int n = 0;
  for (size_t i = 0; i < hex.size() && n < 8;) {
    if (hex[i] == ' ') {
      i++;
      continue;
    }
    if (i + 1 >= hex.size()) return -1;
    char t[3] = {hex[i], hex[i + 1], 0};
    char *end = nullptr;
    long v = strtol(t, &end, 16);
    if (end != t + 2) return -1;
    b[n++] = (uint8_t) v;
    i += 2;
  }
  if (n < 5 || b[0] != 0x7E) return -1;
  return b[4];
}

static std::string format_mac(uint64_t mac) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned) ((mac >> 40) & 0xFF),
           (unsigned) ((mac >> 32) & 0xFF), (unsigned) ((mac >> 24) & 0xFF), (unsigned) ((mac >> 16) & 0xFF),
           (unsigned) ((mac >> 8) & 0xFF), (unsigned) (mac & 0xFF));
  return std::string(b);
}

float PackInfo::cell_min() const {
  float v = NAN;
  for (float c : cell_voltages) v = std::isnan(v) ? c : std::min(v, c);
  return v;
}
float PackInfo::cell_max() const {
  float v = NAN;
  for (float c : cell_voltages) v = std::isnan(v) ? c : std::max(v, c);
  return v;
}
float PackInfo::cell_delta() const { return this->cell_max() - this->cell_min(); }
float PackInfo::temp_min() const {
  float v = NAN;
  for (float t : temperatures) v = std::isnan(v) ? t : std::min(v, t);
  return v;
}
float PackInfo::temp_max() const {
  float v = NAN;
  for (float t : temperatures) v = std::isnan(v) ? t : std::max(v, t);
  return v;
}

void BmsBridge::setup() {
  if (profile_name_ != "auto") {
    protocol_ = create_protocol(profile_name_, address_);
    if (protocol_ == nullptr) ESP_LOGE(TAG, "unknown profile '%s'", profile_name_.c_str());
  }

  if (transport_ == "ble" && ble_client_ != nullptr) {
    ble_transport_ = new BleTransport();
    ble_transport_->set_hub(this);
    // register_ble_node() also sets the parent and, crucially, adds us to the
    // client's node list so gattc events reach our handler.
    ble_client_->register_ble_node(ble_transport_);
  }
  if (transport_ == "ble") {
    scan_ = new BleScanListener();
    // Discovery is done by the YAML `esp32_ble_tracker: on_ble_advertise:` hook.
    // Registering a listener here proved unstable across ESPHome versions.
    if (ble_client_ == nullptr)
      ESP_LOGI(TAG, "no ble_client configured: scan-only mode, see /bms/api/scan");
  }


  // Collect advertisements for 30 s after boot so the scan text sensor is
  // useful without pressing anything.
  g_last_frame_ms = millis();
  this->load_saved_mac_();
  this->load_cfg_nvs_();

  if (ble_client_ != nullptr && ble_client_->get_address() != 0) g_active_hubs++;
  bool slot_seen = false;
  for (uint8_t i = 0; i < g_slot_count; i++)
    if (g_slots[i] == mac_slot_) slot_seen = true;
  if (slot_seen) {
    ESP_LOGE(TAG, "duplicate mac_slot %u: give every hub a unique mac_slot", (unsigned) mac_slot_);
  } else if (g_slot_count < 8) {
    g_slots[g_slot_count++] = mac_slot_;
  }

  // Collect advertisements for 30 s after boot so the scan text sensor is
  this->start_scan(30000);
  start_at_ = millis() + poll_offset_;

  ESP_LOGCONFIG(TAG, "BMS Bridge: transport=%s profile=%s address=0x%02X", transport_.c_str(),
                protocol_ != nullptr ? protocol_->name() : "auto", address_);
}

void BmsBridge::loop() {
  if (millis() - last_diag_ > 30000) {
    last_diag_ = millis();
    ESP_LOGI(TAG, "hub %s: chunks=%u bytes=%u frames=%u buf=%u crc=%u drops=%u valid=%d",
             profile_name_.c_str(), (unsigned) rx_chunks_, (unsigned) rx_bytes_,
             (unsigned) (protocol_ != nullptr ? protocol_->frames() : 0),
             (unsigned) (protocol_ != nullptr ? protocol_->buf_size() : 0),
             (unsigned) (protocol_ != nullptr ? protocol_->crc_fail() : 0),
             (unsigned) (protocol_ != nullptr ? protocol_->drops() : 0),
             (protocol_ != nullptr && protocol_->info().valid) ? 1 : 0);
    if (protocol_ != nullptr && protocol_->frames() == 0 && protocol_->buf_size() > 0)
      ESP_LOGI(TAG, "  buf peek: %s", protocol_->buf_peek().c_str());
  }
  if (!pending_out_.empty()) {
    for (auto *t : text_sensors_)
      if (t->type() == "output") t->publish_state(pending_out_);
    pending_out_.clear();
  }
  // Stall recovery: the link says connected but no frames arrive. Some BMS
  // Bluetooth modules stop notifying after a short exchange; cycling the
  // client recovers them (equivalent to toggling Bluetooth on the pack).
  if (ble_client_ != nullptr && rx_since_enable_ && !stall_recovering_ && !ble_backoff_ &&
      (millis() - last_rx_) > STALL_MS) {
    stall_recovering_ = true;
    stall_count_++;
    ESP_LOGW(TAG, "no frames for %us, cycling BLE (stall #%u)", (unsigned) ((millis() - last_rx_) / 1000),
             (unsigned) stall_count_);
    ble_client_->set_enabled(false);
    // After repeated stalls, hold the link down longer; toggling too quickly
    // leaves some modules in the same wedged state.
    uint32_t recover_ms = stall_count_ >= 3 ? STALL_HARD_MS : STALL_RECOVER_MS;
    this->set_timeout(recover_ms, [this]() {
      if (ble_client_ != nullptr) ble_client_->set_enabled(true);
      stall_recovering_ = false;
      rx_since_enable_ = false;
      last_rx_ = millis();
    });
  }
  // Optional last resort: reboot only when every hub has been silent for the
  // configured window, and at most 3 times without an hour of recovery. A
  // single offline pack keeps the other hub's frames flowing, so it cannot
  // trigger this.
  if (auto_reboot_ && !g_reboot_pending && ble_client_ != nullptr && g_active_hubs > 0 &&
      g_last_frame_ms != 0 && millis() > 1200000 && (millis() - g_last_frame_ms) > auto_reboot_timeout_ms_) {
    uint32_t key = fnv1a_hash_extend(2166136261u, "bms_auto_reboot_count");
    auto pref = global_preferences->make_preference<uint8_t>(key, true);
    uint8_t count = 0;
    pref.load(&count);
    if (count >= 3) {
      if (!auto_reboot_logged_) {
        auto_reboot_logged_ = true;
        ESP_LOGW(TAG, "auto-reboot suppressed after %u reboots; press Restart ESP if needed",
                 (unsigned) count);
      }
    } else {
      count++;
      pref.save(&count);
      g_reboot_pending = true;
      ESP_LOGE(TAG, "no frames from any hub for %us: rebooting (auto #%u)",
               (unsigned) ((millis() - g_last_frame_ms) / 1000), (unsigned) count);
      App.reboot();
    }
  }
  // A full hour of live frames clears the boot-loop guard.
  if (auto_reboot_ && g_good_since_ms != 0 && (millis() - g_good_since_ms) > 3600000) {
    uint32_t key = fnv1a_hash_extend(2166136261u, "bms_auto_reboot_count");
    auto pref = global_preferences->make_preference<uint8_t>(key, true);
    uint8_t count = 0;
    pref.load(&count);
    if (count != 0) {
      count = 0;
      pref.save(&count);
      auto_reboot_logged_ = false;
    }
  }

  // Publish any non-telemetry response (settings/alarms) for read-only mapping.
  if (protocol_ != nullptr) {
    std::string raw = protocol_->last_raw_hex();
    if (!raw.empty() && raw != last_raw_published_) {
      last_raw_published_ = raw;
      uint8_t cid = protocol_->last_cid();
      for (auto *t : text_sensors_) {
        if ((cid == 0x47 && t->type() == "settings_raw") || (cid == 0x44 && t->type() == "alarms_raw"))
          t->publish_state(raw);
      }
      ESP_LOGI(TAG, "cid 0x%02X: %s", cid, raw.c_str());
      if (cid == 0x47) force_full_ = true;
    }
    std::string cap = protocol_->capture();
    if (!cap.empty() && cap != last_capture_) {
      last_capture_ = cap;
      for (auto *t : text_sensors_)
        if (t->type() == "crc_capture") t->publish_state(cap);
    }
    std::string tail = protocol_->status_tail_hex();
    if (!tail.empty() && tail != last_tail_published_) {
      last_tail_published_ = tail;
      for (auto *t : text_sensors_)
        if (t->type() == "status_tail") t->publish_state(tail);
    }
  }

  // Watchdog: the client can retry at link layer without ever emitting a
  // DISCONNECT event (open errors), which would otherwise spin forever.
  if (!connected_ && !ble_backoff_ && ble_client_ != nullptr && (millis() - last_conn_check_) > 30000) {
    last_conn_check_ = millis();
    ble_backoff_ = true;
    ESP_LOGW(TAG, "no connection for 30 s, pausing BLE 90 s");
    ble_client_->set_enabled(false);
    this->set_timeout(90000, [this]() {
      ble_backoff_ = false;
      ble_fail_count_ = 0;
      if (ble_client_ != nullptr) ble_client_->set_enabled(true);
      rx_since_enable_ = false;
      last_rx_ = millis();
      // Start the 30 s window now; otherwise the check fires immediately
      // (it was stamped before the 90 s pause) and disables the client
      // again ~10 ms after reopening.
      last_conn_check_ = millis();
      ESP_LOGI(TAG, "BLE retry window reopened");
    });
  }

  if (scanning_ && (millis() - scan_started_) >= scan_window_) {
    scanning_ = false;
    std::string summary = "none";
    if (!adv_.empty()) {
      std::vector<AdvHit> hits;
      for (auto &kv : adv_) hits.push_back(kv.second);
      std::sort(hits.begin(), hits.end(), [](const AdvHit &a, const AdvHit &b) { return a.rssi > b.rssi; });
      summary.clear();
      size_t n = 0;
      for (const auto &h : hits) {
        if (n++ >= 6) break;
        char b[96];
        snprintf(b, sizeof(b), "%s%s %d %s", summary.empty() ? "" : "; ", h.mac.c_str(), h.rssi, h.name.c_str());
        summary += b;
        if (summary.size() > 200) break;
      }
    }
    ESP_LOGI(TAG, "scan complete (%u devices): %s", (unsigned) adv_.size(), summary.c_str());
    for (auto *t : text_sensors_)
      if (t->type() == "scan_results") t->publish_state(summary);
  }

  if (protocol_ == nullptr) return;
  uint32_t now = millis();

  // Publish when new frames arrive, not on a timer. Publishing immediately
  // after sending a poll competes with the reply burst for the main loop and
  // the BLE event queue (88 slots).
  uint32_t frames = protocol_->frames();
  bool fresh = last_rx_ != 0 && (now - last_rx_) < update_interval_ms_ * 3;
  if (frames != last_frames_) {
    last_frames_ = frames;
    this->publish_(false);
    last_heartbeat_ = now;
  } else if (force_full_) {
    force_full_ = false;
    this->publish_(true);
    last_heartbeat_ = now;
  } else if (fresh && (now - last_heartbeat_) > HEARTBEAT_MS) {
    // Slow heartbeat so a stable pack still looks alive in history.
    last_heartbeat_ = now;
    this->publish_(true);
  } else if (!fresh && protocol_->info().valid) {
    // Link quiet: make it visibly unavailable rather than silently stale.
    this->publish_stale_();
  }

  // Stagger the first poll of each hub so two packs do not answer together.
  if ((int32_t) (now - start_at_) < 0) return;
  bool link_alive = connected_ || fresh;
  if (!link_alive) return;
  // A manual/automatic request owns the link for a moment so its reply cannot
  // collide with a scheduled poll.
  if ((int32_t) (now - next_poll_allowed_) < 0) return;
  if (settings_interval_ms_ > 0 && connected_ && now - last_settings_ >= settings_interval_ms_) {
    last_settings_ = now;
    this->request_cid(0x47);
    return;
  }
  if (now - last_poll_ < update_interval_ms_) return;
  last_poll_ = now;

  // Serialise requests: some modules wedge if the next poll is written before
  // the previous reply arrives. Send the first, then the second after a gap.
  // The pair owns the link until the second request has gone out, so a manual
  // or interval settings read cannot be injected between them.
  auto reqs = protocol_->poll_requests();
  if (!reqs.empty()) this->send_bytes(reqs[0]);
  next_poll_allowed_ = now + 3000;
  if (reqs.size() > 1) {
    auto second = reqs[1];
    this->set_timeout(1500, [this, second]() {
      if (ble_client_ != nullptr && protocol_ != nullptr && !request_active_) this->send_bytes(second);
    });
  }
}

void BmsBridge::on_bytes(const uint8_t *data, size_t len) {
  uint32_t now = millis();
  last_rx_ = now;
  if (g_last_frame_ms == 0 || (now - g_last_frame_ms) > 5000) g_good_since_ms = now;
  g_last_frame_ms = now;
  rx_since_enable_ = true;
  rx_chunks_++;
  rx_bytes_ += len;
  if (len > 0 && data[0] == 0x7E && start_samples_ < 6) {
    start_samples_++;
    char b[4];
    std::string h;
    for (size_t i = 0; i < len && i < 32; i++) { snprintf(b, sizeof(b), "%02X ", data[i]); h += b; }
    ESP_LOGI(TAG, "hub %p FRAME START(%u): %s", this, (unsigned) len, h.c_str());
  }
  // Publish the first bytes of each frame-start chunk so a console exchange
  // is visible over the API (logs truncate on this device).
  if (len > 0 && data[0] == 0x7E) {
    char hb[4];
    std::string hx;
    for (size_t i = 0; i < len && i < 80; i++) { snprintf(hb, sizeof(hb), "%02X ", data[i]); hx += hb; }
    pending_out_ = "rx: " + hx;
  }
  // Sample the raw stream occasionally so a protocol mismatch is visible
  // without logging every chunk (which starves the BLE task).
  if (rx_chunks_ % 40 == 1) {
    char b[4];
    std::string h;
    for (size_t i = 0; i < len && i < 24; i++) { snprintf(b, sizeof(b), "%02X ", data[i]); h += b; }
    ESP_LOGI(TAG, "hub %p chunk(%u): %s", this, (unsigned) len, h.c_str());
  }
  std::vector<uint8_t> chunk(data, data + len);

  if (protocol_ != nullptr) {
    protocol_->feed(chunk);
    return;
  }
  if (profile_name_ != "auto") return;

  for (const char *candidate : {"seplos_v2_ble", "seplos_v2_uart"}) {
    Protocol *p = create_protocol(candidate, address_);
    if (p == nullptr) continue;
    bool ok = p->feed(chunk) && p->identified();
    if (ok) {
      protocol_ = p;
      ESP_LOGI(TAG, "auto-detected profile '%s'", p->name());
      return;
    }
    delete p;
  }
}

void BmsBridge::on_connected() {
  connected_ = true;
  last_conn_check_ = millis();
  force_full_ = true;
  // The settings block is read on demand, or automatically when configured.
  last_settings_ = millis();
  if (read_settings_on_connect_) {
    // After the first poll pair (0x61, then 0x51 at +1.5 s).
    this->set_timeout(6000, [this]() {
      if (connected_) this->request_cid(0x47);
    });
  }
}
void BmsBridge::on_disconnected() { connected_ = false; }

void BmsBridge::send_bytes(const std::vector<uint8_t> &frame) { this->send_raw(frame.data(), frame.size()); }

void BmsBridge::send_raw(const uint8_t *data, size_t len) {
  if (ble_transport_ != nullptr) {
    ble_transport_->send(data, len);
    return;
  }
  ESP_LOGW(TAG, "no transport bound for send");
}

void BmsBridge::publish_(bool force) {
  if (protocol_ == nullptr) return;
  const PackInfo &i = protocol_->info();
  if (!i.valid) return;

  if (force)
    ESP_LOGI(TAG, "pack %.2fV %.2fA soc=%.1f%% cells=%u (full publish)", i.total_voltage, i.current, i.soc,
             i.cell_count);

  for (auto *s : sensors_) {
    float v = this->value_for_(s->type(), s->index());
    if (s->should_publish(v, force)) {
      s->publish_value(v);
      s->mark_published(v);
    }
  }

  for (auto *t : text_sensors_) {
    const std::string &k = t->type();
    if (k == "profile") {
      if (force || t->state != protocol_->name()) t->publish_state(protocol_->name());
    } else if (k == "status") {
      std::string st = i.valid ? "ok" : "no data";
      if (force || t->state != st) t->publish_state(st);
    } else if (k == "system_state") {
      std::string st = i.tail_valid ? i.sys_state_name() : "Unknown";
      if (force || t->state != st) t->publish_state(st);
    } else if (k == "version") {
      if (force || t->state != BMS_BRIDGE_VERSION) t->publish_state(BMS_BRIDGE_VERSION);
    } else if (k == "active_alarms") {
      std::string st;
      if (i.tail_valid) {
        for (size_t bit = 0; bit < i.events.size() * 8; bit++) {
          if (!i.event(bit)) continue;
          if (!st.empty()) st += ", ";
          st += event_bit_name(bit);
          if (st.size() > 200) {
            st += "\xE2\x80\xA6";
            break;
          }
        }
      }
      if (st.empty()) st = i.tail_valid ? "none" : "unknown";
      if (force || t->state != st) t->publish_state(st);
    }
  }

  if (mac_text_ != nullptr && !mac_str_.empty() && (force || mac_text_->state != mac_str_))
    mac_text_->publish_state(mac_str_);

  for (auto *b : binary_sensors_) {
    const std::string &k = b->type();
    bool v = false;
    if (k == "charge_switch") {
      v = i.charge_switch_closed();
    } else if (k == "discharge_switch") {
      v = i.discharge_switch_closed();
    } else if (k == "balancing") {
      v = i.balancing_count() > 0;
    } else if (k == "alarm") {
      v = i.active_event_count() > 0;
    } else if (k == "cell_balancing") {
      size_t idx = b->index();
      v = i.tail_valid && idx >= 1 && idx <= i.balance_state.size() && i.balance_state[idx - 1];
    }
    if (force || b->state != v) b->publish_state(v);
  }
}

void BmsBridge::publish_stale_() {
  // Publish NaN once so Home Assistant shows the pack as unavailable instead
  // of a healthy timestamp on frozen values.
  for (auto *s : sensors_) {
    float v = NAN;
    if (s->should_publish(v, false)) {
      s->publish_value(v);
      s->mark_published(v);
    }
  }
}

float BmsBridge::value_for_(const std::string &type, uint8_t index) const {
  static const std::pair<const char *, int> SETTINGS_MAP[] = {
      {"setting_cell_ov_alarm", 0},       {"setting_cell_ov_rec", 1},
      {"setting_cell_uv_alarm", 2},       {"setting_cell_uv_rec", 3},
      {"setting_cell_ovp", 4},            {"setting_cell_ovp_rec", 5},
      {"setting_cell_uvp", 6},            {"setting_cell_uvp_rec", 7},
      {"setting_balance_start", 8},       {"setting_charge_forbid", 9},
      {"setting_pack_ov_alarm", 10},      {"setting_pack_ov_rec", 11},
      {"setting_pack_uv_alarm", 12},      {"setting_pack_uv_rec", 13},
      {"setting_pack_ovp", 14},           {"setting_pack_ovp_rec", 15},
      {"setting_pack_uvp", 16},           {"setting_pack_uvp_rec", 17},
      {"setting_charge_ovp", 18},         {"setting_charge_ovp_rec", 19},
      {"setting_charge_temp_high_warn", 20}, {"setting_charge_temp_high_rec", 21},
      {"setting_charge_temp_low_warn", 22},  {"setting_charge_temp_low_rec", 23},
      {"setting_charge_temp_high_prot", 24}, {"setting_charge_temp_high_prot_rec", 25},
      {"setting_charge_temp_low_prot", 26},  {"setting_charge_temp_low_prot_rec", 27},
      {"setting_discharge_temp_high_warn", 28}, {"setting_discharge_temp_high_rec", 29},
      {"setting_discharge_temp_low_warn", 30},  {"setting_discharge_temp_low_rec", 31},
      {"setting_discharge_temp_high_prot", 32}, {"setting_discharge_temp_high_prot_rec", 33},
      {"setting_discharge_temp_low_prot", 34},  {"setting_discharge_temp_low_prot_rec", 35},
      {"setting_cell_heat", 36},          {"setting_cell_heat_rec", 37},
  };
  if (type.rfind("setting_", 0) == 0) {
    for (const auto &e : SETTINGS_MAP) {
      if (type != e.first) continue;
      const auto &w = protocol_->settings_words();
      if (!protocol_->settings_valid() || (size_t) e.second >= w.size()) return NAN;
      uint16_t raw = w[e.second];
      if (e.second < 10) return raw / 1000.0f;
      if (e.second < 20) return raw / 100.0f;
      return (raw - 2731) / 10.0f;
    }
    return NAN;
  }

  const PackInfo &i = protocol_->info();
  if (type == "total_voltage") return i.total_voltage;
  if (type == "current") return i.current;
  if (type == "power") return i.power;
  if (type == "soc") return i.soc;
  if (type == "soh") return i.soh;
  if (type == "capacity") return i.capacity;
  if (type == "remaining_capacity") return i.remaining_capacity;
  if (type == "rated_capacity") return i.rated_capacity;
  // Derived values, matching the semantics shown by the vendor app:
  // full charge = capacity - remaining.
  if (type == "full_charge") return i.capacity - i.remaining_capacity;
  if (type == "full_charge_watts") return i.total_voltage * (i.capacity - i.remaining_capacity);
  if (type == "remaining_watts") return i.total_voltage * i.remaining_capacity;
  if (type == "capacity_watts") return i.total_voltage * i.capacity;
  if (type == "battery_watts") return i.port_voltage * i.current;
  if (type == "temp_delta" && !i.temperatures.empty()) return i.temp_max() - i.temp_min();
  if (type == "cell_max_number" || type == "cell_min_number") {
    if (i.cell_voltages.empty()) return NAN;
    size_t best = 0;
    for (size_t n = 1; n < i.cell_voltages.size(); n++) {
      if ((type == "cell_max_number" && i.cell_voltages[n] > i.cell_voltages[best]) ||
          (type == "cell_min_number" && i.cell_voltages[n] < i.cell_voltages[best]))
        best = n;
    }
    return (float) (best + 1);
  }
  if (type == "temp_max_number" || type == "temp_min_number") {
    if (i.temperatures.empty()) return NAN;
    size_t best = 0;
    for (size_t n = 1; n < i.temperatures.size(); n++) {
      if ((type == "temp_max_number" && i.temperatures[n] > i.temperatures[best]) ||
          (type == "temp_min_number" && i.temperatures[n] < i.temperatures[best]))
        best = n;
    }
    return (float) (best + 1);
  }
  if (type == "cycles") return (float) i.cycles;
  if (type == "port_voltage") return i.port_voltage;
  if (type == "ambient_temperature") return i.ambient_temperature;
  if (type == "power_temperature") return i.mos_temperature;
  if (type == "mos_temperature") return i.mos_temperature;
  if (type == "sys_state") return i.tail_valid ? (float) i.sys_state : NAN;
  if (type == "balancing_cells") return i.tail_valid ? (float) i.balancing_count() : NAN;
  if (type == "invalid_cells") return i.tail_valid ? (float) i.invalidated_cell_count() : NAN;
  if (type == "heartbeat") return millis() / 1000.0f;
  if (type == "active_events") return i.tail_valid ? (float) i.active_event_count() : NAN;
  if (type == "cell_warning" && index >= 1 && index <= i.cell_warnings.size())
    return (float) i.cell_warnings[index - 1];
  if (type == "temp_warning" && index >= 1 && index <= i.temp_warnings.size())
    return (float) i.temp_warnings[index - 1];
  if (type == "cell_voltage_min") return i.cell_min();
  if (type == "cell_voltage_max") return i.cell_max();
  if (type == "cell_voltage_delta") return i.cell_delta();
  if (type == "temperature_min") return i.temp_min();
  if (type == "temperature_max") return i.temp_max();
  if (type == "cell_voltage" && index >= 1 && index <= i.cell_voltages.size()) return i.cell_voltages[index - 1];
  if (type == "temperature" && index >= 1 && index <= i.temperatures.size()) return i.temperatures[index - 1];
  return NAN;
}

std::string BmsBridge::info_json() const {
  const PackInfo &i = protocol_ != nullptr ? protocol_->info() : PackInfo{};
  char b[512];
  snprintf(b, sizeof(b),
           "{\"connected\":%s,\"profile\":\"%s\",\"address\":%u,\"valid\":%s,"
           "\"cells\":%u,\"temps\":%u,\"voltage\":%.2f,\"current\":%.2f,\"soc\":%.1f,\"soh\":%.1f}",
           connected_ ? "true" : "false", this->profile_name(), address_, i.valid ? "true" : "false", i.cell_count,
           i.temp_count, i.total_voltage, i.current, i.soc, i.soh);
  return b;
}

std::string BmsBridge::settings_json() const { return protocol_ != nullptr ? protocol_->settings_json() : "{}"; }

std::string BmsBridge::scan_json() const { return scan_ != nullptr ? scan_->json() : "{\"devices\":[]}"; }

void BmsBridge::note_advert(const std::string &mac, int rssi, const std::string &name, int services) {
  auto it = adv_.find(mac);
  if (it == adv_.end()) {
    adv_[mac] = AdvHit{mac, name, rssi, services};
  } else {
    if (rssi > it->second.rssi) it->second.rssi = rssi;
    if (!name.empty()) it->second.name = name;
    it->second.services = services;
  }
}

void BmsBridge::start_scan(uint32_t window_ms) {
  adv_.clear();
  scan_started_ = millis();
  scan_window_ = window_ms;
  scanning_ = true;
  ESP_LOGI(TAG, "scan started (%u ms)", (unsigned) window_ms);
}

void BmsBridge::load_cfg_nvs_() {
  uint32_t k_en = fnv1a_hash_extend(2166136261u, "bms_auto_reboot_cfg_" + std::to_string(mac_slot_));
  auto p_en = global_preferences->make_preference<uint8_t>(k_en, true);
  uint8_t en = 0;
  if (p_en.load(&en)) auto_reboot_ = en != 0;

  uint32_t k_to = fnv1a_hash_extend(2166136261u, "bms_auto_reboot_to_" + std::to_string(mac_slot_));
  auto p_to = global_preferences->make_preference<uint32_t>(k_to, true);
  uint32_t mins = 0;
  if (p_to.load(&mins) && mins > 0) auto_reboot_timeout_ms_ = mins * 60000u;

  for (auto *s : switches_) s->publish_state(auto_reboot_);
  for (auto *n : numbers_) n->publish_state(auto_reboot_timeout_ms_ / 60000.0f);
  ESP_LOGI(TAG, "auto_reboot %s, timeout %u min", auto_reboot_ ? "on" : "off",
           (unsigned) (auto_reboot_timeout_ms_ / 60000));
}

void BmsBridge::set_auto_reboot_from_ui(bool on) {
  auto_reboot_ = on;
  uint32_t k = fnv1a_hash_extend(2166136261u, "bms_auto_reboot_cfg_" + std::to_string(mac_slot_));
  auto p = global_preferences->make_preference<uint8_t>(k, true);
  uint8_t v = on ? 1 : 0;
  p.save(&v);
  for (auto *s : switches_) s->publish_state(on);
}

void BmsBridge::set_auto_reboot_timeout_min(float minutes) {
  if (minutes < 1) minutes = 1;
  if (minutes > 1440) minutes = 1440;
  auto_reboot_timeout_ms_ = (uint32_t) (minutes * 60000.0f);
  uint32_t k = fnv1a_hash_extend(2166136261u, "bms_auto_reboot_to_" + std::to_string(mac_slot_));
  auto p = global_preferences->make_preference<uint32_t>(k, true);
  uint32_t m = (uint32_t) minutes;
  p.save(&m);
  for (auto *n : numbers_) n->publish_state(minutes);
}

void BmsSwitch::write_state(bool state) {
  this->publish_state(state);
  if (hub_ != nullptr && type_ == "auto_reboot") hub_->set_auto_reboot_from_ui(state);
}

void BmsNumber::control(float value) {
  this->publish_state(value);
  if (hub_ != nullptr && type_ == "auto_reboot_timeout") hub_->set_auto_reboot_timeout_min(value);
}

void BmsBridge::load_saved_mac_() {
  if (ble_client_ == nullptr) return;
  uint32_t key = fnv1a_hash_extend(2166136261u, "bms_bridge_mac_" + std::to_string(mac_slot_));
  auto pref = global_preferences->make_preference<uint64_t>(key, true);
  uint64_t mac = 0;
  if (!pref.load(&mac) || mac == 0) {
    // No stored MAC: fall back to the YAML value. A placeholder (all zero)
    // means there is nothing to connect to yet — stay disabled instead of
    // retrying a bogus address until the user sets one.
    if (ble_client_->get_address() == 0) {
      ble_client_->set_enabled(false);
      ESP_LOGW(TAG, "no MAC configured: set the 'Pack MAC' text entity");
    }
    return;
  }
  ble_client_->set_address(mac);
  mac_str_ = format_mac(mac);
  ESP_LOGI(TAG, "using stored MAC %s", mac_str_.c_str());
  this->set_timeout(1500, [this]() {
    if (ble_client_ != nullptr) {
      ble_client_->set_enabled(true);
      ble_client_->connect();
    }
  });
}

void BmsBridge::set_mac(const std::string &mac) {
  uint32_t key = fnv1a_hash_extend(2166136261u, "bms_bridge_mac_" + std::to_string(mac_slot_));
  auto pref = global_preferences->make_preference<uint64_t>(key, true);

  if (mac == "clear" || mac == "none" || mac == "00:00:00:00:00:00") {
    uint64_t zero = 0;
    pref.save(&zero);
    mac_str_.clear();
    if (ble_client_ != nullptr) {
      ble_client_->disconnect();
      ble_client_->set_enabled(false);
    }
    if (mac_text_ != nullptr) mac_text_->publish_state("");
    this->set_output("MAC cleared");
    return;
  }

  uint64_t value = 0;
  if (!parse_mac(mac, value)) {
    this->set_output("invalid MAC (expected AA:BB:CC:DD:EE:FF or 'clear')");
    return;
  }
  pref.save(&value);
  mac_str_ = format_mac(value);
  ESP_LOGI(TAG, "stored MAC %s (slot %u)", mac_str_.c_str(), (unsigned) mac_slot_);
  if (ble_client_ != nullptr) {
    if (ble_client_->get_address() == 0) g_active_hubs++;
    ble_client_->set_address(value);
    ble_client_->disconnect();
    // A fresh link must not be judged by the previous link's stall state.
    rx_since_enable_ = false;
    last_rx_ = millis();
    stall_recovering_ = false;
    stall_count_ = 0;
    this->set_timeout(600, [this]() {
      if (ble_client_ != nullptr) {
        ble_client_->set_enabled(true);
        ble_client_->connect();
        last_conn_check_ = millis();
      }
    });
  }
  if (mac_text_ != nullptr) mac_text_->publish_state(mac_str_);
  this->set_output(("MAC set: " + mac_str_).c_str());
}

void BmsCommandText::control(const std::string &value) {
  if (mac_mode_ && hub_ != nullptr) {
    hub_->set_mac(value);
    return;
  }
  this->publish_state(value);
}

void BmsBridge::on_ble_disconnect() {
  last_conn_check_ = millis();
  if (ble_backoff_) return;
  if (++ble_fail_count_ < 5) return;

  ble_backoff_ = true;
  ESP_LOGW(TAG, "%u connect failures, pausing BLE for 60 s", ble_fail_count_);
  if (ble_client_ != nullptr) ble_client_->set_enabled(false);
  this->set_timeout(60000, [this]() {
    ble_backoff_ = false;
    ble_fail_count_ = 0;
    if (ble_client_ != nullptr) ble_client_->set_enabled(true);
    rx_since_enable_ = false;
    last_rx_ = millis();
    last_conn_check_ = millis();
    ESP_LOGI(TAG, "BLE retry window reopened");
  });
}

void BmsButton::press_action() {
  if (hub_ == nullptr) return;
  switch (mode_) {
    case 1: hub_->run_command_from_text(); break;
    case 2: hub_->request_cid(0x47); break;  // settings (read-only)
    case 3: hub_->request_cid(0x44); break;  // alarms  (read-only)
    default: hub_->start_scan(30000); break;
  }
}

void BmsBridge::run_command_from_text() {
  if (cmd_text_ == nullptr) {
    this->set_output("no command text entity");
    return;
  }
  this->run_command(cmd_text_->state);
}

void BmsBridge::request_cid(uint8_t cid2) {
  if (protocol_ == nullptr || ble_transport_ == nullptr) {
    this->set_output("no link");
    return;
  }
  auto f = protocol_->build_request(cid2);
  if (f.empty()) {
    this->set_output("unsupported cid");
    return;
  }
  ble_transport_->send(f.data(), f.size());
  next_poll_allowed_ = millis() + 2500;
  request_active_ = true;
  this->set_timeout(2500, [this]() { request_active_ = false; });
  if (cid2 == 0x47) last_settings_ = millis();
  char b[24];
  snprintf(b, sizeof(b), "sent cid 0x%02X", cid2);
  this->set_output(b);
}

void BmsBridge::set_output(const std::string &text) {
  ESP_LOGI(TAG, "out: %s", text.c_str());
  for (auto *t : text_sensors_)
    if (t->type() == "output") t->publish_state(text);
}

void BmsBridge::run_command(const std::string &cmd) {
  if (ble_client_ == nullptr) {
    this->set_output("no ble_client");
    return;
  }
  if (cmd == "connect") {
    ble_client_->set_enabled(true);
    rx_since_enable_ = false;
    last_rx_ = millis();
    this->set_output("connect requested");
  } else if (cmd == "disconnect") {
    ble_client_->set_enabled(false);
    this->set_output("disconnected");
  } else if (cmd == "pair") {
    ble_client_->pair();
    this->set_output("pair requested");
  } else if (cmd == "dump") {
    this->set_output(ble_transport_ != nullptr ? ble_transport_->services_summary() : "no transport");
  } else if (cmd.rfind("send ", 0) == 0) {
    if (ble_transport_ != nullptr) {
      if (!allow_writes_) {
        int cid = frame_cid2(cmd.substr(5));
        if (cid < 0) {
          this->set_output("send: not a 7E frame");
          return;
        }
        // Read-only by default: status, device info, cell info, alarms, settings.
        if (cid != 0x61 && cid != 0x51 && cid != 0x42 && cid != 0x44 && cid != 0x47) {
          char b[48];
          snprintf(b, sizeof(b), "read-only: cid 0x%02X blocked", cid);
          this->set_output(b);
          return;
        }
      }
      ble_transport_->send_hex(cmd.substr(5));
      this->set_output("sent " + cmd.substr(5));
    }
  } else {
    this->set_output("unknown: " + cmd);
  }
}

void BmsBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "BMS Bridge:");
  ESP_LOGCONFIG(TAG, "  transport: %s", transport_.c_str());
  ESP_LOGCONFIG(TAG, "  address: 0x%02X", address_);
  ESP_LOGCONFIG(TAG, "  sensors: %u", (unsigned) sensors_.size());
}

}  // namespace bms_bridge
}  // namespace esphome
