#pragma once

#include "protocol.h"

namespace esphome {
namespace bms_bridge {

// Seplos / PACE v2 family. Two wire encodings, one field layout:
//   BLE  : 7E 10 00 46 CID2 LEN_HI LEN_LO <payload> CRC16-XMODEM 0D
//   UART : ~ <addr:2> 00 46 <cid2:2> <len:4> <info> <chk:4> \r   (ASCII hex)
class SeplosV2 : public Protocol {
 public:
  enum Mode { MODE_BLE_BINARY, MODE_UART_ASCII };

  SeplosV2(Mode mode, uint8_t address) : mode_(mode), address_(address) {}

  const char *name() const override;
  uint32_t capabilities() const override { return CAP_INFO | CAP_CELLS | CAP_TEMPS | CAP_SETTINGS; }

  std::vector<std::vector<uint8_t>> poll_requests() override;
  bool feed(const std::vector<uint8_t> &chunk) override;
  const PackInfo &info() const override { return info_; }
  bool identified() const override { return identified_; }
  std::string settings_json() const override;
  std::vector<uint8_t> build_request(uint8_t cid2) const override { return this->build_(cid2, {}); }
  uint8_t last_cid() const override { return last_cid_; }
  std::string last_raw_hex() const override { return last_raw_; }
  const std::vector<uint16_t> &settings_words() const override { return settings_words_; }
  bool settings_valid() const override { return settings_valid_; }
  std::string status_tail_hex() const override { return tail_hex_; }
  uint32_t frames() const override { return frames_; }
  size_t buf_size() const override { return buf_.size(); }
  uint32_t crc_fail() const override { return crc_fail_; }
  uint32_t drops() const override { return drops_; }
  std::string buf_peek() const override;
  std::string capture() const override { return capture_; }
  uint32_t pushes() const override { return pushes_; }

 protected:
  void handle_message_(uint8_t cid2, const std::vector<uint8_t> &payload);
  void decode_status_(const std::vector<uint8_t> &p);
  void decode_manufacturer_(const std::vector<uint8_t> &p);
  void decode_settings_(const std::vector<uint8_t> &p);
  std::vector<uint8_t> build_(uint8_t cid2, const std::vector<uint8_t> &payload) const;

  Mode mode_;
  uint8_t address_;
  std::vector<uint8_t> buf_;
  PackInfo info_;
  bool identified_{false};
  std::string model_, hw_, sw_;
  uint8_t last_cid_{0};
  std::string last_raw_;
  std::vector<uint16_t> settings_words_;
  bool settings_valid_{false};
  std::string tail_hex_;
  uint32_t frames_{0};
  uint32_t crc_fail_{0};
  uint32_t drops_{0};
  uint32_t pushes_{0};
  std::string capture_;
};

}  // namespace bms_bridge
}  // namespace esphome
