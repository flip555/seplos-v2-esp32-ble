#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/text/text.h"

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

 protected:
  std::string type_;
  uint8_t index_{0};
};

class BmsTextSensor : public text_sensor::TextSensor {
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
