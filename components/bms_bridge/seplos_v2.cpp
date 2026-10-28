#include "seplos_v2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace esphome {
namespace bms_bridge {

static uint16_t crc16_xmodem(const uint8_t *d, size_t n) {
  uint16_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc ^= (uint16_t) d[i] << 8;
    for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t) ((crc << 1) ^ 0x1021) : (uint16_t) (crc << 1);
  }
  return crc;
}

static uint16_t rd16(const std::vector<uint8_t> &p, size_t i) {
  return (uint16_t) ((p[i] << 8) | p[i + 1]);
}

const char *SeplosV2::name() const {
  return mode_ == MODE_BLE_BINARY ? "seplos_v2_ble" : "seplos_v2_uart";
}

std::vector<uint8_t> SeplosV2::build_(uint8_t cid2, const std::vector<uint8_t> &payload) const {
  std::vector<uint8_t> f;

  if (mode_ == MODE_BLE_BINARY) {
    f = {0x7E, 0x10, address_, 0x46, cid2, (uint8_t) (payload.size() >> 8), (uint8_t) payload.size()};
    f.insert(f.end(), payload.begin(), payload.end());
    uint16_t crc = crc16_xmodem(f.data() + 1, f.size() - 1);
    f.push_back((uint8_t) (crc >> 8));
    f.push_back((uint8_t) crc);
    f.push_back(0x0D);
    return f;
  }

  char head[32];
  snprintf(head, sizeof(head), "%02X0046%02X%04X", address_, cid2, (unsigned) payload.size());
  std::string s(head);
  for (uint8_t b : payload) {
    char hx[4];
    snprintf(hx, sizeof(hx), "%02X", b);
    s += hx;
  }
  uint32_t sum = 0;
  for (char c : s) sum += (uint8_t) c;
  char tail[8];
  snprintf(tail, sizeof(tail), "%04X", (unsigned) ((~sum + 1) & 0xFFFF));
  s += tail;

  f.push_back(0x7E);
  for (char c : s) f.push_back((uint8_t) c);
  f.push_back(0x0D);
  return f;
}

std::vector<std::vector<uint8_t>> SeplosV2::poll_requests() {
  return {this->build_(0x61, {}), this->build_(0x51, {})};
}

bool SeplosV2::feed(const std::vector<uint8_t> &chunk) {
  buf_.insert(buf_.end(), chunk.begin(), chunk.end());
  bool consumed = false;

  // Re-scan for the start byte after every rejected byte. Parsing forward from
  // a misaligned position would cascade through the whole buffer (one bad
  // frame can otherwise eat every good frame behind it).
  while (!buf_.empty()) {
    auto sof = std::find(buf_.begin(), buf_.end(), (uint8_t) 0x7E);
    if (sof == buf_.end()) {
      if (buf_.size() > 2048) buf_.clear();  // runaway guard
      break;
    }
    if (sof != buf_.begin()) {
      buf_.erase(buf_.begin(), sof);
      consumed = true;
    }
    if (buf_.size() < 7) break;

    if (mode_ == MODE_BLE_BINARY) {
      // Reject a false sync byte immediately instead of waiting for a bogus
      // length to arrive. Payload bytes legitimately contain 0x7E (e.g. cell
      // voltages near 0x0C7E), so a misaligned buffer otherwise stalls.
      uint8_t ver = buf_[1];
      uint8_t cid = buf_[3];
      bool cid_ok = cid == 0x61 || cid == 0x51 || cid == 0x47 || cid == 0x42 || cid == 0x44 ||
                    cid == 0x46 || cid == 0xA2 || cid == 0xA3 || cid == 0x80 || cid == 0x81 || cid == 0xC0;
      if ((ver != 0x14 && ver != 0x10) || buf_[2] > 0x0F || !cid_ok) {
        drops_++;
        buf_.erase(buf_.begin());
        consumed = true;
        continue;
      }
      uint16_t len = rd16(buf_, 5);
      size_t total = 10 + len;
      if (len == 0 || total > 320) {
        drops_++;
        buf_.erase(buf_.begin());
        consumed = true;
        continue;
      }
      if (buf_.size() < total) break;
      uint16_t want = crc16_xmodem(buf_.data() + 1, total - 4);
      uint16_t got = (uint16_t) ((buf_[total - 3] << 8) | buf_[total - 2]);
      if (buf_[total - 1] != 0x0D || want != got) {
        crc_fail_++;
        if (crc_fail_ <= 6 || crc_fail_ % 40 == 0) {
          // Record the whole candidate plus where its real terminator sits, so
          // a per-pack layout difference is visible without a serial console.
          size_t end0d = 0, next7e = 0;
          for (size_t k = 7; k < buf_.size(); k++)
            if (buf_[k] == 0x0D) { end0d = k; break; }
          for (size_t k = 1; k < buf_.size(); k++)
            if (buf_[k] == 0x7E) { next7e = k; break; }
          std::string hx;
          char hb[4];
          for (size_t k = 0; k < buf_.size() && k < 150; k++) {
            snprintf(hb, sizeof(hb), "%02X ", buf_[k]);
            hx += hb;
          }
          char cb[720];
          snprintf(cb, sizeof(cb), "#%u len=%u total=%u end0d=%u next7e=%u crc=%04X/%04X %s",
                   (unsigned) crc_fail_, (unsigned) len, (unsigned) total, (unsigned) end0d,
                   (unsigned) next7e, want, got, hx.c_str());
          capture_ = cb;
        }
        buf_.erase(buf_.begin());
        consumed = true;
        continue;
      }
      std::vector<uint8_t> payload(buf_.begin() + 7, buf_.begin() + total - 3);
      this->handle_message_(buf_[3], payload);
      buf_.erase(buf_.begin(), buf_.begin() + total);
      consumed = true;
    } else {
      auto eof = std::find(buf_.begin() + 1, buf_.end(), (uint8_t) 0x0D);
      if (eof == buf_.end()) break;
      size_t total = (size_t) (eof - buf_.begin()) + 1;
      std::vector<uint8_t> b;
      for (size_t i = 1; i + 1 < total; i += 2) {
        char hx[3] = {(char) buf_[i], (char) buf_[i + 1], 0};
        b.push_back((uint8_t) strtoul(hx, nullptr, 16));
      }
      if (b.size() >= 6) {
        std::vector<uint8_t> info(b.begin() + 6, b.size() >= 8 ? b.end() - 2 : b.end());
        this->handle_message_(b[3], info);
      }
      buf_.erase(buf_.begin(), buf_.begin() + total);
      consumed = true;
    }
  }
  return consumed;
}

void SeplosV2::handle_message_(uint8_t cid2, const std::vector<uint8_t> &p) {
  switch (cid2) {
    case 0x61:
    case 0x42:
      this->decode_status_(p);
      break;
    case 0x51:
      this->decode_manufacturer_(p);
      break;
    case 0x47:
      this->decode_settings_(p);
      break;
    case 0xA2:
    case 0xA3:
      pushes_++;
      break;
    default: {
      this->last_cid_ = cid2;
      this->last_raw_.clear();
      char b[4];
      for (size_t i = 0; i < p.size() && this->last_raw_.size() < 700; i++) {
        snprintf(b, sizeof(b), "%02X ", p[i]);
        this->last_raw_ += b;
      }
      break;
    }
  }
}

std::string SeplosV2::buf_peek() const {
  std::string h;
  char b[4];
  for (size_t i = 0; i < buf_.size() && i < 24; i++) {
    snprintf(b, sizeof(b), "%02X ", buf_[i]);
    h += b;
  }
  return h;
}

void SeplosV2::decode_settings_(const std::vector<uint8_t> &p) {
  // The 0x47 block: two leading bytes (word count), then 16-bit big-endian
  // words. Word 0 is the cell OV alarm, 10 the pack OV alarm, 20+ temperatures.
  settings_words_.clear();
  for (size_t i = 2; i + 1 < p.size(); i += 2)
    settings_words_.push_back((uint16_t) ((p[i] << 8) | p[i + 1]));
  settings_valid_ = !settings_words_.empty();
}

void SeplosV2::decode_manufacturer_(const std::vector<uint8_t> &p) {
  if (p.size() < 36) return;
  auto txt = [&](size_t a, size_t b) {
    std::string s;
    for (size_t i = a; i < b && i < p.size(); i++) {
      char c = (char) p[i];
      if (c >= 32 && c < 127) s += c;
    }
    return s;
  };
  model_ = txt(0, 20);
  hw_ = txt(20, 30);
  char v[16];
  snprintf(v, sizeof(v), "%u.%u", p[30], p[31]);
  sw_ = v;
  identified_ = !model_.empty();
}

void SeplosV2::decode_status_(const std::vector<uint8_t> &p) {
  if (p.size() < 53) return;
  uint8_t cells = p[2];
  if (cells == 0 || cells > 32) return;

  info_.cell_voltages.clear();
  for (uint8_t i = 0; i < cells; i++) info_.cell_voltages.push_back(rd16(p, 3 + i * 2) * 0.001f);
  info_.cell_count = cells;

  size_t to = 3 + cells * 2;
  uint8_t temps = p[to];
  info_.temperatures.clear();
  for (uint8_t i = 0; i < temps && to + 3 + i * 2 < p.size(); i++)
    info_.temperatures.push_back((rd16(p, to + 1 + i * 2) - 2731) * 0.1f);
  info_.temp_count = temps;

  size_t o = to + 1 + (size_t) temps * 2;
  if (o + 18 > p.size()) return;
  info_.current = (int16_t) rd16(p, o) * 0.01f;
  info_.total_voltage = rd16(p, o + 2) * 0.01f;
  info_.power = info_.total_voltage * info_.current;
  info_.remaining_capacity = rd16(p, o + 4) * 0.01f;
  info_.capacity = rd16(p, o + 7) * 0.01f;
  info_.soc = rd16(p, o + 9) * 0.1f;
  info_.rated_capacity = rd16(p, o + 11) * 0.01f;
  info_.cycles = rd16(p, o + 13);
  info_.soh = rd16(p, o + 15) * 0.1f;
  info_.port_voltage = rd16(p, o + 17) * 0.01f;
  // Everything after port voltage: the still-unmapped alarm/function tail.
  this->tail_hex_.clear();
  for (size_t i = o + 19; i < p.size(); i++) {
    char b[4];
    snprintf(b, sizeof(b), "%02X ", p[i]);
    this->tail_hex_ += b;
  }
  info_.valid = true;
  identified_ = true;
  frames_++;
}

std::string SeplosV2::settings_json() const {
  char b[192];
  snprintf(b, sizeof(b), "{\"model\":\"%s\",\"hw\":\"%s\",\"sw\":\"%s\",\"cells\":%u,\"temps\":%u}",
           model_.c_str(), hw_.c_str(), sw_.c_str(), info_.cell_count, info_.temp_count);
  return b;
}

Protocol *create_protocol(const std::string &profile, uint8_t address) {
  if (profile == "seplos_v2_ble") return new SeplosV2(SeplosV2::MODE_BLE_BINARY, address);
  if (profile == "seplos_v2_uart") return new SeplosV2(SeplosV2::MODE_UART_ASCII, address);
  return nullptr;
}

}  // namespace bms_bridge
}  // namespace esphome
