#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace bms_bridge {

enum Capability : uint32_t {
  CAP_INFO = 1u << 0,
  CAP_CELLS = 1u << 1,
  CAP_TEMPS = 1u << 2,
  CAP_SETTINGS = 1u << 3,
  CAP_WRITE = 1u << 4,
  CAP_PACK_TABLE = 1u << 5,
};

struct PackInfo {
  bool valid{false};
  uint8_t address{0};
  uint8_t cell_count{0};
  uint8_t temp_count{0};
  float total_voltage{0};
  float current{0};
  float power{0};
  float soc{0};
  float soh{0};
  float capacity{0};
  float remaining_capacity{0};
  float rated_capacity{0};
  uint16_t cycles{0};
  float port_voltage{0};
  float mos_temperature{0};
  std::vector<float> cell_voltages;
  std::vector<float> temperatures;

  float cell_min() const;
  float cell_max() const;
  float cell_delta() const;
  float temp_min() const;
  float temp_max() const;
};

class Protocol {
 public:
  virtual ~Protocol() = default;

  virtual const char *name() const = 0;
  virtual uint32_t capabilities() const { return CAP_INFO; }

  // Frames to send at the start of each update cycle.
  virtual std::vector<std::vector<uint8_t>> poll_requests() = 0;

  // Feed received bytes; true when at least one complete message was consumed.
  virtual bool feed(const std::vector<uint8_t> &chunk) = 0;

  virtual const PackInfo &info() const = 0;
  virtual bool identified() const = 0;

  virtual std::string settings_json() const { return "{}"; }

  // One-off request for a specific CID2 (settings 0x47, alarms 0x44, ...).
  virtual std::vector<uint8_t> build_request(uint8_t cid2) const { return {}; }
  // Last non-telemetry message seen, for read-only diagnostics.
  virtual uint8_t last_cid() const { return 0; }
  virtual std::string last_raw_hex() const { return ""; }

  // Read-only settings block (raw words, scaled by the consumer).
  virtual const std::vector<uint16_t> &settings_words() const {
    static const std::vector<uint16_t> empty;
    return empty;
  }
  virtual bool settings_valid() const { return false; }

  // Bytes after the decoded telemetry fields in the status frame (alarm /
  // function-state tail). Exposed read-only until the bit layout is mapped.
  virtual std::string status_tail_hex() const { return ""; }
  virtual uint32_t frames() const { return 0; }
  virtual size_t buf_size() const { return 0; }
  virtual uint32_t crc_fail() const { return 0; }
  virtual uint32_t drops() const { return 0; }
  virtual std::string buf_peek() const { return ""; }
  virtual std::string capture() const { return ""; }
  virtual uint32_t pushes() const { return 0; }
};

// Factory. Returns nullptr for an unknown profile.
Protocol *create_protocol(const std::string &profile, uint8_t address);

}  // namespace bms_bridge
}  // namespace esphome
