#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <string>
#include <vector>

namespace esphome {
namespace bms_bridge {

// Collects nearby BLE advertisements so a MAC can be chosen without recompiling.
class BleScanListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  std::string json() const;
  void clear() { hits_.clear(); }
  size_t size() const { return hits_.size(); }

 protected:
  struct Hit {
    std::string mac;
    std::string name;
    int rssi;
    uint8_t services;
  };
  std::vector<Hit> hits_;
};

}  // namespace bms_bridge
}  // namespace esphome
