#include "ble_transport.h"

#include "bms_bridge.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bms_bridge {

static const char *const TAG = "bms_bridge.ble";

static const uint16_t SERVICE_UUID = 0xFF00;
static const uint16_t NOTIFY_UUID = 0xFF01;
static const uint16_t WRITE_UUID = 0xFF02;

void BleTransport::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE transport: service 0x%04X notify 0x%04X write 0x%04X", SERVICE_UUID, NOTIFY_UUID, WRITE_UUID);
}

void BleTransport::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT:
      ESP_LOGI(TAG, "connected");
      if (hub_ != nullptr) hub_->on_connected();
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *notify = this->parent()->get_characteristic(SERVICE_UUID, NOTIFY_UUID);
      auto *write = this->parent()->get_characteristic(SERVICE_UUID, WRITE_UUID);
      if (notify == nullptr || write == nullptr) {
        ESP_LOGE(TAG, "required characteristic missing (notify=%p write=%p)", notify, write);
        break;
      }
      notify_handle_ = notify->handle;
      write_handle_ = write->handle;
      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      notify_handle_);
      if (status != ESP_OK) ESP_LOGW(TAG, "register_for_notify failed: %d", status);
      break;
    }

    case ESP_GATTC_SEARCH_RES_EVT:
      ESP_LOGI(TAG, "service uuid16=0x%04X handle=%u", param->search_res.srvc_id.uuid.uuid.uuid16,
               param->search_res.start_handle);
      {
        char b[32];
        snprintf(b, sizeof(b), "0x%04X ", param->search_res.srvc_id.uuid.uuid.uuid16);
        services_ += b;
      }
      break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      ready_ = true;
      ESP_LOGI(TAG, "notifications enabled");
      break;

    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == notify_handle_ && hub_ != nullptr) {
        // A notification proves the link is usable. The CCCD write completion
        // can be missed on a second concurrent connection, leaving ready_ and
        // the hub's connected_ flag false and silently blocking publishing.
        if (!ready_) {
          ready_ = true;
          hub_->on_connected();
        }
        // Keep this callback cheap: no hex formatting, no text-sensor publish,
        // no logging. Doing that per chunk starved the BLE task and dropped
        // chunks on the second link (losing the frame start byte).
        hub_->on_bytes(param->notify.value, param->notify.value_len);
      }
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ready_ = false;
      ESP_LOGW(TAG, "disconnected");
      if (hub_ != nullptr) {
        hub_->on_disconnected();
        hub_->on_ble_disconnect();
      }
      break;

    default:
      break;
  }
}

void BleTransport::send(const uint8_t *data, size_t len) {
  if (!ready_ || write_handle_ == 0) {
    ESP_LOGW(TAG, "send dropped: link not ready");
    return;
  }
  auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), write_handle_,
                                         len, const_cast<uint8_t *>(data), ESP_GATT_WRITE_TYPE_NO_RSP,
                                         ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) ESP_LOGW(TAG, "write failed: %d", status);
}

void BleTransport::send_hex(const std::string &hex) {
  std::vector<uint8_t> out;
  uint8_t hi = 0;
  bool have_hi = false;
  for (char c : hex) {
    int v = -1;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
    if (v < 0) continue;
    if (!have_hi) { hi = (uint8_t) v; have_hi = true; }
    else { out.push_back((uint8_t) ((hi << 4) | v)); have_hi = false; }
  }
  if (out.empty()) { ESP_LOGW(TAG, "send_hex: no bytes"); return; }
  this->send(out.data(), out.size());
}

}  // namespace bms_bridge
}  // namespace esphome
