#include "web_ui.h"

#include "bms_bridge.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bms_bridge {

static const char *const TAG = "bms_bridge.web";

#ifdef USE_WEB_SERVER_BASE

static const char *const PAGE = R"HTML(
<!doctype html><meta name=viewport content="width=device-width,initial-scale=1">
<title>BMS Bridge</title>
<style>body{font:15px system-ui;margin:2rem;max-width:44rem}code{background:#eee;padding:.1rem .3rem}</style>
<h1>BMS Bridge</h1>
<pre id=info>loading…</pre>
<p><a href="/bms/api/info">/bms/api/info</a> ·
   <a href="/bms/api/settings">/bms/api/settings</a> ·
   <a href="/bms/api/scan">/bms/api/scan</a></p>
<script>fetch('/bms/api/info').then(r=>r.text()).then(t=>info.textContent=t)</script>
)HTML";

bool BmsWebHandler::canHandle(AsyncWebServerRequest *request) { return request->url().startsWith("/bms"); }

void BmsWebHandler::handleRequest(AsyncWebServerRequest *request) {
  const String &url = request->url();
  if (url == "/bms") {
    request->send(200, "text/html", PAGE);
  } else if (url == "/bms/api/info") {
    request->send(200, "application/json", hub_->info_json().c_str());
  } else if (url == "/bms/api/settings") {
    request->send(200, "application/json", hub_->settings_json().c_str());
  } else if (url == "/bms/api/scan") {
    request->send(200, "application/json", hub_->scan_json().c_str());
  } else {
    request->send(404, "text/plain", "not found");
  }
}

#endif

void register_web_ui(BmsBridge *hub) {
#ifdef USE_WEB_SERVER_BASE
  web_server_base::global_web_server_base->add_handler(new BmsWebHandler(hub));
  ESP_LOGI(TAG, "web UI registered at /bms");
#else
  ESP_LOGW(TAG, "web_ui: true but web_server is not enabled");
#endif
}

}  // namespace bms_bridge
}  // namespace esphome
