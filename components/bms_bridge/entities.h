#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text/text.h"

#include <cmath>
#include <string>

namespace esphome {
namespace bms_bridge {

class BmsSensor : public sensor::Sensor {
 public:
  void set_type(const std::string &t) { type_ = t; }
  void set_index(uint8_t i) { index_ = i; }
  const std::string &type() const { return type_; }
  uint8_t index() const { return index_; }
  void publish_value(float v) { this->publish_state(v); }

  // Dedupe: publish only when the rendered value changes. NaN compares equal
  // to NaN so undecoded entries do not republish every cycle.
  bool should_publish(float v, bool force) const {
    if (force) return true;
    bool both_nan = std::isnan(v) && std::isnan(last_);
    if (both_nan) return false;
    return !has_ || v != last_;
  }
  void mark_published(float v) { last_ = v; has_ = true; }
  bool is_nan_published() const { return has_ && std::isnan(last_); }

 protected:
  std::string type_;
  uint8_t index_{0};
  float last_{NAN};
  bool has_{false};
};

class BmsTextSensor : public text_sensor::TextSensor {
 public:
  void set_type(const std::string &t) { type_ = t; }
  const std::string &type() const { return type_; }

 protected:
  std::string type_;
};

class BmsBinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_type(const std::string &t) { type_ = t; }
  const std::string &type() const { return type_; }

 protected:
  std::string type_;
};

class BmsBridge;

class BmsButton : public button::Button {
 public:
  void set_hub(BmsBridge *hub) { hub_ = hub; }
  void set_mode(int mode) { mode_ = mode; }
  void press_action() override;

 protected:
  BmsBridge *hub_{nullptr};
  int mode_{0};  // 0=scan, 1=run command, 2=settings, 3=alarms
};

class BmsCommandText : public text::Text {
 public:
  void control(const std::string &value) override { this->publish_state(value); }
};

}  // namespace bms_bridge
}  // namespace esphome
