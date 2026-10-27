#pragma once

#ifdef USE_WEB_SERVER_BASE
#include "esphome/components/web_server_base/web_server_base.h"
#endif

namespace esphome {
namespace bms_bridge {

class BmsBridge;

void register_web_ui(BmsBridge *hub);

#ifdef USE_WEB_SERVER_BASE
class BmsWebHandler : public AsyncWebHandler {
 public:
  explicit BmsWebHandler(BmsBridge *hub) : hub_(hub) {}
  bool canHandle(AsyncWebServerRequest *request) override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  BmsBridge *hub_;
};
#endif

}  // namespace bms_bridge
}  // namespace esphome
