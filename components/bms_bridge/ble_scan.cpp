#include "ble_scan.h"

#include "esphome/core/log.h"

#include <cstdio>

namespace esphome {
namespace bms_bridge {

static const char *const TAG = "bms_bridge.scan";
static const size_t MAX_HITS = 24;

bool BleScanListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  const std::string mac = device.address_str();
  const std::string name = device.get_name();
  const int rssi = device.get_rssi();
  const uint8_t services = (uint8_t) device.get_service_uuids().size();

  for (auto &h : hits_) {
    if (h.mac == mac) {
      h.rssi = rssi;
      if (!name.empty()) h.name = name;
      h.services = services;
      return false;
    }
  }
  if (hits_.size() >= MAX_HITS) return false;

  hits_.push_back({mac, name, rssi, services});
  ESP_LOGI(TAG, "seen %s rssi=%d name='%s' services=%u", mac.c_str(), rssi, name.c_str(), services);
  return false;
}

std::string BleScanListener::json() const {
  std::string out = "{\"devices\":[";
  bool first = true;
  for (const auto &h : hits_) {
    char buf[192];
    snprintf(buf, sizeof(buf), "%s{\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"services\":%u}",
             first ? "" : ",", h.mac.c_str(), h.name.c_str(), h.rssi, h.services);
    out += buf;
    first = false;
  }
  out += "]}";
  return out;
}

}  // namespace bms_bridge
}  // namespace esphome
