#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

namespace esphome {
namespace bms_bridge {

class BmsBridge;

// Seplos BLE profile: service 0xFF00, notify 0xFF01, write 0xFF02.
class BleTransport : public Component, public ble_client::BLEClientNode {
 public:
  void set_hub(BmsBridge *hub) { hub_ = hub; }
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void send(const uint8_t *data, size_t len);
  void send_hex(const std::string &hex);
  std::string services_summary() const { return services_; }
  void dump_config() override;

 protected:
  BmsBridge *hub_{nullptr};
  uint16_t notify_handle_{0};
  uint16_t write_handle_{0};
  bool ready_{false};
  std::string services_;
};

}  // namespace bms_bridge
}  // namespace esphome
