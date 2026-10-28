#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "entities.h"
#include "ble_scan.h"
#include "protocol.h"

#include <string>
#include <map>
#include <vector>

namespace esphome {
namespace bms_bridge {

class BleTransport;

class BmsBridge : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_transport(const std::string &t) { transport_ = t; }
  void set_profile(const std::string &p) { profile_name_ = p; }
  void set_address(uint8_t a) { address_ = a; }
  void set_update_interval_ms(uint32_t ms) { update_interval_ms_ = ms; }
  void set_update_interval(uint32_t ms) { update_interval_ms_ = ms; }
  void set_allow_writes(bool on) { allow_writes_ = on; }
  void set_ble_client(ble_client::BLEClient *c) { ble_client_ = c; }
  void set_poll_offset(uint32_t ms) { poll_offset_ = ms; }

  void register_sensor(BmsSensor *s) { sensors_.push_back(s); }
  void register_text_sensor(BmsTextSensor *t) { text_sensors_.push_back(t); }
  void register_command_text(BmsCommandText *t) { cmd_text_ = t; }

  void run_command(const std::string &cmd);
  void run_command_from_text();
  void set_output(const std::string &text);
  void request_cid(uint8_t cid2);

  void on_bytes(const uint8_t *data, size_t len);
  void on_connected();
  void on_disconnected();
  void send_bytes(const std::vector<uint8_t> &frame);
  void send_raw(const uint8_t *data, size_t len);

  const Protocol *protocol() const { return protocol_; }
  const char *profile_name() const { return protocol_ != nullptr ? protocol_->name() : "auto"; }
  bool connected() const { return connected_; }

  std::string info_json() const;
  std::string settings_json() const;
  std::string scan_json() const;

  // Advertisement collection (driven by the YAML on_ble_advertise hook).
  void note_advert(const std::string &mac, int rssi, const std::string &name, int services);
  void start_scan(uint32_t window_ms);
  bool scanning() const { return scanning_; }
  void on_ble_disconnect();

 protected:
  void publish_(bool force = false);
  void publish_stale_();
  float value_for_(const std::string &type, uint8_t index) const;

  std::string transport_{"ble"};
  std::string profile_name_{"auto"};
  uint8_t address_{0};
  uint32_t update_interval_ms_{10000};
  uint32_t last_poll_{0};
  bool allow_writes_{false};
  bool connected_{false};

  struct AdvHit {
    std::string mac;
    std::string name;
    int rssi{0};
    int services{0};
  };
  std::map<std::string, AdvHit> adv_;
  bool scanning_{false};
  uint32_t scan_started_{0};
  uint32_t scan_window_{30000};
  uint8_t ble_fail_count_{0};
  bool ble_backoff_{false};
  uint32_t last_conn_check_{0};
  uint8_t last_seen_cid_{0};
  std::string last_raw_published_;
  std::string last_tail_published_;
  std::string last_capture_;
  std::string pending_out_;
  uint32_t last_rx_{0};
  uint32_t rx_chunks_{0};
  uint32_t rx_bytes_{0};
  uint32_t last_diag_{0};
  uint8_t start_samples_{0};
  uint32_t poll_offset_{0};
  uint32_t start_at_{0};
  uint32_t last_frames_{0};
  uint32_t last_heartbeat_{0};
  bool force_full_{true};
  bool stall_recovering_{false};
  bool rx_since_enable_{false};
  uint32_t stall_count_{0};

  Protocol *protocol_{nullptr};
  ble_client::BLEClient *ble_client_{nullptr};
  BleTransport *ble_transport_{nullptr};
  BleScanListener *scan_{nullptr};
  bool scan_registered_{false};
  BmsCommandText *cmd_text_{nullptr};
  std::vector<BmsSensor *> sensors_;
  std::vector<BmsTextSensor *> text_sensors_;
};

}  // namespace bms_bridge
}  // namespace esphome
