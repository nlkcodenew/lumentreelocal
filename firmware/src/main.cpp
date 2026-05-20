#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <map>
#include <vector>

#if __has_include("lumentree_config.h")
#include "lumentree_config.h"
#endif

#ifndef LUMENTREE_DEFAULT_WIFI_SSID
#define LUMENTREE_DEFAULT_WIFI_SSID ""
#endif
#ifndef LUMENTREE_DEFAULT_WIFI_PASSWORD
#define LUMENTREE_DEFAULT_WIFI_PASSWORD ""
#endif
#ifndef LUMENTREE_DEFAULT_API_URL
#define LUMENTREE_DEFAULT_API_URL "https://lumentree.jonah.io.vn"
#endif
#ifndef LUMENTREE_DEFAULT_API_TOKEN
#define LUMENTREE_DEFAULT_API_TOKEN ""
#endif
#ifndef LUMENTREE_DEFAULT_DEVICE_ID
#define LUMENTREE_DEFAULT_DEVICE_ID "P240819130"
#endif
#ifndef LUMENTREE_DEFAULT_TARGET_MAC
#define LUMENTREE_DEFAULT_TARGET_MAC ""
#endif
#ifndef LUMENTREE_DEFAULT_GATEWAY_ID
#define LUMENTREE_DEFAULT_GATEWAY_ID "esp32-lumentree"
#endif
#ifndef LUMENTREE_DEFAULT_UPLOAD_INTERVAL_SECONDS
#define LUMENTREE_DEFAULT_UPLOAD_INTERVAL_SECONDS 10
#endif
#ifndef LUMENTREE_DEFAULT_PRODUCTION_ENABLED
#define LUMENTREE_DEFAULT_PRODUCTION_ENABLED 0
#endif
#ifndef LUMENTREE_DEFAULT_TLS_INSECURE
#define LUMENTREE_DEFAULT_TLS_INSECURE 1
#endif

#ifndef LUMENTREE_FIRMWARE_NAME
#define LUMENTREE_FIRMWARE_NAME "lumentree-ble-bridge"
#endif
#ifndef LUMENTREE_FIRMWARE_VERSION
#define LUMENTREE_FIRMWARE_VERSION "0.0.0-dev"
#endif

static const char* FW_NAME = LUMENTREE_FIRMWARE_NAME;
static const char* FW_VERSION = LUMENTREE_FIRMWARE_VERSION;
static const char* KNOWN_LUMENTREE_SERVICE_UUID = "a018739b-734d-8211-cb80-c9acd39d13b4";
static const char* LUMENTREE_VENDOR_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
static const char* LUMENTREE_VENDOR_CHARACTERISTIC_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";
static const uint32_t SERIAL_BAUD = 115200;
static const uint8_t DEFAULT_SCAN_SECONDS = 2;
static const uint16_t DEFAULT_SCAN_INTERVAL = 800;  // 500 ms in BLE units
static const uint16_t DEFAULT_SCAN_WINDOW = 80;     // 50 ms in BLE units
static const uint16_t DEFAULT_LOG_LIMIT = 80;
static const uint16_t MIN_UPLOAD_INTERVAL_SECONDS = 5;
static const uint16_t DEFAULT_UPLOAD_INTERVAL_SECONDS = LUMENTREE_DEFAULT_UPLOAD_INTERVAL_SECONDS;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
static const uint16_t PROVISIONING_DNS_PORT = 53;
static const uint32_t WATCHDOG_TIMEOUT_SECONDS = 60;
static const unsigned long HEARTBEAT_INTERVAL_MS = 300000;
static const unsigned long AUTO_DISCOVERY_RETRY_MS = 60000;
static const unsigned long COMMAND_POLL_INTERVAL_MS = 5000;
static const unsigned long WRITE_PAIRING_CODE_TTL_MS = 600000;
static const unsigned long SETTINGS_UPLOAD_INTERVAL_MS = 60000;
static const unsigned long STATS_UPLOAD_INTERVAL_MS = 5UL * 60UL * 1000UL;

static BLEScan* bleScan = nullptr;
static Preferences prefs;
static WebServer server(80);
static DNSServer dnsServer;
static String targetMac;
static String wifiSsid;
static String wifiPassword;
static String apiUrl;
static String apiToken;
static String deviceId;
static String gatewayId;
static uint8_t scanSeconds = DEFAULT_SCAN_SECONDS;
static uint16_t scanInterval = DEFAULT_SCAN_INTERVAL;
static uint16_t scanWindow = DEFAULT_SCAN_WINDOW;
static uint16_t logLimit = DEFAULT_LOG_LIMIT;
static uint16_t uploadIntervalSeconds = DEFAULT_UPLOAD_INTERVAL_SECONDS;
static uint16_t loggedThisScan = 0;
static bool scanActive = false;
static bool activeScan = false;
static bool candidateOnly = false;
static bool productionEnabled = LUMENTREE_DEFAULT_PRODUCTION_ENABLED != 0;
static bool tlsInsecure = LUMENTREE_DEFAULT_TLS_INSECURE != 0;
static bool provisioningPortalActive = false;
static String provisioningApSsid;
static esp_ble_addr_type_t targetAddressType = BLE_ADDR_TYPE_PUBLIC;
static bool targetAddressTypeKnown = false;
static BLEClient* discoveryClient = nullptr;
static BLEClient* modbusClient = nullptr;
static unsigned long bootMs = 0;
static unsigned long nextUploadMs = 0;
static unsigned long lastHeartbeatMs = 0;
static unsigned long lastAutoDiscoveryMs = 0;
static unsigned long lastGatewayStatusPostMs = 0;
static unsigned long lastCommandPollMs = 0;
static unsigned long lastSettingsUploadMs = 0;
static unsigned long lastStatsUploadMs = 0;
static volatile bool modbusNotifyReceived = false;
static volatile unsigned long modbusLastNotifyMs = 0;
static uint16_t modbusNotifyCount = 0;
static std::string modbusResponseBytes;
static String writePairingCode;
static unsigned long writePairingCodeExpiresMs = 0;

struct ModbusReadResult {
  bool ok = false;
  String payloadHex;
  size_t length = 0;
  uint16_t notifyCount = 0;
};

struct BleCandidate {
  String mac;
  String name;
  int rssi = -127;
  int addressType = 0;
  bool nameMatch = false;
  bool serviceUuidMatch = false;
  bool vendorGattMatch = false;
  int score = 0;
  unsigned long lastSeenMs = 0;
};

static std::vector<BleCandidate> bleCandidates;
static String pairingStatus = "unconfigured";

static void addCandidatesJson(JsonArray array);
static bool runBleDiscovery(bool allowAutoBind);
static bool postGatewayStatus(const char* reason);
static bool postWritePairingCode(const String& code, String& response);
static long writePairingCodeExpiresInSeconds();
static void pollPendingCommand();
static ModbusReadResult runModbusRead(uint16_t startRegister, uint16_t registerCount, const char* label);
static ModbusReadResult runModbusReadInput(uint16_t startRegister, uint16_t registerCount, const char* label);
static bool readRegisterFromResult(const ModbusReadResult& result, uint16_t startRegister, uint16_t registerAddress, uint16_t& value);
static void modbusNotifyCallback(BLERemoteCharacteristic* chr, uint8_t* data, size_t length, bool isNotify);

static void feedWatchdog() {
  esp_task_wdt_reset();
}

static String normalizeMac(String mac) {
  mac.trim();
  mac.toLowerCase();
  return mac;
}

static String normalizeUuid(String uuid) {
  uuid.trim();
  uuid.toLowerCase();
  if (uuid.length() == 4) {
    return "0000" + uuid + "-0000-1000-8000-00805f9b34fb";
  }
  return uuid;
}

static String bytesToHex(const uint8_t* data, size_t len) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    out += hex[(data[i] >> 4) & 0x0F];
    out += hex[data[i] & 0x0F];
  }
  return out;
}

static String bytesToHex(const std::string& data) {
  return bytesToHex((const uint8_t*)data.data(), data.length());
}

static bool hexToBytes(const String& hex, std::string& out) {
  out.clear();
  if (hex.length() % 2 != 0) return false;
  out.reserve(hex.length() / 2);
  for (size_t i = 0; i < hex.length(); i += 2) {
    char pair[3] = {hex[i], hex[i + 1], 0};
    char* end = nullptr;
    unsigned long value = strtoul(pair, &end, 16);
    if (end == pair || *end != 0 || value > 0xFF) return false;
    out.push_back((char)value);
  }
  return true;
}

static uint16_t crc16Modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

static String buildReadCommand(uint8_t slaveId, uint16_t startRegister, uint16_t registerCount) {
  uint8_t frame[8] = {
    slaveId,
    0x03,
    (uint8_t)((startRegister >> 8) & 0xFF),
    (uint8_t)(startRegister & 0xFF),
    (uint8_t)((registerCount >> 8) & 0xFF),
    (uint8_t)(registerCount & 0xFF),
    0,
    0,
  };
  uint16_t crc = crc16Modbus(frame, 6);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}

static String buildReadInputCommand(uint8_t slaveId, uint16_t startRegister, uint16_t registerCount) {
  uint8_t frame[8] = {
    slaveId,
    0x04,
    (uint8_t)((startRegister >> 8) & 0xFF),
    (uint8_t)(startRegister & 0xFF),
    (uint8_t)((registerCount >> 8) & 0xFF),
    (uint8_t)(registerCount & 0xFF),
    0,
    0,
  };
  uint16_t crc = crc16Modbus(frame, 6);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}

static String buildWriteSingleRegisterCommand(uint8_t slaveId, uint16_t registerAddress, uint16_t value) {
  uint8_t frame[8] = {
    slaveId,
    0x06,
    (uint8_t)((registerAddress >> 8) & 0xFF),
    (uint8_t)(registerAddress & 0xFF),
    (uint8_t)((value >> 8) & 0xFF),
    (uint8_t)(value & 0xFF),
    0,
    0,
  };
  uint16_t crc = crc16Modbus(frame, 6);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}

static String buildWriteSingleRegisterFunction16Command(uint8_t slaveId, uint16_t registerAddress, uint16_t value) {
  uint8_t frame[11] = {
    slaveId,
    0x10,
    (uint8_t)((registerAddress >> 8) & 0xFF),
    (uint8_t)(registerAddress & 0xFF),
    0x00,
    0x01,
    0x02,
    (uint8_t)((value >> 8) & 0xFF),
    (uint8_t)(value & 0xFF),
    0,
    0,
  };
  uint16_t crc = crc16Modbus(frame, 9);
  frame[9] = (uint8_t)(crc & 0xFF);
  frame[10] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}

static String buildWriteMultipleRegistersAck(uint8_t slaveId, uint16_t registerAddress, uint16_t registerCount) {
  uint8_t frame[8] = {
    slaveId,
    0x10,
    (uint8_t)((registerAddress >> 8) & 0xFF),
    (uint8_t)(registerAddress & 0xFF),
    (uint8_t)((registerCount >> 8) & 0xFF),
    (uint8_t)(registerCount & 0xFF),
    0,
    0,
  };
  uint16_t crc = crc16Modbus(frame, 6);
  frame[6] = (uint8_t)(crc & 0xFF);
  frame[7] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}

static void printJson(JsonDocument& doc) {
  serializeJson(doc, Serial0);
  Serial0.println();
}

static void loadConfig() {
  prefs.begin("lumentree", false);
  wifiSsid = prefs.getString("wifi_ssid", LUMENTREE_DEFAULT_WIFI_SSID);
  wifiPassword = prefs.getString("wifi_pass", LUMENTREE_DEFAULT_WIFI_PASSWORD);
  apiUrl = prefs.getString("api_url", LUMENTREE_DEFAULT_API_URL);
  apiToken = prefs.getString("api_token", LUMENTREE_DEFAULT_API_TOKEN);
  deviceId = prefs.getString("device_id", LUMENTREE_DEFAULT_DEVICE_ID);
  gatewayId = prefs.getString("gateway_id", LUMENTREE_DEFAULT_GATEWAY_ID);
  targetMac = normalizeMac(prefs.getString("target_mac", LUMENTREE_DEFAULT_TARGET_MAC));
  uploadIntervalSeconds = prefs.getUShort("upload_s", DEFAULT_UPLOAD_INTERVAL_SECONDS);
  productionEnabled = prefs.getBool("prod", LUMENTREE_DEFAULT_PRODUCTION_ENABLED != 0);
  tlsInsecure = prefs.getBool("tls_insec", LUMENTREE_DEFAULT_TLS_INSECURE != 0);
  uploadIntervalSeconds = (uint16_t)constrain(uploadIntervalSeconds, MIN_UPLOAD_INTERVAL_SECONDS, 3600);
  targetAddressTypeKnown = targetMac.length() > 0;
  pairingStatus = targetMac.length() > 0 ? "paired" : "unconfigured";
}

static void saveStringConfig(const char* key, const String& value) {
  prefs.putString(key, value);
}

static void emitConfig(const char* type) {
  JsonDocument doc;
  doc["type"] = type;
  doc["fw"] = FW_NAME;
  doc["version"] = FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  doc["production_enabled"] = productionEnabled;
  doc["wifi_configured"] = wifiSsid.length() > 0;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_ssid"] = wifiSsid.length() > 0 ? wifiSsid : "";
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["api_url"] = apiUrl;
  doc["api_token_configured"] = apiToken.length() > 0;
  doc["device_id"] = deviceId;
  doc["gateway_id"] = gatewayId;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  doc["candidate_count"] = bleCandidates.size();
  doc["upload_interval_seconds"] = uploadIntervalSeconds;
  doc["tls_insecure"] = tlsInsecure;
  doc["provisioning_portal_active"] = provisioningPortalActive;
  doc["provisioning_ap_ssid"] = provisioningApSsid;
  doc["write_pairing_code_active"] = writePairingCodeExpiresInSeconds() > 0;
  doc["write_pairing_code_expires_in_seconds"] = writePairingCodeExpiresInSeconds();
  printJson(doc);
}

static void emitStatus(const char* type) {
  JsonDocument doc;
  doc["type"] = type;
  doc["fw"] = FW_NAME;
  doc["version"] = FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  doc["candidate_count"] = bleCandidates.size();
  doc["scan_seconds"] = scanSeconds;
  doc["scan_interval"] = scanInterval;
  doc["scan_window"] = scanWindow;
  doc["active_scan"] = activeScan;
  doc["candidate_only"] = candidateOnly;
  doc["target_address_type_known"] = targetAddressTypeKnown;
  doc["target_address_type"] = targetAddressType;
  doc["log_limit"] = logLimit;
  doc["device_id_name_hint"] = deviceId;
  doc["known_lumentree_service_uuid"] = KNOWN_LUMENTREE_SERVICE_UUID;
  doc["wifi_mode"] = productionEnabled ? "production_upload" : "manual";
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["api_url"] = apiUrl;
  doc["api_token_configured"] = apiToken.length() > 0;
  doc["device_id"] = deviceId;
  doc["gateway_id"] = gatewayId;
  doc["upload_interval_seconds"] = uploadIntervalSeconds;
  doc["production_enabled"] = productionEnabled;
  doc["provisioning_portal_active"] = provisioningPortalActive;
  doc["provisioning_ap_ssid"] = provisioningApSsid;
  doc["write_pairing_code_active"] = writePairingCodeExpiresInSeconds() > 0;
  doc["write_pairing_code_expires_in_seconds"] = writePairingCodeExpiresInSeconds();
  printJson(doc);
}

static void emitError(const char* code, const char* message) {
  JsonDocument doc;
  doc["type"] = "error";
  doc["code"] = code;
  doc["message"] = message;
  doc["uptime_ms"] = millis() - bootMs;
  printJson(doc);
}

static void emitAck(const char* command) {
  JsonDocument doc;
  doc["type"] = "ack";
  doc["command"] = command;
  doc["uptime_ms"] = millis() - bootMs;
  printJson(doc);
}

static String htmlEscape(const String& text) {
  String out;
  out.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String generateWritePairingCode() {
  String code;
  code.reserve(8);
  for (int i = 0; i < 8; i++) {
    code += char('0' + (esp_random() % 10));
  }
  return code;
}

static long writePairingCodeExpiresInSeconds() {
  if (writePairingCode.length() == 0 || writePairingCodeExpiresMs == 0) return 0;
  long remaining = (long)(writePairingCodeExpiresMs - millis());
  if (remaining <= 0) return 0;
  return remaining / 1000;
}

static void sendPortalPage() {
  String page = F(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Lumentree Local</title><style>"
    "body{font-family:Arial,sans-serif;margin:0;background:#101820;color:#e8edf2}"
    "main{max-width:520px;margin:0 auto;padding:20px}"
    "h1{font-size:22px;margin:10px 0 4px;color:#61d394}"
    "p{color:#a7b4bf;font-size:14px;line-height:1.45}"
    "label{display:block;margin-top:14px;font-size:13px;color:#b9c6cf}"
    "input{width:100%;box-sizing:border-box;padding:11px;margin-top:5px;border:1px solid #314454;border-radius:8px;background:#0a1118;color:#fff;font-size:15px}"
    "button{margin-top:16px;padding:11px 14px;border:0;border-radius:8px;background:#61d394;color:#07100b;font-weight:700;font-size:15px}"
    "button.secondary{background:#263746;color:#d8e2ea;margin-left:8px}"
    "#nets{margin-top:10px;border:1px solid #263746;border-radius:8px;overflow:hidden;display:none}"
    ".net{padding:10px;border-bottom:1px solid #263746;cursor:pointer}.net:last-child{border-bottom:0}.net small{color:#8da0ad;float:right}"
    ".row{display:flex;align-items:center;gap:8px}.row input[type=checkbox]{width:auto}"
    "</style></head><body><main>"
    "<h1>Lumentree Local</h1>"
    "<p>Configure Wi-Fi and production upload. Credentials are stored only in ESP32 NVS.</p>"
    "<div><button type='button' onclick='scan()'>Scan Wi-Fi</button><button class='secondary' type='button' onclick='bleScan()'>Scan BLE</button><button class='secondary' type='button' onclick='status()'>Status</button></div>"
    "<div id='nets'></div>"
    "<div id='ble'></div>"
    "<section style='margin-top:18px;padding:12px;border:1px solid #263746;border-radius:8px;background:#0b141d'>"
    "<h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>Write Access</h2>"
    "<p>Read-only telemetry does not need write access. Generate a one-time code only when Home Assistant asks for write authorization.</p>"
    "<button type='button' onclick='writeCode()'>Generate write pairing code</button><div id='write'></div></section>"
    "<form onsubmit='save(event)'>"
    "<label>Wi-Fi SSID</label><input id='ssid' name='ssid' value='"
  );
  page += htmlEscape(wifiSsid);
  page += F(
    "' required>"
    "<label>Wi-Fi Password</label><input id='pass' name='password' type='password' placeholder='Leave blank to keep current password'>"
    "<label>API URL</label><input id='api_url' name='api_url' value='"
  );
  page += htmlEscape(apiUrl);
  page += F(
    "' required>"
    "<label>API Token</label><input id='api_token' name='api_token' type='password' placeholder='Leave blank to keep current token'>"
    "<label>Device ID</label><input id='device_id' name='device_id' value='"
  );
  page += htmlEscape(deviceId);
  page += F(
    "' required>"
    "<label>Target BLE MAC</label><input id='target_mac' name='target_mac' value='"
  );
  page += htmlEscape(targetMac);
  page += F(
    "'>"
    "<label>Gateway ID</label><input id='gateway_id' name='gateway_id' value='"
  );
  page += htmlEscape(gatewayId);
  page += F(
    "' required>"
    "<label>Upload interval seconds</label><input id='upload_interval' name='upload_interval' type='number' min='5' max='3600' value='"
  );
  page += String(uploadIntervalSeconds);
  page += F(
    "'>"
    "<label class='row'><input id='production' name='production' type='checkbox' "
  );
  if (productionEnabled) page += "checked";
  page += F(
    "> Enable production upload</label>"
    "<button type='submit'>Save & Reboot</button>"
    "</form><pre id='out'></pre>"
    "<script>"
    "function q(id){return document.getElementById(id)}"
    "function scan(){fetch('/api/scan').then(r=>r.json()).then(d=>{let n=q('nets');n.style.display='block';n.innerHTML=d.networks.map(x=>`<div class=net onclick=\"q('ssid').value='${x.ssid.replace(/'/g,'&#39;')}'\">${x.ssid}<small>${x.rssi} dBm ${x.secure?'locked':'open'}</small></div>`).join('')||'<div class=net>No networks</div>'})}"
    "function renderBle(d){let b=q('ble');let list=d.candidates||[];b.innerHTML='<p>Pairing: '+(d.pairing_status||'unknown')+'</p>'+list.map(x=>`<div class=net onclick=\"q('target_mac').value='${x.mac}'\"><b>${x.name||x.mac}</b><small>${x.rssi} dBm score ${x.score}</small><br><span style='color:#8da0ad'>${x.mac}</span></div>`).join('')}"
    "function renderWrite(d){let w=q('write');if(!d.write_pairing_code_active){w.innerHTML='<p>No active write pairing code.</p>';return}if(d.write_pairing_code){w.innerHTML=`<p><b style=\"font-size:24px;color:#61d394;letter-spacing:2px\">${d.write_pairing_code}</b></p><p>Expires in ${d.write_pairing_code_expires_in_seconds}s. Enter this code in Home Assistant Lumentree Local options.</p>`}else{w.innerHTML=`<p>Write pairing code is active and expires in ${d.write_pairing_code_expires_in_seconds}s.</p>`}}"
    "function bleScan(){q('out').textContent='Scanning BLE...';fetch('/api/ble_scan',{method:'POST'}).then(r=>r.json()).then(d=>{renderBle(d);q('out').textContent=JSON.stringify(d,null,2)})}"
    "function status(){fetch('/api/status').then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);renderBle(d);renderWrite(d)})}"
    "function writeCode(){q('out').textContent='Generating write pairing code...';fetch('/api/write_code',{method:'POST'}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);renderWrite(d)})}"
    "function save(e){e.preventDefault();let data={ssid:q('ssid').value,password:q('pass').value,api_url:q('api_url').value,api_token:q('api_token').value,device_id:q('device_id').value,target_mac:q('target_mac').value,gateway_id:q('gateway_id').value,upload_interval:parseInt(q('upload_interval').value||'15'),production:q('production').checked};fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);setTimeout(()=>location.reload(),4000)})}"
    "status();</script></main></body></html>"
  );
  server.send(200, "text/html; charset=utf-8", page);
}

static void setupProvisioningWebServer() {
  server.on("/", HTTP_GET, sendPortalPage);
  server.on("/generate_204", HTTP_GET, sendPortalPage);
  server.on("/fwlink", HTTP_GET, sendPortalPage);
  server.on("/hotspot-detect.html", HTTP_GET, sendPortalPage);

  server.on("/api/status", HTTP_GET, []() {
    JsonDocument doc;
    doc["fw"] = FW_NAME;
    doc["version"] = FW_VERSION;
    doc["mode"] = provisioningPortalActive ? "AP" : "STA";
    doc["ap_ssid"] = provisioningApSsid;
    doc["wifi_configured"] = wifiSsid.length() > 0;
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    doc["api_url"] = apiUrl;
    doc["api_token_configured"] = apiToken.length() > 0;
    doc["device_id"] = deviceId;
    doc["gateway_id"] = gatewayId;
    doc["target_mac"] = targetMac;
    doc["pairing_status"] = pairingStatus;
    doc["candidate_count"] = bleCandidates.size();
    doc["write_pairing_code_active"] = writePairingCodeExpiresInSeconds() > 0;
    doc["write_pairing_code_expires_in_seconds"] = writePairingCodeExpiresInSeconds();
    JsonArray candidates = doc["candidates"].to<JsonArray>();
    addCandidatesJson(candidates);
    doc["upload_interval_seconds"] = uploadIntervalSeconds;
    doc["production_enabled"] = productionEnabled;
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/write_code", HTTP_POST, []() {
    if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0 || deviceId.length() == 0) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"api_url, api_token, gateway_id, and device_id are required\"}");
      return;
    }
    String code = generateWritePairingCode();
    String response;
    if (!postWritePairingCode(code, response)) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["error"] = "failed to register write pairing code with API";
      doc["response"] = response.substring(0, 300);
      String body;
      serializeJson(doc, body);
      server.send(502, "application/json", body);
      return;
    }
    writePairingCode = code;
    writePairingCodeExpiresMs = millis() + WRITE_PAIRING_CODE_TTL_MS;
    JsonDocument doc;
    doc["ok"] = true;
    doc["device_id"] = deviceId;
    doc["gateway_id"] = gatewayId;
    doc["write_pairing_code_active"] = true;
    doc["write_pairing_code"] = writePairingCode;
    doc["write_pairing_code_expires_in_seconds"] = writePairingCodeExpiresInSeconds();
    doc["safety"] = "authorization_only_no_ble_write";
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/scan", HTTP_GET, []() {
    int count = WiFi.scanNetworks(false, true);
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    for (int i = 0; i < count; i++) {
      JsonObject item = networks.add<JsonObject>();
      item["ssid"] = WiFi.SSID(i);
      item["rssi"] = WiFi.RSSI(i);
      item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/ble_scan", HTTP_POST, []() {
    runBleDiscovery(false);
    JsonDocument doc;
    doc["ok"] = true;
    doc["device_id"] = deviceId;
    doc["target_mac"] = targetMac;
    doc["pairing_status"] = pairingStatus;
    JsonArray candidates = doc["candidates"].to<JsonArray>();
    addCandidatesJson(candidates);
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/save", HTTP_POST, []() {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }
    String nextSsid = doc["ssid"] | "";
    String nextPassword = doc["password"] | "";
    String nextApiUrl = doc["api_url"] | "";
    String nextApiToken = doc["api_token"] | "";
    String nextDeviceId = doc["device_id"] | "";
    String nextTargetMac = doc["target_mac"] | "";
    String nextGatewayId = doc["gateway_id"] | "";
    int nextInterval = doc["upload_interval"] | DEFAULT_UPLOAD_INTERVAL_SECONDS;
    bool nextProduction = doc["production"] | false;

    nextSsid.trim();
    nextApiUrl.trim();
    nextApiToken.trim();
    nextDeviceId.trim();
    nextTargetMac = normalizeMac(nextTargetMac);
    nextGatewayId.trim();

    if (nextSsid.length() > 0) {
      wifiSsid = nextSsid;
      saveStringConfig("wifi_ssid", wifiSsid);
    }
    if (nextPassword.length() > 0) {
      wifiPassword = nextPassword;
      saveStringConfig("wifi_pass", wifiPassword);
    }
    if (nextApiUrl.length() > 0) {
      apiUrl = nextApiUrl;
      saveStringConfig("api_url", apiUrl);
    }
    if (nextApiToken.length() > 0) {
      apiToken = nextApiToken;
      saveStringConfig("api_token", apiToken);
    }
    if (nextDeviceId.length() > 0) {
      deviceId = nextDeviceId;
      saveStringConfig("device_id", deviceId);
    }
    targetMac = nextTargetMac;
    targetAddressTypeKnown = targetMac.length() > 0;
    if (targetMac.length() > 0) {
      saveStringConfig("target_mac", targetMac);
      pairingStatus = "paired";
    } else {
      prefs.remove("target_mac");
      pairingStatus = "unconfigured";
    }
    if (nextGatewayId.length() > 0) {
      gatewayId = nextGatewayId;
      saveStringConfig("gateway_id", gatewayId);
    }
    uploadIntervalSeconds = (uint16_t)constrain(nextInterval, MIN_UPLOAD_INTERVAL_SECONDS, 3600);
    prefs.putUShort("upload_s", uploadIntervalSeconds);
    productionEnabled = nextProduction;
    prefs.putBool("prod", productionEnabled);

    server.send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
    delay(500);
    ESP.restart();
  });

  server.onNotFound(sendPortalPage);
  server.begin();
}

static void startProvisioningPortal(bool apStaMode) {
  String mac = WiFi.macAddress();
  String suffix = mac.substring(mac.length() - 5);
  suffix.replace(":", "");
  provisioningApSsid = "Lumentree-" + suffix;
  WiFi.mode(apStaMode ? WIFI_AP_STA : WIFI_AP);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  WiFi.softAP(provisioningApSsid.c_str(), nullptr, 6);
  delay(200);
  dnsServer.start(PROVISIONING_DNS_PORT, "*", WiFi.softAPIP());
  setupProvisioningWebServer();
  provisioningPortalActive = true;

  JsonDocument doc;
  doc["type"] = "provisioning_ap_started";
  doc["uptime_ms"] = millis() - bootMs;
  doc["ssid"] = provisioningApSsid;
  doc["ip"] = WiFi.softAPIP().toString();
  doc["mode"] = apStaMode ? "AP_STA" : "AP";
  printJson(doc);
}

static void handleProvisioningPortal() {
  if (!provisioningPortalActive) return;
  dnsServer.processNextRequest();
  server.handleClient();
}

static void stopProvisioningPortal() {
  if (!provisioningPortalActive) return;
  dnsServer.stop();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  provisioningPortalActive = false;

  JsonDocument doc;
  doc["type"] = "provisioning_ap_stopped";
  doc["uptime_ms"] = millis() - bootMs;
  doc["reason"] = "wifi_connected";
  printJson(doc);
}

static bool ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (wifiSsid.length() == 0) {
    emitError("wifi_not_configured", "set Wi-Fi with SET_WIFI ssid password");
    if (!provisioningPortalActive) startProvisioningPortal(false);
    return false;
  }

  JsonDocument start;
  start["type"] = "wifi_connect_start";
  start["uptime_ms"] = millis() - bootMs;
  start["ssid"] = wifiSsid;
  printJson(start);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  unsigned long deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    feedWatchdog();
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    JsonDocument failed;
    failed["type"] = "wifi_connect_failed";
    failed["uptime_ms"] = millis() - bootMs;
    failed["status"] = WiFi.status();
    printJson(failed);
    if (!provisioningPortalActive) startProvisioningPortal(true);
    return false;
  }

  JsonDocument done;
  done["type"] = "wifi_connected";
  done["uptime_ms"] = millis() - bootMs;
  done["ssid"] = wifiSsid;
  done["ip"] = WiFi.localIP().toString();
  done["rssi"] = WiFi.RSSI();
  printJson(done);
  stopProvisioningPortal();
  return true;
}

static bool postTelemetry(
  const String& payloadHex,
  uint16_t notifyCount,
  size_t payloadLength,
  uint16_t startRegister,
  uint16_t registerCount,
  const char* label,
  const char* safety
) {
  if (apiUrl.length() == 0) {
    emitError("api_url_not_configured", "set API URL with SET_API_URL");
    return false;
  }
  if (apiToken.length() == 0) {
    emitError("api_token_not_configured", "set API token with SET_API_TOKEN");
    return false;
  }
  if (deviceId.length() == 0) {
    emitError("device_id_not_configured", "set device id with SET_DEVICE_ID");
    return false;
  }
  if (!ensureWifiConnected()) return false;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/events";

  JsonDocument doc;
  doc["device_id"] = deviceId;
  doc["mac"] = targetMac;
  doc["gateway_id"] = gatewayId;
  doc["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  JsonObject raw = doc["raw"].to<JsonObject>();
  raw["transport"] = "esp32-ble-wifi";
  raw["payload_hex"] = payloadHex;
  raw["notify_count"] = notifyCount;
  raw["payload_length"] = payloadLength;
  raw["label"] = label;
  raw["start_register"] = startRegister;
  raw["register_count"] = registerCount;
  raw["target_mac"] = targetMac;
  raw["safety"] = safety;
  JsonObject metadata = doc["metadata"].to<JsonObject>();
  metadata["wifi_rssi"] = WiFi.RSSI();
  metadata["ip"] = WiFi.localIP().toString();
  metadata["pairing_status"] = pairingStatus;
  metadata["candidate_count"] = bleCandidates.size();
  metadata["register_label"] = label;
  metadata["start_register"] = startRegister;
  metadata["register_count"] = registerCount;

  String body;
  serializeJson(doc, body);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) {
    emitError("http_begin_failed", "could not initialize HTTP client");
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int status = http.POST((uint8_t*)body.c_str(), body.length());
  String response = http.getString();
  http.end();

  JsonDocument result;
  result["type"] = status >= 200 && status < 300 ? "api_upload_ok" : "api_upload_failed";
  result["uptime_ms"] = millis() - bootMs;
  result["http_status"] = status;
  result["payload_length"] = payloadLength;
  result["notify_count"] = notifyCount;
  result["response"] = response.substring(0, 300);
  printJson(result);
  if (status >= 200 && status < 300) {
    pairingStatus = "paired";
    unsigned long now = millis();
    if (lastGatewayStatusPostMs == 0 || now - lastGatewayStatusPostMs >= HEARTBEAT_INTERVAL_MS) {
      postGatewayStatus("telemetry_upload_ok");
    }
  }
  return status >= 200 && status < 300;
}

static bool postGatewayStatus(const char* reason) {
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0) return false;
  if (!ensureWifiConnected()) return false;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/gateways/status";

  JsonDocument doc;
  doc["gateway_id"] = gatewayId;
  doc["device_id"] = deviceId;
  doc["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  doc["reason"] = reason;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  JsonArray candidates = doc["candidates"].to<JsonArray>();
  addCandidatesJson(candidates);

  String body;
  serializeJson(doc, body);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) return false;

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int status = http.POST((uint8_t*)body.c_str(), body.length());
  String response = http.getString();
  http.end();

  JsonDocument result;
  result["type"] = status >= 200 && status < 300 ? "gateway_status_upload_ok" : "gateway_status_upload_failed";
  result["uptime_ms"] = millis() - bootMs;
  result["http_status"] = status;
  result["pairing_status"] = pairingStatus;
  result["candidate_count"] = bleCandidates.size();
  result["response"] = response.substring(0, 300);
  printJson(result);
  if (status >= 200 && status < 300) {
    lastGatewayStatusPostMs = millis();
  }
  return status >= 200 && status < 300;
}

static bool postWritePairingCode(const String& code, String& response) {
  response = "";
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0 || deviceId.length() == 0) return false;
  if (!ensureWifiConnected()) return false;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/gateways/";
  endpoint += gatewayId;
  endpoint += "/write-pairing-code";

  JsonDocument doc;
  doc["gateway_id"] = gatewayId;
  doc["device_id"] = deviceId;
  doc["code"] = code;
  doc["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  doc["safety"] = "authorization_only_no_ble_write";

  String body;
  serializeJson(doc, body);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) return false;

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.POST((uint8_t*)body.c_str(), body.length());
  response = http.getString();
  http.end();

  JsonDocument log;
  log["type"] = httpStatus >= 200 && httpStatus < 300 ? "write_pairing_code_registered" : "write_pairing_code_failed";
  log["uptime_ms"] = millis() - bootMs;
  log["http_status"] = httpStatus;
  log["safety"] = "authorization_only_no_ble_write";
  log["response"] = response.substring(0, 300);
  printJson(log);
  return httpStatus >= 200 && httpStatus < 300;
}

static bool postCommandResult(uint64_t commandId, const char* status, JsonDocument& resultDoc, const char* error) {
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0) return false;
  if (!ensureWifiConnected()) return false;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/commands/";
  endpoint += String((unsigned long long)commandId);
  endpoint += "/result";

  JsonDocument doc;
  doc["gateway_id"] = gatewayId;
  doc["device_id"] = deviceId;
  doc["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  doc["status"] = status;
  if (error != nullptr && strlen(error) > 0) {
    doc["error"] = error;
  }
  doc["result"] = resultDoc.as<JsonVariant>();

  String body;
  serializeJson(doc, body);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) return false;

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.POST((uint8_t*)body.c_str(), body.length());
  String response = http.getString();
  http.end();

  JsonDocument log;
  log["type"] = httpStatus >= 200 && httpStatus < 300 ? "command_result_upload_ok" : "command_result_upload_failed";
  log["uptime_ms"] = millis() - bootMs;
  log["command_id"] = commandId;
  log["http_status"] = httpStatus;
  log["command_status"] = status;
  log["response"] = response.substring(0, 300);
  log["safety"] = "command_result_upload_only";
  printJson(log);
  return httpStatus >= 200 && httpStatus < 300;
}

static bool runModbusWriteSingleRegisterFunction16(uint16_t registerAddress, uint16_t value, const char* label) {
  if (targetMac.length() == 0) {
    emitError("target_not_set", "run SET_TARGET before writing BLE Modbus data");
    return false;
  }
  if (!targetAddressTypeKnown) {
    emitError("address_type_unknown", "scan the target once before writing BLE Modbus data");
    return false;
  }

  String commandHex = buildWriteSingleRegisterFunction16Command(1, registerAddress, value);
  String expectedAckHex = buildWriteMultipleRegistersAck(1, registerAddress, 1);
  std::string commandBytes;
  if (!hexToBytes(commandHex, commandBytes)) {
    emitError("command_build_failed", "could not build Modbus function 16 write command");
    return false;
  }

  JsonDocument start;
  start["type"] = "ble_modbus_write_multiple_registers_start";
  start["uptime_ms"] = millis() - bootMs;
  start["target_mac"] = targetMac;
  start["address_type"] = targetAddressType;
  start["label"] = label;
  start["register"] = registerAddress;
  start["register_count"] = 1;
  start["value"] = value;
  start["command_hex"] = commandHex;
  start["expected_ack_hex"] = expectedAckHex;
  start["service_uuid"] = LUMENTREE_VENDOR_SERVICE_UUID;
  start["characteristic_uuid"] = LUMENTREE_VENDOR_CHARACTERISTIC_UUID;
  start["safety"] = "semantic_function_16_single_register_write";
  printJson(start);

  if (modbusClient == nullptr) {
    modbusClient = BLEDevice::createClient();
  }
  if (modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(250);
  }

  bool connected = modbusClient->connect(BLEAddress(targetMac.c_str()), targetAddressType);
  if (!connected) {
    emitError("gatt_connect_failed", "could not connect to target for BLE Modbus write");
    return false;
  }

  BLERemoteService* service = modbusClient->getService(BLEUUID(LUMENTREE_VENDOR_SERVICE_UUID));
  if (service == nullptr) {
    emitError("gatt_service_missing", "FFE0 service not found");
    modbusClient->disconnect();
    return false;
  }

  BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(LUMENTREE_VENDOR_CHARACTERISTIC_UUID));
  if (chr == nullptr) {
    emitError("gatt_characteristic_missing", "FFE1 characteristic not found");
    modbusClient->disconnect();
    return false;
  }
  if (!chr->canWrite() && !chr->canWriteNoResponse()) {
    emitError("gatt_write_missing", "FFE1 cannot carry Modbus write request");
    modbusClient->disconnect();
    return false;
  }

  modbusNotifyReceived = false;
  modbusLastNotifyMs = 0;
  modbusNotifyCount = 0;
  modbusResponseBytes.clear();
  if (chr->canNotify() || chr->canIndicate()) {
    chr->registerForNotify(modbusNotifyCallback);
    delay(200);
  }

  JsonDocument sent;
  sent["type"] = "ble_modbus_write_multiple_registers_sent";
  sent["uptime_ms"] = millis() - bootMs;
  sent["target_mac"] = targetMac;
  sent["label"] = label;
  sent["register"] = registerAddress;
  sent["value"] = value;
  sent["command_hex"] = commandHex;
  sent["write_with_response"] = chr->canWrite();
  printJson(sent);

  chr->writeValue((uint8_t*)commandBytes.data(), commandBytes.length(), chr->canWrite());

  unsigned long deadline = millis() + 5000;
  while (millis() < deadline) {
    feedWatchdog();
    delay(50);
    if (modbusNotifyReceived && millis() - modbusLastNotifyMs > 600) break;
  }

  String responseHex = "";
  if (modbusResponseBytes.length() > 0) {
    responseHex = bytesToHex(modbusResponseBytes);
  }
  bool ackMatches = responseHex == expectedAckHex || responseHex.endsWith(expectedAckHex);

  JsonDocument done;
  done["type"] = "ble_modbus_write_multiple_registers_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["label"] = label;
  done["register"] = registerAddress;
  done["value"] = value;
  done["response_hex"] = responseHex;
  done["ack_matches"] = ackMatches;
  done["notify_count"] = modbusNotifyCount;
  done["safety"] = "function_16_semantic_single_register_write_done";
  printJson(done);

  delay(200);
  modbusClient->disconnect();
  delay(500);
  return ackMatches;
}

static uint16_t dischargeTargetSocRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 144;
    case 2: return 146;
    case 3: return 177;
    case 4: return 178;
    default: return 0;
  }
}

static bool runSetDischargeTargetSoc(int slot, uint16_t requestedValue, JsonDocument& result, String& errorMessage) {
  uint16_t targetSocRegister = dischargeTargetSocRegisterForSlot(slot);
  if (targetSocRegister == 0) {
    errorMessage = "invalid discharge target SOC slot (must be 1..4)";
    result["verified"] = false;
    return false;
  }

  result["dry_run"] = false;
  result["ble_write"] = true;
  result["modbus_write"] = true;
  result["write_enabled"] = true;
  result["would_execute"] = true;
  result["gateway_id"] = gatewayId;
  result["device_id"] = deviceId;
  result["target_mac"] = targetMac;
  result["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  result["register"] = targetSocRegister;
  result["slot"] = slot;
  result["requested_value"] = requestedValue;
  result["safety"] = "semantic_target_soc_pre_read_function_16_post_read";

  if (requestedValue < 5 || requestedValue > 100) {
    errorMessage = "target_soc must be 5..100";
    result["verified"] = false;
    return false;
  }

  String preLabel = "pre_read_register_" + String(targetSocRegister) + "_before_target_soc_write";
  ModbusReadResult beforeRead = runModbusRead(targetSocRegister, 1, preLabel.c_str());
  uint16_t beforeValue = 0;
  if (!readRegisterFromResult(beforeRead, targetSocRegister, targetSocRegister, beforeValue)) {
    errorMessage = "could not read register " + String(targetSocRegister) + " before write";
    result["verified"] = false;
    return false;
  }
  result["before_value"] = beforeValue;

  if (beforeValue == requestedValue) {
    result["after_value"] = beforeValue;
    result["verified"] = true;
    result["safety"] = "target_soc_already_at_requested_value_no_write_sent";
    return true;
  }

  String writeLabel = "set_discharge_target_soc_slot_" + String(slot) + "_register_" + String(targetSocRegister);
  bool writeAck = runModbusWriteSingleRegisterFunction16(targetSocRegister, requestedValue, writeLabel.c_str());
  result["write_ack"] = writeAck;
  if (!writeAck) {
    emitError("target_soc_write_ack_failed", "function 16 ACK not observed; verification will still run");
  }

  delay(1000);

  String postLabel = "post_read_register_" + String(targetSocRegister) + "_after_target_soc_write";
  ModbusReadResult afterRead = runModbusRead(targetSocRegister, 1, postLabel.c_str());
  uint16_t afterValue = 0;
  if (!readRegisterFromResult(afterRead, targetSocRegister, targetSocRegister, afterValue)) {
    errorMessage = "could not read register " + String(targetSocRegister) + " after write";
    result["verified"] = false;
    return false;
  }

  result["after_value"] = afterValue;
  result["verified"] = afterValue == requestedValue;
  if (afterValue != requestedValue) {
    errorMessage = "register " + String(targetSocRegister) + " did not verify requested target_soc";
    return false;
  }
  return true;
}

static bool runSetFirstDischargeTargetSoc(uint16_t requestedValue, JsonDocument& result, String& errorMessage) {
  return runSetDischargeTargetSoc(1, requestedValue, result, errorMessage);
}

static uint16_t dischargeTimeEnableRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 135;
    case 2: return 137;
    case 3: return 151;
    case 4: return 152;
    default: return 0;
  }
}

static bool runSetDischargeTimeEnable(int slot, uint16_t enabled, JsonDocument& result, String& errorMessage) {
  uint16_t reg = dischargeTimeEnableRegisterForSlot(slot);
  if (reg == 0) {
    errorMessage = "invalid discharge time slot (must be 1..4)";
    result["verified"] = false;
    return false;
  }

  result["dry_run"] = false;
  result["ble_write"] = true;
  result["modbus_write"] = true;
  result["write_enabled"] = true;
  result["would_execute"] = true;
  result["gateway_id"] = gatewayId;
  result["device_id"] = deviceId;
  result["target_mac"] = targetMac;
  result["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  result["register"] = reg;
  result["slot"] = slot;
  result["requested_value"] = enabled;
  result["safety"] = "semantic_discharge_time_enable_pre_read_function_16_post_read";

  if (enabled > 1) {
    errorMessage = "enabled must be 0 or 1";
    result["verified"] = false;
    return false;
  }

  String preLabel = "pre_read_register_" + String(reg) + "_before_enable_write";
  ModbusReadResult beforeRead = runModbusRead(reg, 1, preLabel.c_str());
  uint16_t beforeValue = 0;
  if (!readRegisterFromResult(beforeRead, reg, reg, beforeValue)) {
    errorMessage = "could not read register " + String(reg) + " before write";
    result["verified"] = false;
    return false;
  }
  result["before_value"] = beforeValue;

  if (beforeValue == enabled) {
    result["after_value"] = beforeValue;
    result["verified"] = true;
    result["safety"] = "discharge_time_enable_already_at_requested_value_no_write_sent";
    return true;
  }

  String writeLabel = "set_discharge_time_enable_slot_" + String(slot) + "_register_" + String(reg);
  bool writeAck = runModbusWriteSingleRegisterFunction16(reg, enabled, writeLabel.c_str());
  result["write_ack"] = writeAck;
  if (!writeAck) {
    emitError("discharge_time_enable_write_ack_failed", "function 16 ACK not observed; verification will still run");
  }

  delay(1000);

  String postLabel = "post_read_register_" + String(reg) + "_after_enable_write";
  ModbusReadResult afterRead = runModbusRead(reg, 1, postLabel.c_str());
  uint16_t afterValue = 0;
  if (!readRegisterFromResult(afterRead, reg, reg, afterValue)) {
    errorMessage = "could not read register " + String(reg) + " after write";
    result["verified"] = false;
    return false;
  }

  result["after_value"] = afterValue;
  result["verified"] = afterValue == enabled;
  if (afterValue != enabled) {
    errorMessage = "register " + String(reg) + " did not verify requested enable value";
    return false;
  }
  return true;
}

static uint16_t dischargePowerRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 180;
    case 2: return 182;
    case 3: return 183;
    case 4: return 184;
    default: return 0;
  }
}

static uint16_t dischargeTimeStartRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 131;
    case 2: return 133;
    case 3: return 173;
    case 4: return 175;
    default: return 0;
  }
}

static uint16_t dischargeTimeEndRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 139;
    case 2: return 141;
    case 3: return 174;
    case 4: return 176;
    default: return 0;
  }
}

static bool isValidHhmm(uint16_t value) {
  uint16_t hours = value / 100;
  uint16_t minutes = value % 100;
  return hours <= 23 && minutes <= 59;
}

static uint16_t mainsChargeTimeEnableRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 134;
    case 2: return 136;
    default: return 0;
  }
}

static uint16_t mainsChargeTimeStartRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 130;
    case 2: return 132;
    default: return 0;
  }
}

static uint16_t mainsChargeTimeEndRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 138;
    case 2: return 140;
    default: return 0;
  }
}

static uint16_t mainsChargeTargetSocRegisterForSlot(int slot) {
  switch (slot) {
    case 1: return 143;
    case 2: return 145;
    default: return 0;
  }
}

static void prepareSemanticWriteResult(JsonDocument& result, uint16_t reg, int slot, uint16_t requestedValue, const char* safety) {
  result["dry_run"] = false;
  result["ble_write"] = true;
  result["modbus_write"] = true;
  result["write_enabled"] = true;
  result["would_execute"] = true;
  result["gateway_id"] = gatewayId;
  result["device_id"] = deviceId;
  result["target_mac"] = targetMac;
  result["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  result["register"] = reg;
  result["slot"] = slot;
  result["requested_value"] = requestedValue;
  result["safety"] = safety;
}

static bool writeSemanticSingleRegister(
  uint16_t reg,
  uint16_t requestedValue,
  const char* writeLabel,
  const char* preLabelSuffix,
  const char* postLabelSuffix,
  JsonDocument& result,
  String& errorMessage
) {
  String preLabel = "pre_read_register_" + String(reg) + preLabelSuffix;
  ModbusReadResult beforeRead = runModbusRead(reg, 1, preLabel.c_str());
  uint16_t beforeValue = 0;
  if (!readRegisterFromResult(beforeRead, reg, reg, beforeValue)) {
    errorMessage = "could not read register " + String(reg) + " before write";
    result["verified"] = false;
    return false;
  }
  result["before_value"] = beforeValue;

  if (beforeValue == requestedValue) {
    result["after_value"] = beforeValue;
    result["verified"] = true;
    result["safety"] = "requested_value_already_set_no_write_sent";
    return true;
  }

  bool writeAck = runModbusWriteSingleRegisterFunction16(reg, requestedValue, writeLabel);
  result["write_ack"] = writeAck;
  if (!writeAck) {
    emitError("semantic_write_ack_failed", "function 16 ACK not observed; verification will still run");
  }

  delay(1000);

  String postLabel = "post_read_register_" + String(reg) + postLabelSuffix;
  ModbusReadResult afterRead = runModbusRead(reg, 1, postLabel.c_str());
  uint16_t afterValue = 0;
  if (!readRegisterFromResult(afterRead, reg, reg, afterValue)) {
    errorMessage = "could not read register " + String(reg) + " after write";
    result["verified"] = false;
    return false;
  }

  result["after_value"] = afterValue;
  result["verified"] = afterValue == requestedValue;
  if (afterValue != requestedValue) {
    errorMessage = "register " + String(reg) + " did not verify requested value";
    return false;
  }
  return true;
}

static bool runSetMainsChargeTimeEnable(int slot, uint16_t enabled, JsonDocument& result, String& errorMessage) {
  uint16_t reg = mainsChargeTimeEnableRegisterForSlot(slot);
  if (reg == 0) {
    errorMessage = "invalid mains charge slot (must be 1..2)";
    result["verified"] = false;
    return false;
  }
  prepareSemanticWriteResult(result, reg, slot, enabled, "semantic_mains_charge_time_enable_pre_read_function_16_post_read");

  if (enabled > 1) {
    errorMessage = "enabled must be 0 or 1";
    result["verified"] = false;
    return false;
  }

  String writeLabel = "set_mains_charge_time_enable_slot_" + String(slot) + "_register_" + String(reg);
  return writeSemanticSingleRegister(
    reg,
    enabled,
    writeLabel.c_str(),
    "_before_mains_charge_enable_write",
    "_after_mains_charge_enable_write",
    result,
    errorMessage
  );
}

static bool runSetMainsChargeTargetSoc(int slot, uint16_t requestedValue, JsonDocument& result, String& errorMessage) {
  uint16_t reg = mainsChargeTargetSocRegisterForSlot(slot);
  if (reg == 0) {
    errorMessage = "invalid mains charge slot (must be 1..2)";
    result["verified"] = false;
    return false;
  }
  prepareSemanticWriteResult(result, reg, slot, requestedValue, "semantic_mains_charge_target_soc_pre_read_function_16_post_read");

  if (requestedValue < 5 || requestedValue > 100) {
    errorMessage = "target_soc must be 5..100";
    result["verified"] = false;
    return false;
  }

  String writeLabel = "set_mains_charge_target_soc_slot_" + String(slot) + "_register_" + String(reg);
  return writeSemanticSingleRegister(
    reg,
    requestedValue,
    writeLabel.c_str(),
    "_before_mains_charge_target_soc_write",
    "_after_mains_charge_target_soc_write",
    result,
    errorMessage
  );
}

static bool runSetMainsChargeTimeValue(
  int slot,
  uint16_t requestedTime,
  bool isStart,
  JsonDocument& result,
  String& errorMessage
) {
  uint16_t reg = isStart ? mainsChargeTimeStartRegisterForSlot(slot) : mainsChargeTimeEndRegisterForSlot(slot);
  const char* fieldName = isStart ? "start" : "end";
  if (reg == 0) {
    errorMessage = "invalid mains charge slot (must be 1..2)";
    result["verified"] = false;
    return false;
  }
  prepareSemanticWriteResult(
    result,
    reg,
    slot,
    requestedTime,
    isStart
      ? "semantic_mains_charge_time_start_pre_read_function_16_post_read"
      : "semantic_mains_charge_time_end_pre_read_function_16_post_read"
  );
  result["field"] = fieldName;

  if (!isValidHhmm(requestedTime)) {
    errorMessage = "time must be a valid HHMM value from 0000 to 2359";
    result["verified"] = false;
    return false;
  }

  String writeLabel = "set_mains_charge_time_" + String(fieldName) + "_slot_" + String(slot) + "_register_" + String(reg);
  return writeSemanticSingleRegister(
    reg,
    requestedTime,
    writeLabel.c_str(),
    isStart ? "_before_mains_charge_time_start_write" : "_before_mains_charge_time_end_write",
    isStart ? "_after_mains_charge_time_start_write" : "_after_mains_charge_time_end_write",
    result,
    errorMessage
  );
}

static bool runSetDischargePower(int slot, uint16_t requestedPower, JsonDocument& result, String& errorMessage) {
  uint16_t reg = dischargePowerRegisterForSlot(slot);
  if (reg == 0) {
    errorMessage = "invalid discharge power slot (must be 1..4)";
    result["verified"] = false;
    return false;
  }

  result["dry_run"] = false;
  result["ble_write"] = true;
  result["modbus_write"] = true;
  result["write_enabled"] = true;
  result["would_execute"] = true;
  result["gateway_id"] = gatewayId;
  result["device_id"] = deviceId;
  result["target_mac"] = targetMac;
  result["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  result["register"] = reg;
  result["slot"] = slot;
  result["requested_value"] = requestedPower;
  result["safety"] = "semantic_discharge_power_pre_read_function_16_post_read";

  if (requestedPower < 500 || requestedPower > 5000) {
    errorMessage = "discharge power must be 500..5000 W";
    result["verified"] = false;
    return false;
  }

  String preLabel = "pre_read_register_" + String(reg) + "_before_discharge_power_write";
  ModbusReadResult beforeRead = runModbusRead(reg, 1, preLabel.c_str());
  uint16_t beforeValue = 0;
  if (!readRegisterFromResult(beforeRead, reg, reg, beforeValue)) {
    errorMessage = "could not read register " + String(reg) + " before write";
    result["verified"] = false;
    return false;
  }
  result["before_value"] = beforeValue;

  if (beforeValue == requestedPower) {
    result["after_value"] = beforeValue;
    result["verified"] = true;
    result["safety"] = "discharge_power_already_at_requested_value_no_write_sent";
    return true;
  }

  String writeLabel = "set_discharge_power_slot_" + String(slot) + "_register_" + String(reg);
  bool writeAck = runModbusWriteSingleRegisterFunction16(reg, requestedPower, writeLabel.c_str());
  result["write_ack"] = writeAck;
  if (!writeAck) {
    emitError("discharge_power_write_ack_failed", "function 16 ACK not observed; verification will still run");
  }

  delay(1000);

  String postLabel = "post_read_register_" + String(reg) + "_after_discharge_power_write";
  ModbusReadResult afterRead = runModbusRead(reg, 1, postLabel.c_str());
  uint16_t afterValue = 0;
  if (!readRegisterFromResult(afterRead, reg, reg, afterValue)) {
    errorMessage = "could not read register " + String(reg) + " after write";
    result["verified"] = false;
    return false;
  }

  result["after_value"] = afterValue;
  result["verified"] = afterValue == requestedPower;
  if (afterValue != requestedPower) {
    errorMessage = "register " + String(reg) + " did not verify requested discharge power";
    return false;
  }
  return true;
}

static bool runSetDischargeTimeValue(
  int slot,
  uint16_t requestedTime,
  bool isStart,
  JsonDocument& result,
  String& errorMessage
) {
  uint16_t reg = isStart ? dischargeTimeStartRegisterForSlot(slot) : dischargeTimeEndRegisterForSlot(slot);
  const char* fieldName = isStart ? "start" : "end";
  if (reg == 0) {
    errorMessage = "invalid discharge time slot (must be 1..4)";
    result["verified"] = false;
    return false;
  }

  result["dry_run"] = false;
  result["ble_write"] = true;
  result["modbus_write"] = true;
  result["write_enabled"] = true;
  result["would_execute"] = true;
  result["gateway_id"] = gatewayId;
  result["device_id"] = deviceId;
  result["target_mac"] = targetMac;
  result["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  result["register"] = reg;
  result["slot"] = slot;
  result["field"] = fieldName;
  result["requested_value"] = requestedTime;
  result["safety"] = isStart
    ? "semantic_discharge_time_start_pre_read_function_16_post_read"
    : "semantic_discharge_time_end_pre_read_function_16_post_read";

  if (!isValidHhmm(requestedTime)) {
    errorMessage = "time must be a valid HHMM value from 0000 to 2359";
    result["verified"] = false;
    return false;
  }

  String preLabel = "pre_read_register_" + String(reg) + "_before_discharge_time_" + fieldName + "_write";
  ModbusReadResult beforeRead = runModbusRead(reg, 1, preLabel.c_str());
  uint16_t beforeValue = 0;
  if (!readRegisterFromResult(beforeRead, reg, reg, beforeValue)) {
    errorMessage = "could not read register " + String(reg) + " before write";
    result["verified"] = false;
    return false;
  }
  result["before_value"] = beforeValue;

  if (beforeValue == requestedTime) {
    result["after_value"] = beforeValue;
    result["verified"] = true;
    result["safety"] = isStart
      ? "discharge_time_start_already_at_requested_value_no_write_sent"
      : "discharge_time_end_already_at_requested_value_no_write_sent";
    return true;
  }

  String writeLabel = "set_discharge_time_" + String(fieldName) + "_slot_" + String(slot) + "_register_" + String(reg);
  bool writeAck = runModbusWriteSingleRegisterFunction16(reg, requestedTime, writeLabel.c_str());
  result["write_ack"] = writeAck;
  if (!writeAck) {
    emitError("discharge_time_write_ack_failed", "function 16 ACK not observed; verification will still run");
  }

  delay(1000);

  String postLabel = "post_read_register_" + String(reg) + "_after_discharge_time_" + fieldName + "_write";
  ModbusReadResult afterRead = runModbusRead(reg, 1, postLabel.c_str());
  uint16_t afterValue = 0;
  if (!readRegisterFromResult(afterRead, reg, reg, afterValue)) {
    errorMessage = "could not read register " + String(reg) + " after write";
    result["verified"] = false;
    return false;
  }

  result["after_value"] = afterValue;
  result["verified"] = afterValue == requestedTime;
  if (afterValue != requestedTime) {
    errorMessage = "register " + String(reg) + " did not verify requested discharge time";
    return false;
  }
  return true;
}

static void executeCommand(JsonObject command) {
  uint64_t commandId = command["id"] | 0ULL;
  const char* commandName = command["command"] | "";
  const char* mode = command["mode"] | "";
  const char* commandDeviceId = command["device_id"] | "";

  JsonDocument result;
  result["ble_write"] = false;
  result["modbus_write"] = false;
  result["write_enabled"] = false;
  result["would_execute"] = false;
  result["gateway_id"] = gatewayId;
  result["device_id"] = deviceId;
  result["target_mac"] = targetMac;
  result["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  result["safety"] = "no_ble_write_function_called";

  JsonDocument log;
  log["type"] = "command_received";
  log["uptime_ms"] = millis() - bootMs;
  log["command_id"] = commandId;
  log["command"] = commandName;
  log["mode"] = mode;
  log["device_id"] = commandDeviceId;
  log["safety"] = "semantic_write_only_no_generic_register_writer";
  printJson(log);

  if (commandId <= 0 || String(commandDeviceId) != deviceId) {
    postCommandResult(commandId, "rejected", result, "invalid command id or device mismatch");
    return;
  }

  if (strcmp(mode, "dry_run") == 0 && strcmp(commandName, "dry_run_noop") == 0) {
    result["dry_run"] = true;
    postCommandResult(commandId, "dry_run_completed", result, nullptr);
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_first_discharge_target_soc") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int targetSoc = payload["target_soc"] | -1;
    String errorMessage;
    bool ok = runSetFirstDischargeTargetSoc((uint16_t)targetSoc, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_discharge_target_soc") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int targetSoc = payload["target_soc"] | -1;
    String errorMessage;
    bool ok = runSetDischargeTargetSoc(slot, (uint16_t)targetSoc, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_discharge_power") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int power = payload["power"] | -1;
    String errorMessage;
    bool ok = runSetDischargePower(slot, (uint16_t)power, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_discharge_time_enable") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int enabled = payload["enabled"] | -1;
    String errorMessage;
    bool ok = runSetDischargeTimeEnable(slot, (uint16_t)enabled, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (
    strcmp(mode, "write") == 0
    && (
      strcmp(commandName, "set_discharge_time_start") == 0
      || strcmp(commandName, "set_discharge_time_end") == 0
    )
  ) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int timeValue = payload["time"] | -1;
    String errorMessage;
    bool isStart = strcmp(commandName, "set_discharge_time_start") == 0;
    bool ok = runSetDischargeTimeValue(slot, (uint16_t)timeValue, isStart, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_mains_charge_target_soc") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int targetSoc = payload["target_soc"] | -1;
    String errorMessage;
    bool ok = runSetMainsChargeTargetSoc(slot, (uint16_t)targetSoc, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_mains_charge_time_enable") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int enabled = payload["enabled"] | -1;
    String errorMessage;
    bool ok = runSetMainsChargeTimeEnable(slot, (uint16_t)enabled, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  if (
    strcmp(mode, "write") == 0
    && (
      strcmp(commandName, "set_mains_charge_time_start") == 0
      || strcmp(commandName, "set_mains_charge_time_end") == 0
    )
  ) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int timeValue = payload["time"] | -1;
    String errorMessage;
    bool isStart = strcmp(commandName, "set_mains_charge_time_start") == 0;
    bool ok = runSetMainsChargeTimeValue(slot, (uint16_t)timeValue, isStart, result, errorMessage);
    postCommandResult(commandId, ok ? "completed" : "failed", result, ok ? nullptr : errorMessage.c_str());
    return;
  }

  postCommandResult(commandId, "rejected", result, "unsupported command");
}

static void pollPendingCommand() {
  if (!productionEnabled) return;
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0 || deviceId.length() == 0) return;
  unsigned long now = millis();
  if (lastCommandPollMs != 0 && now - lastCommandPollMs < COMMAND_POLL_INTERVAL_MS) return;
  lastCommandPollMs = now;
  if (!ensureWifiConnected()) return;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/gateways/";
  endpoint += gatewayId;
  endpoint += "/commands/next?device_id=";
  endpoint += deviceId;

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) return;

  http.setTimeout(8000);
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.GET();
  String response = http.getString();
  http.end();

  if (httpStatus < 200 || httpStatus >= 300) {
    JsonDocument log;
    log["type"] = "command_poll_failed";
    log["uptime_ms"] = millis() - bootMs;
    log["http_status"] = httpStatus;
    log["response"] = response.substring(0, 300);
    printJson(log);
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    emitError("command_poll_invalid_json", "command poll returned invalid JSON");
    return;
  }

  JsonVariant commandValue = doc["command"];
  if (commandValue.isNull()) {
    return;
  }
  if (!commandValue.is<JsonObject>()) {
    emitError("command_poll_invalid_command", "command payload is invalid");
    return;
  }

  executeCommand(commandValue.as<JsonObject>());
}

static void emitModbusNotify(BLERemoteCharacteristic* chr, uint8_t* data, size_t length, bool isNotify) {
  modbusNotifyReceived = true;
  modbusLastNotifyMs = millis();
  modbusNotifyCount++;
  modbusResponseBytes.append((const char*)data, length);

  JsonDocument doc;
  doc["type"] = isNotify ? "ble_modbus_notify" : "ble_modbus_indicate";
  doc["uptime_ms"] = millis() - bootMs;
  doc["target_mac"] = targetMac;
  doc["service_uuid"] = LUMENTREE_VENDOR_SERVICE_UUID;
  doc["characteristic_uuid"] = chr != nullptr ? chr->getUUID().toString().c_str() : LUMENTREE_VENDOR_CHARACTERISTIC_UUID;
  doc["payload_hex"] = bytesToHex(data, length);
  doc["length"] = length;
  doc["safety"] = "read_request_response_only_no_setting_write";
  printJson(doc);
}

static void modbusNotifyCallback(
  BLERemoteCharacteristic* chr,
  uint8_t* data,
  size_t length,
  bool isNotify
) {
  emitModbusNotify(chr, data, length, isNotify);
}

static bool hasUsefulPayload(BLEAdvertisedDevice& device) {
  return device.haveName() ||
         device.haveManufacturerData() ||
         device.haveServiceData() ||
         device.getServiceUUIDCount() > 0;
}

static bool serviceUuidMatches(BLEAdvertisedDevice& device, const char* uuid) {
  String expected = normalizeUuid(uuid);
  for (int i = 0; i < device.getServiceUUIDCount(); i++) {
    String actual = normalizeUuid(device.getServiceUUID(i).toString().c_str());
    if (actual == expected) return true;
  }
  for (int i = 0; i < device.getServiceDataUUIDCount(); i++) {
    String actual = normalizeUuid(device.getServiceDataUUID(i).toString().c_str());
    if (actual == expected) return true;
  }
  return false;
}

static bool advertisedNameMatchesDeviceId(const String& name) {
  if (deviceId.length() == 0 || name.length() == 0) return false;
  String lowerName = name;
  String lowerDeviceId = deviceId;
  lowerName.toLowerCase();
  lowerDeviceId.toLowerCase();
  return lowerName.indexOf(lowerDeviceId) >= 0;
}

static int candidateScore(bool nameMatch, bool serviceUuidMatch, bool vendorGattMatch, int rssi) {
  int score = 0;
  if (nameMatch) score += 100;
  if (serviceUuidMatch) score += 60;
  if (vendorGattMatch) score += 40;
  if (rssi > -65) score += 15;
  else if (rssi > -80) score += 5;
  return score;
}

static void rememberCandidate(BLEAdvertisedDevice& device, bool nameMatch, bool serviceUuidMatch, bool vendorGattMatch) {
  if (!nameMatch && !serviceUuidMatch && !vendorGattMatch) return;

  String mac = normalizeMac(device.getAddress().toString().c_str());
  int rssi = device.getRSSI();
  int score = candidateScore(nameMatch, serviceUuidMatch, vendorGattMatch, rssi);

  for (BleCandidate& candidate : bleCandidates) {
    if (candidate.mac != mac) continue;
    candidate.name = device.haveName() ? device.getName().c_str() : candidate.name;
    candidate.rssi = rssi;
    candidate.addressType = device.getAddressType();
    candidate.nameMatch = candidate.nameMatch || nameMatch;
    candidate.serviceUuidMatch = candidate.serviceUuidMatch || serviceUuidMatch;
    candidate.vendorGattMatch = candidate.vendorGattMatch || vendorGattMatch;
    candidate.score = max(candidate.score, score);
    candidate.lastSeenMs = millis() - bootMs;
    return;
  }

  BleCandidate candidate;
  candidate.mac = mac;
  candidate.name = device.haveName() ? device.getName().c_str() : "";
  candidate.rssi = rssi;
  candidate.addressType = device.getAddressType();
  candidate.nameMatch = nameMatch;
  candidate.serviceUuidMatch = serviceUuidMatch;
  candidate.vendorGattMatch = vendorGattMatch;
  candidate.score = score;
  candidate.lastSeenMs = millis() - bootMs;
  bleCandidates.push_back(candidate);
}

static void addCandidatesJson(JsonArray array) {
  for (const BleCandidate& candidate : bleCandidates) {
    JsonObject item = array.add<JsonObject>();
    item["mac"] = candidate.mac;
    item["name"] = candidate.name;
    item["rssi"] = candidate.rssi;
    item["address_type"] = candidate.addressType;
    item["name_match"] = candidate.nameMatch;
    item["service_uuid_match"] = candidate.serviceUuidMatch;
    item["vendor_gatt_match"] = candidate.vendorGattMatch;
    item["score"] = candidate.score;
    item["last_seen_ms"] = candidate.lastSeenMs;
  }
}

static void emitAdvertisement(BLEAdvertisedDevice& device) {
  if (!scanActive) return;
  if (loggedThisScan >= logLimit) return;

  String mac = normalizeMac(device.getAddress().toString().c_str());
  if (targetMac.length() > 0 && mac != targetMac) return;
  if (targetMac.length() == 0 && !hasUsefulPayload(device)) return;

  String name = device.haveName() ? device.getName().c_str() : "";
  bool nameMatch = advertisedNameMatchesDeviceId(name);
  bool uuidMatch = serviceUuidMatches(device, KNOWN_LUMENTREE_SERVICE_UUID);
  bool vendorGattMatch = serviceUuidMatches(device, LUMENTREE_VENDOR_SERVICE_UUID) ||
                         serviceUuidMatches(device, "ffe0") ||
                         serviceUuidMatches(device, "ffe1");
  bool lumentreeCandidate = nameMatch || uuidMatch || vendorGattMatch;
  if (candidateOnly && !lumentreeCandidate) return;

  rememberCandidate(device, nameMatch, uuidMatch, vendorGattMatch);

  if ((targetMac.length() > 0 && mac == targetMac) || lumentreeCandidate) {
    targetAddressType = device.getAddressType();
    targetAddressTypeKnown = true;
  }

  JsonDocument doc;
  doc["type"] = "adv";
  doc["uptime_ms"] = millis() - bootMs;
  doc["mac"] = mac;
  doc["rssi"] = device.getRSSI();
  doc["name"] = name;
  doc["lumentree_candidate"] = lumentreeCandidate;
  doc["candidate_name_match"] = nameMatch;
  doc["candidate_uuid_match"] = uuidMatch;
  doc["candidate_vendor_gatt_match"] = vendorGattMatch;
  doc["candidate_score"] = candidateScore(nameMatch, uuidMatch, vendorGattMatch, device.getRSSI());
  doc["address_type"] = device.getAddressType();
  doc["payload_hex"] = bytesToHex(device.getPayload(), device.getPayloadLength());
  doc["manufacturer_hex"] = device.haveManufacturerData()
    ? bytesToHex(device.getManufacturerData())
    : "";

  JsonArray services = doc["service_uuids"].to<JsonArray>();
  for (int i = 0; i < device.getServiceUUIDCount(); i++) {
    services.add(device.getServiceUUID(i).toString().c_str());
  }

  JsonArray serviceData = doc["service_data"].to<JsonArray>();
  for (int i = 0; i < device.getServiceDataCount(); i++) {
    JsonObject item = serviceData.add<JsonObject>();
    item["uuid"] = device.getServiceDataUUID(i).toString().c_str();
    item["hex"] = bytesToHex(device.getServiceData(i));
  }

  printJson(doc);
  loggedThisScan++;
}

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) override {
    emitAdvertisement(device);
  }
};

static void runScanOnce() {
  if (bleScan == nullptr) {
    emitError("ble_not_ready", "BLE scanner is not initialized");
    return;
  }
  if (scanWindow == 0 || scanInterval == 0 || scanWindow > scanInterval) {
    emitError("bad_scan_params", "scan window must be >0 and <= interval");
    return;
  }

  JsonDocument start;
  start["type"] = "scan_start";
  start["uptime_ms"] = millis() - bootMs;
  start["target_mac"] = targetMac;
  start["seconds"] = scanSeconds;
  start["interval"] = scanInterval;
  start["window"] = scanWindow;
  start["active_scan"] = activeScan;
  start["candidate_only"] = candidateOnly;
  printJson(start);

  loggedThisScan = 0;
  scanActive = true;
  bleScan->setActiveScan(activeScan);
  bleScan->setInterval(scanInterval);
  bleScan->setWindow(scanWindow);
  BLEScanResults results = bleScan->start(scanSeconds, false);
  scanActive = false;
  bleScan->clearResults();

  JsonDocument done;
  done["type"] = "scan_done";
  done["uptime_ms"] = millis() - bootMs;
  done["found"] = results.getCount();
  done["logged"] = loggedThisScan;
  printJson(done);
}

static void emitBleCandidates(const char* type) {
  JsonDocument doc;
  doc["type"] = type;
  doc["uptime_ms"] = millis() - bootMs;
  doc["device_id"] = deviceId;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  JsonArray candidates = doc["candidates"].to<JsonArray>();
  addCandidatesJson(candidates);
  printJson(doc);
}

static bool runBleDiscovery(bool allowAutoBind) {
  if (bleScan == nullptr) {
    emitError("ble_not_ready", "BLE scanner is not initialized");
    return false;
  }
  if (deviceId.length() == 0) {
    pairingStatus = "unconfigured";
    emitError("device_id_not_configured", "set device id before BLE discovery");
    return false;
  }

  pairingStatus = "scanning";
  bleCandidates.clear();

  bool previousActiveScan = activeScan;
  bool previousCandidateOnly = candidateOnly;
  uint8_t previousScanSeconds = scanSeconds;
  uint16_t previousLogLimit = logLimit;

  activeScan = true;
  candidateOnly = true;
  if (scanSeconds < 5) scanSeconds = 5;
  if (logLimit < 120) logLimit = 120;
  runScanOnce();

  activeScan = previousActiveScan;
  candidateOnly = previousCandidateOnly;
  scanSeconds = previousScanSeconds;
  logLimit = previousLogLimit;

  if (bleCandidates.size() == 0) {
    pairingStatus = "scanning_no_candidate";
    emitBleCandidates("ble_candidates");
    postGatewayStatus("ble_discovery_no_candidate");
    return false;
  }

  if (allowAutoBind && bleCandidates.size() == 1) {
    const BleCandidate& candidate = bleCandidates[0];
    targetMac = candidate.mac;
    targetAddressType = (esp_ble_addr_type_t)candidate.addressType;
    targetAddressTypeKnown = true;
    saveStringConfig("target_mac", targetMac);
    pairingStatus = "paired";

    JsonDocument doc;
    doc["type"] = "ble_auto_paired";
    doc["uptime_ms"] = millis() - bootMs;
    doc["device_id"] = deviceId;
    doc["target_mac"] = targetMac;
    doc["name"] = candidate.name;
    doc["rssi"] = candidate.rssi;
    doc["score"] = candidate.score;
    printJson(doc);
    postGatewayStatus("ble_auto_paired");
    return true;
  }

  pairingStatus = "multiple_candidates";
  emitBleCandidates("ble_candidates");
  postGatewayStatus("ble_discovery_multiple_candidates");
  return false;
}

static void emitCharacteristic(BLERemoteService* service, BLERemoteCharacteristic* chr) {
  JsonDocument doc;
  doc["type"] = "gatt_characteristic";
  doc["uptime_ms"] = millis() - bootMs;
  doc["target_mac"] = targetMac;
  doc["service_uuid"] = service->getUUID().toString().c_str();
  doc["uuid"] = chr->getUUID().toString().c_str();
  doc["handle"] = chr->getHandle();
  doc["broadcast"] = chr->canBroadcast();
  doc["read"] = chr->canRead();
  doc["write"] = chr->canWrite();
  doc["write_no_response"] = chr->canWriteNoResponse();
  doc["notify"] = chr->canNotify();
  doc["indicate"] = chr->canIndicate();
  printJson(doc);
}

static void runGattDiscovery() {
  if (targetMac.length() == 0) {
    emitError("target_not_set", "run SET_TARGET before DISCOVER_GATT");
    return;
  }
  if (!targetAddressTypeKnown) {
    emitError("address_type_unknown", "scan the target once before DISCOVER_GATT");
    return;
  }

  JsonDocument start;
  start["type"] = "gatt_discovery_start";
  start["uptime_ms"] = millis() - bootMs;
  start["target_mac"] = targetMac;
  start["address_type"] = targetAddressType;
  start["safety"] = "discover_only_no_read_no_notify_no_write";
  printJson(start);

  if (discoveryClient == nullptr) {
    discoveryClient = BLEDevice::createClient();
  }

  bool connected = discoveryClient->connect(BLEAddress(targetMac.c_str()), targetAddressType);
  if (!connected) {
    emitError("gatt_connect_failed", "could not connect to target");
    return;
  }

  int serviceCount = 0;
  int characteristicCount = 0;
  std::map<std::string, BLERemoteService*>* services = discoveryClient->getServices();
  if (services != nullptr) {
    for (auto servicePair = services->begin(); servicePair != services->end(); ++servicePair) {
      BLERemoteService* service = servicePair->second;
      if (service == nullptr) continue;
      serviceCount++;

      JsonDocument serviceDoc;
      serviceDoc["type"] = "gatt_service";
      serviceDoc["uptime_ms"] = millis() - bootMs;
      serviceDoc["target_mac"] = targetMac;
      serviceDoc["uuid"] = service->getUUID().toString().c_str();
      serviceDoc["handle"] = service->getHandle();
      printJson(serviceDoc);

      std::map<std::string, BLERemoteCharacteristic*>* chars = service->getCharacteristics();
      if (chars == nullptr) continue;
      for (auto charPair = chars->begin(); charPair != chars->end(); ++charPair) {
        BLERemoteCharacteristic* chr = charPair->second;
        if (chr == nullptr) continue;
        characteristicCount++;
        emitCharacteristic(service, chr);
        delay(5);
      }
    }
  }

  JsonDocument done;
  done["type"] = "gatt_discovery_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["services"] = serviceCount;
  done["characteristics"] = characteristicCount;
  done["safety"] = "no_read_no_notify_no_write";
  printJson(done);

  delay(200);
  discoveryClient->disconnect();
  delay(500);

  JsonDocument disconnected;
  disconnected["type"] = "gatt_disconnected";
  disconnected["uptime_ms"] = millis() - bootMs;
  disconnected["target_mac"] = targetMac;
  printJson(disconnected);
}

static ModbusReadResult runModbusRead(uint16_t startRegister, uint16_t registerCount, const char* label) {
  ModbusReadResult result;
  if (targetMac.length() == 0) {
    emitError("target_not_set", "run SET_TARGET before reading BLE Modbus data");
    return result;
  }
  if (!targetAddressTypeKnown) {
    emitError("address_type_unknown", "scan the target once before reading BLE Modbus data");
    return result;
  }

  String commandHex = buildReadCommand(1, startRegister, registerCount);
  std::string commandBytes;
  if (!hexToBytes(commandHex, commandBytes)) {
    emitError("command_build_failed", "could not build Modbus read command");
    return result;
  }

  JsonDocument start;
  start["type"] = "ble_modbus_read_start";
  start["uptime_ms"] = millis() - bootMs;
  start["target_mac"] = targetMac;
  start["address_type"] = targetAddressType;
  start["label"] = label;
  start["start_register"] = startRegister;
  start["register_count"] = registerCount;
  start["command_hex"] = commandHex;
  start["service_uuid"] = LUMENTREE_VENDOR_SERVICE_UUID;
  start["characteristic_uuid"] = LUMENTREE_VENDOR_CHARACTERISTIC_UUID;
  start["safety"] = "function_03_read_only_no_setting_write";
  printJson(start);

  if (modbusClient == nullptr) {
    modbusClient = BLEDevice::createClient();
  }
  if (modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(250);
  }

  bool connected = modbusClient->connect(BLEAddress(targetMac.c_str()), targetAddressType);
  if (!connected) {
    emitError("gatt_connect_failed", "could not connect to target for BLE Modbus read");
    return result;
  }

  BLERemoteService* service = modbusClient->getService(BLEUUID(LUMENTREE_VENDOR_SERVICE_UUID));
  if (service == nullptr) {
    emitError("gatt_service_missing", "FFE0 service not found");
    modbusClient->disconnect();
    return result;
  }

  BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(LUMENTREE_VENDOR_CHARACTERISTIC_UUID));
  if (chr == nullptr) {
    emitError("gatt_characteristic_missing", "FFE1 characteristic not found");
    modbusClient->disconnect();
    return result;
  }
  if (!chr->canWrite() && !chr->canWriteNoResponse()) {
    emitError("gatt_write_missing", "FFE1 cannot carry Modbus read request");
    modbusClient->disconnect();
    return result;
  }

  modbusNotifyReceived = false;
  modbusLastNotifyMs = 0;
  modbusNotifyCount = 0;
  modbusResponseBytes.clear();
  if (chr->canNotify() || chr->canIndicate()) {
    chr->registerForNotify(modbusNotifyCallback);
    delay(200);
  }

  JsonDocument sent;
  sent["type"] = "ble_modbus_read_request_sent";
  sent["uptime_ms"] = millis() - bootMs;
  sent["target_mac"] = targetMac;
  sent["label"] = label;
  sent["command_hex"] = commandHex;
  sent["write_with_response"] = chr->canWrite();
  printJson(sent);

  chr->writeValue((uint8_t*)commandBytes.data(), commandBytes.length(), chr->canWrite());

  unsigned long deadline = millis() + 5000;
  while (millis() < deadline) {
    feedWatchdog();
    delay(50);
    if (modbusNotifyReceived && millis() - modbusLastNotifyMs > 600) break;
  }

  if (modbusResponseBytes.length() > 0) {
    result.ok = true;
    result.payloadHex = bytesToHex(modbusResponseBytes);
    result.length = modbusResponseBytes.length();
    result.notifyCount = modbusNotifyCount;
    JsonDocument response;
    response["type"] = "ble_modbus_response";
    response["uptime_ms"] = millis() - bootMs;
    response["target_mac"] = targetMac;
    response["label"] = label;
    response["payload_hex"] = result.payloadHex;
    response["length"] = modbusResponseBytes.length();
    response["notify_count"] = modbusNotifyCount;
    response["safety"] = "reassembled_function_03_read_response_only";
    printJson(response);
  } else if (chr->canRead()) {
    std::string value = chr->readValue();
    if (value.length() > 0) {
      result.ok = true;
      result.payloadHex = bytesToHex(value);
      result.length = value.length();
      result.notifyCount = modbusNotifyCount;
      JsonDocument readDoc;
      readDoc["type"] = "ble_modbus_read_value";
      readDoc["uptime_ms"] = millis() - bootMs;
      readDoc["target_mac"] = targetMac;
      readDoc["label"] = label;
      readDoc["payload_hex"] = result.payloadHex;
      readDoc["length"] = value.length();
      readDoc["safety"] = "read_request_response_only_no_setting_write";
      printJson(readDoc);
    }
  }

  JsonDocument done;
  done["type"] = "ble_modbus_read_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["label"] = label;
  done["notify_count"] = modbusNotifyCount;
  done["safety"] = "function_03_read_only_no_setting_write";
  printJson(done);

  delay(200);
  modbusClient->disconnect();
  delay(500);
  return result;
}

static ModbusReadResult runModbusReadInput(uint16_t startRegister, uint16_t registerCount, const char* label) {
  ModbusReadResult result;
  if (targetMac.length() == 0) {
    emitError("target_not_set", "run SET_TARGET before reading BLE Modbus data");
    return result;
  }
  if (!targetAddressTypeKnown) {
    emitError("address_type_unknown", "scan the target once before reading BLE Modbus data");
    return result;
  }

  String commandHex = buildReadInputCommand(1, startRegister, registerCount);
  std::string commandBytes;
  if (!hexToBytes(commandHex, commandBytes)) {
    emitError("command_build_failed", "could not build Modbus input-register read command");
    return result;
  }

  JsonDocument start;
  start["type"] = "ble_modbus_read_start";
  start["uptime_ms"] = millis() - bootMs;
  start["target_mac"] = targetMac;
  start["address_type"] = targetAddressType;
  start["label"] = label;
  start["start_register"] = startRegister;
  start["register_count"] = registerCount;
  start["command_hex"] = commandHex;
  start["service_uuid"] = LUMENTREE_VENDOR_SERVICE_UUID;
  start["characteristic_uuid"] = LUMENTREE_VENDOR_CHARACTERISTIC_UUID;
  start["safety"] = "function_04_read_only_input_registers";
  printJson(start);

  if (modbusClient == nullptr) {
    modbusClient = BLEDevice::createClient();
  }
  if (modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(250);
  }

  bool connected = modbusClient->connect(BLEAddress(targetMac.c_str()), targetAddressType);
  if (!connected) {
    emitError("gatt_connect_failed", "could not connect to target for BLE Modbus read");
    return result;
  }

  BLERemoteService* service = modbusClient->getService(BLEUUID(LUMENTREE_VENDOR_SERVICE_UUID));
  if (service == nullptr) {
    emitError("gatt_service_missing", "FFE0 service not found");
    modbusClient->disconnect();
    return result;
  }

  BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(LUMENTREE_VENDOR_CHARACTERISTIC_UUID));
  if (chr == nullptr) {
    emitError("gatt_characteristic_missing", "FFE1 characteristic not found");
    modbusClient->disconnect();
    return result;
  }
  if (!chr->canWrite() && !chr->canWriteNoResponse()) {
    emitError("gatt_write_missing", "FFE1 cannot carry Modbus read request");
    modbusClient->disconnect();
    return result;
  }

  modbusNotifyReceived = false;
  modbusLastNotifyMs = 0;
  modbusNotifyCount = 0;
  modbusResponseBytes.clear();
  if (chr->canNotify() || chr->canIndicate()) {
    chr->registerForNotify(modbusNotifyCallback);
    delay(200);
  }

  JsonDocument sent;
  sent["type"] = "ble_modbus_read_request_sent";
  sent["uptime_ms"] = millis() - bootMs;
  sent["target_mac"] = targetMac;
  sent["label"] = label;
  sent["command_hex"] = commandHex;
  sent["write_with_response"] = chr->canWrite();
  printJson(sent);

  chr->writeValue((uint8_t*)commandBytes.data(), commandBytes.length(), chr->canWrite());

  unsigned long deadline = millis() + 5000;
  while (millis() < deadline) {
    feedWatchdog();
    delay(50);
    if (modbusNotifyReceived && millis() - modbusLastNotifyMs > 600) break;
  }

  if (modbusResponseBytes.length() > 0) {
    result.ok = true;
    result.payloadHex = bytesToHex(modbusResponseBytes);
    result.length = modbusResponseBytes.length();
    result.notifyCount = modbusNotifyCount;
    JsonDocument response;
    response["type"] = "ble_modbus_response";
    response["uptime_ms"] = millis() - bootMs;
    response["target_mac"] = targetMac;
    response["label"] = label;
    response["payload_hex"] = result.payloadHex;
    response["length"] = modbusResponseBytes.length();
    response["notify_count"] = modbusNotifyCount;
    response["safety"] = "reassembled_function_04_read_response_only";
    printJson(response);
  } else if (chr->canRead()) {
    std::string value = chr->readValue();
    if (value.length() > 0) {
      result.ok = true;
      result.payloadHex = bytesToHex(value);
      result.length = value.length();
      result.notifyCount = modbusNotifyCount;
      JsonDocument readDoc;
      readDoc["type"] = "ble_modbus_read_value";
      readDoc["uptime_ms"] = millis() - bootMs;
      readDoc["target_mac"] = targetMac;
      readDoc["label"] = label;
      readDoc["payload_hex"] = result.payloadHex;
      readDoc["length"] = value.length();
      readDoc["safety"] = "read_request_response_only_no_setting_write";
      printJson(readDoc);
    }
  }

  JsonDocument done;
  done["type"] = "ble_modbus_read_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["label"] = label;
  done["notify_count"] = modbusNotifyCount;
  done["safety"] = "function_04_read_only_input_registers";
  printJson(done);

  delay(200);
  modbusClient->disconnect();
  delay(500);
  return result;
}

static bool readRegisterFromResult(const ModbusReadResult& result, uint16_t startRegister, uint16_t registerAddress, uint16_t& value) {
  if (!result.ok) return false;
  std::string responseBytes;
  if (!hexToBytes(result.payloadHex, responseBytes)) return false;
  if (responseBytes.length() < 5) return false;
  const uint8_t* bytes = (const uint8_t*)responseBytes.data();
  if (bytes[0] != 1 || bytes[1] != 0x03) return false;
  uint8_t byteCount = bytes[2];
  if (responseBytes.length() < (size_t)byteCount + 5) return false;
  if (registerAddress < startRegister) return false;
  uint16_t offsetRegister = registerAddress - startRegister;
  size_t offset = 3 + ((size_t)offsetRegister * 2);
  if (offset + 1 >= 3 + byteCount) return false;
  value = ((uint16_t)bytes[offset] << 8) | bytes[offset + 1];
  return true;
}

static bool runModbusWriteSingleRegister(uint16_t registerAddress, uint16_t value, const char* label) {
  if (targetMac.length() == 0) {
    emitError("target_not_set", "run SET_TARGET before writing BLE Modbus data");
    return false;
  }
  if (!targetAddressTypeKnown) {
    emitError("address_type_unknown", "scan the target once before writing BLE Modbus data");
    return false;
  }

  String commandHex = buildWriteSingleRegisterCommand(1, registerAddress, value);
  std::string commandBytes;
  if (!hexToBytes(commandHex, commandBytes)) {
    emitError("command_build_failed", "could not build Modbus write command");
    return false;
  }

  JsonDocument start;
  start["type"] = "ble_modbus_write_single_register_start";
  start["uptime_ms"] = millis() - bootMs;
  start["target_mac"] = targetMac;
  start["address_type"] = targetAddressType;
  start["label"] = label;
  start["register"] = registerAddress;
  start["value"] = value;
  start["command_hex"] = commandHex;
  start["service_uuid"] = LUMENTREE_VENDOR_SERVICE_UUID;
  start["characteristic_uuid"] = LUMENTREE_VENDOR_CHARACTERISTIC_UUID;
  start["safety"] = "whitelisted_function_06_single_register_write";
  printJson(start);

  if (modbusClient == nullptr) {
    modbusClient = BLEDevice::createClient();
  }
  if (modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(250);
  }

  bool connected = modbusClient->connect(BLEAddress(targetMac.c_str()), targetAddressType);
  if (!connected) {
    emitError("gatt_connect_failed", "could not connect to target for BLE Modbus write");
    return false;
  }

  BLERemoteService* service = modbusClient->getService(BLEUUID(LUMENTREE_VENDOR_SERVICE_UUID));
  if (service == nullptr) {
    emitError("gatt_service_missing", "FFE0 service not found");
    modbusClient->disconnect();
    return false;
  }

  BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(LUMENTREE_VENDOR_CHARACTERISTIC_UUID));
  if (chr == nullptr) {
    emitError("gatt_characteristic_missing", "FFE1 characteristic not found");
    modbusClient->disconnect();
    return false;
  }
  if (!chr->canWrite() && !chr->canWriteNoResponse()) {
    emitError("gatt_write_missing", "FFE1 cannot carry Modbus write request");
    modbusClient->disconnect();
    return false;
  }

  modbusNotifyReceived = false;
  modbusLastNotifyMs = 0;
  modbusNotifyCount = 0;
  modbusResponseBytes.clear();
  if (chr->canNotify() || chr->canIndicate()) {
    chr->registerForNotify(modbusNotifyCallback);
    delay(200);
  }

  JsonDocument sent;
  sent["type"] = "ble_modbus_write_single_register_sent";
  sent["uptime_ms"] = millis() - bootMs;
  sent["target_mac"] = targetMac;
  sent["label"] = label;
  sent["register"] = registerAddress;
  sent["value"] = value;
  sent["command_hex"] = commandHex;
  sent["write_with_response"] = chr->canWrite();
  printJson(sent);

  chr->writeValue((uint8_t*)commandBytes.data(), commandBytes.length(), chr->canWrite());

  unsigned long deadline = millis() + 5000;
  while (millis() < deadline) {
    feedWatchdog();
    delay(50);
    if (modbusNotifyReceived && millis() - modbusLastNotifyMs > 600) break;
  }

  bool responseMatches = false;
  String responseHex = "";
  if (modbusResponseBytes.length() > 0) {
    responseHex = bytesToHex(modbusResponseBytes);
    responseMatches = responseHex == commandHex;
  }

  JsonDocument done;
  done["type"] = "ble_modbus_write_single_register_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["label"] = label;
  done["register"] = registerAddress;
  done["value"] = value;
  done["response_hex"] = responseHex;
  done["response_matches_request"] = responseMatches;
  done["notify_count"] = modbusNotifyCount;
  done["safety"] = "function_06_whitelisted_single_register_write_done";
  printJson(done);

  delay(200);
  modbusClient->disconnect();
  delay(500);
  return responseMatches;
}

static void runTargetSoc144To6Test() {
  const uint16_t targetSocRegister = 144;
  const uint16_t expectedBefore = 16;
  const uint16_t requestedValue = 6;

  JsonDocument start;
  start["type"] = "target_soc_write_test_start";
  start["uptime_ms"] = millis() - bootMs;
  start["register"] = targetSocRegister;
  start["expected_before"] = expectedBefore;
  start["requested_value"] = requestedValue;
  start["safety"] = "single_whitelisted_test_requires_pre_read_and_post_read";
  printJson(start);

  ModbusReadResult beforeRead = runModbusRead(95, 95, "pre_read_registers_95_189_before_target_soc_write");
  uint16_t beforeValue = 0;
  if (!readRegisterFromResult(beforeRead, 95, targetSocRegister, beforeValue)) {
    emitError("target_soc_pre_read_failed", "could not read register 144 before write");
    return;
  }

  JsonDocument before;
  before["type"] = "target_soc_write_test_pre_read";
  before["uptime_ms"] = millis() - bootMs;
  before["register"] = targetSocRegister;
  before["value"] = beforeValue;
  before["expected_before"] = expectedBefore;
  printJson(before);

  if (beforeValue != expectedBefore) {
    emitError("target_soc_precondition_failed", "register 144 is not the expected value 16; write skipped");
    return;
  }

  bool writeAck = runModbusWriteSingleRegister(targetSocRegister, requestedValue, "write_register_144_target_soc_to_6");
  if (!writeAck) {
    emitError("target_soc_write_ack_failed", "write response did not echo request; verification will still run");
  }

  delay(1000);

  ModbusReadResult afterRead = runModbusRead(95, 95, "post_read_registers_95_189_after_target_soc_write");
  uint16_t afterValue = 0;
  if (!readRegisterFromResult(afterRead, 95, targetSocRegister, afterValue)) {
    emitError("target_soc_post_read_failed", "could not read register 144 after write");
    return;
  }

  JsonDocument result;
  result["type"] = "target_soc_write_test_result";
  result["uptime_ms"] = millis() - bootMs;
  result["register"] = targetSocRegister;
  result["before_value"] = beforeValue;
  result["requested_value"] = requestedValue;
  result["after_value"] = afterValue;
  result["write_ack"] = writeAck;
  result["verified"] = afterValue == requestedValue;
  result["safety"] = "pre_read_write_post_read_single_register_144";
  printJson(result);
}

static void runManualReadRange(const String& line) {
  int firstSpace = line.indexOf(' ');
  int secondSpace = firstSpace >= 0 ? line.indexOf(' ', firstSpace + 1) : -1;
  if (firstSpace < 0 || secondSpace < 0) {
    emitError("invalid_read_range", "usage: READ_RANGE start_register register_count");
    return;
  }

  int startRegister = line.substring(firstSpace + 1, secondSpace).toInt();
  int registerCount = line.substring(secondSpace + 1).toInt();
  if (startRegister < 0 || startRegister > 65535) {
    emitError("invalid_read_range", "start_register must be 0..65535");
    return;
  }
  if (registerCount < 1 || registerCount > 95) {
    emitError("invalid_read_range", "register_count must be 1..95");
    return;
  }
  if (startRegister + registerCount > 65536) {
    emitError("invalid_read_range", "register range exceeds uint16 address space");
    return;
  }

  char label[64];
  snprintf(label, sizeof(label), "manual_registers_%d_%d", startRegister, startRegister + registerCount - 1);
  runModbusRead((uint16_t)startRegister, (uint16_t)registerCount, label);
}

static void printHelp() {
  Serial0.println("# Commands:");
  Serial0.println("# HELP");
  Serial0.println("# STATUS");
  Serial0.println("# CONFIG");
  Serial0.println("# SET_TARGET aa:bb:cc:dd:ee:ff");
  Serial0.println("# SET_TARGET_MAC aa:bb:cc:dd:ee:ff");
  Serial0.println("# CLEAR_TARGET");
  Serial0.println("# CLEAR_TARGET_MAC");
  Serial0.println("# SET_ACTIVE 0|1");
  Serial0.println("# SET_CANDIDATE_ONLY 0|1");
  Serial0.println("# SET_SCAN seconds interval window [log_limit]");
  Serial0.println("# SET_WIFI ssid password");
  Serial0.println("# SET_API_URL https://lumentree.jonah.io.vn");
  Serial0.println("# SET_API_TOKEN token");
  Serial0.println("# SET_DEVICE_ID P240819130");
  Serial0.println("# SET_GATEWAY_ID esp32-lumentree");
  Serial0.println("# SET_UPLOAD_INTERVAL seconds");
  Serial0.println("# SET_TLS_INSECURE 0|1");
  Serial0.println("# SET_PRODUCTION 0|1");
  Serial0.println("# START_AP");
  Serial0.println("# WRITE_STATUS");
  Serial0.println("# GENERATE_WRITE_CODE");
  Serial0.println("# SCAN_WIFI");
  Serial0.println("# SCAN_BLE");
  Serial0.println("# BLE_CANDIDATES");
  Serial0.println("# SCAN_ONCE");
  Serial0.println("# DISCOVER_GATT");
  Serial0.println("# READ_MAIN_ONCE");
  Serial0.println("# READ_STATS_ONCE");
  Serial0.println("# READ_RANGE start_register register_count");
  Serial0.println("# WRITE_TARGET_SOC_144_TO_6 CONFIRM");
  Serial0.println("# UPLOAD_ONCE");
  Serial0.println("# READ_CELLS_ONCE");
  Serial0.println("# Lumentree candidate hints: BLE name contains Device ID, uuid=A018739B-734D-8211-CB80-C9ACD39D13B4, or GATT FFE0/FFE1");
  Serial0.println("# Safety: READ_MAIN_ONCE/READ_RANGE/READ_CELLS_ONCE use Modbus function 03; READ_STATS_ONCE uses function 04.");
  Serial0.println("# WRITE_TARGET_SOC_144_TO_6 is a one-off whitelisted function 06 test with pre/post read verification.");
}

static void runUploadOnce() {
  if (targetMac.length() == 0) {
    if (!runBleDiscovery(true)) {
      emitError("target_not_paired", "waiting for ESP32 gateway pairing");
      return;
    }
  }
  ModbusReadResult result = runModbusRead(0, 95, "main_registers_0_94");
  if (!result.ok) {
    emitError("ble_read_empty", "no Modbus response payload to upload");
    return;
  }
  postTelemetry(
    result.payloadHex,
    result.notifyCount,
    result.length,
    0,
    95,
    "main_registers_0_94",
    "function_03_read_only_no_setting_write"
  );

  unsigned long now = millis();
  if (lastSettingsUploadMs == 0 || now - lastSettingsUploadMs >= SETTINGS_UPLOAD_INTERVAL_MS) {
    ModbusReadResult settings = runModbusRead(95, 95, "settings_registers_95_189");
    if (settings.ok) {
      postTelemetry(
        settings.payloadHex,
        settings.notifyCount,
        settings.length,
        95,
        95,
        "settings_registers_95_189",
        "function_03_read_only_no_setting_write"
      );
      lastSettingsUploadMs = millis();
    } else {
      emitError("settings_read_empty", "no settings Modbus response payload to upload");
    }
  }

  now = millis();
  if (lastStatsUploadMs == 0 || now - lastStatsUploadMs >= STATS_UPLOAD_INTERVAL_MS) {
    ModbusReadResult stats = runModbusReadInput(0, 8, "today_statistics_0_7");
    if (stats.ok) {
      postTelemetry(
        stats.payloadHex,
        stats.notifyCount,
        stats.length,
        0,
        8,
        "today_statistics_0_7",
        "function_04_read_only_input_registers"
      );
      lastStatsUploadMs = millis();
    } else {
      emitError("stats_read_empty", "no statistics Modbus response payload to upload");
    }
  }
}

static void runWifiScan() {
  wifi_mode_t previousMode = WiFi.getMode();
  if (previousMode == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    delay(200);
  }
  int count = WiFi.scanNetworks(false, true);
  JsonDocument doc;
  doc["type"] = "wifi_scan";
  doc["uptime_ms"] = millis() - bootMs;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int i = 0; i < count; i++) {
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  printJson(doc);
  if (previousMode == WIFI_MODE_NULL && !productionEnabled && !provisioningPortalActive) {
    WiFi.mode(WIFI_OFF);
  }
}

static void maybeRunProductionUpload() {
  if (!productionEnabled) return;
  unsigned long now = millis();
  if (nextUploadMs == 0) {
    nextUploadMs = now + 2000;
    return;
  }
  if ((long)(now - nextUploadMs) < 0) return;
  if (targetMac.length() == 0 && lastAutoDiscoveryMs != 0 && now - lastAutoDiscoveryMs < AUTO_DISCOVERY_RETRY_MS) {
    nextUploadMs = millis() + 5000;
    return;
  }
  if (targetMac.length() == 0) {
    lastAutoDiscoveryMs = now;
  }
  runUploadOnce();
  nextUploadMs = millis() + ((unsigned long)uploadIntervalSeconds * 1000UL);
}

static void maybeEmitHeartbeat() {
  unsigned long now = millis();
  if (lastHeartbeatMs != 0 && now - lastHeartbeatMs < HEARTBEAT_INTERVAL_MS) return;
  lastHeartbeatMs = now;

  JsonDocument doc;
  doc["type"] = "heartbeat";
  doc["fw"] = FW_NAME;
  doc["version"] = FW_VERSION;
  doc["uptime_ms"] = now - bootMs;
  doc["production_enabled"] = productionEnabled;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_ssid"] = wifiSsid;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["gateway_id"] = gatewayId;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  doc["candidate_count"] = bleCandidates.size();
  doc["upload_interval_seconds"] = uploadIntervalSeconds;
  doc["provisioning_portal_active"] = provisioningPortalActive;
  printJson(doc);
}

static void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line == "HELP") {
    printHelp();
  } else if (line == "STATUS") {
    emitStatus("status");
  } else if (line == "CONFIG") {
    emitConfig("config");
  } else if (line == "CLEAR_TARGET" || line == "CLEAR_TARGET_MAC") {
    targetMac = "";
    targetAddressTypeKnown = false;
    pairingStatus = "unconfigured";
    prefs.remove("target_mac");
    emitAck(line == "CLEAR_TARGET" ? "CLEAR_TARGET" : "CLEAR_TARGET_MAC");
  } else if (line.startsWith("SET_TARGET ") || line.startsWith("SET_TARGET_MAC ")) {
    int offset = line.startsWith("SET_TARGET_MAC ") ? 15 : 11;
    targetMac = normalizeMac(line.substring(offset));
    targetAddressTypeKnown = targetMac.length() > 0;
    saveStringConfig("target_mac", targetMac);
    pairingStatus = targetMac.length() > 0 ? "paired" : "unconfigured";
    emitAck(line.startsWith("SET_TARGET_MAC ") ? "SET_TARGET_MAC" : "SET_TARGET");
  } else if (line.startsWith("SET_ACTIVE ")) {
    int value = line.substring(11).toInt();
    activeScan = value != 0;
    emitAck("SET_ACTIVE");
  } else if (line.startsWith("SET_CANDIDATE_ONLY ")) {
    int value = line.substring(19).toInt();
    candidateOnly = value != 0;
    emitAck("SET_CANDIDATE_ONLY");
  } else if (line.startsWith("SET_SCAN ")) {
    int first = line.indexOf(' ', 9);
    int second = first > 0 ? line.indexOf(' ', first + 1) : -1;
    int third = second > 0 ? line.indexOf(' ', second + 1) : -1;
    if (first < 0 || second < 0) {
      emitError("bad_command", "usage: SET_SCAN seconds interval window [log_limit]");
      return;
    }
    scanSeconds = (uint8_t)constrain(line.substring(9, first).toInt(), 1, 10);
    scanInterval = (uint16_t)constrain(line.substring(first + 1, second).toInt(), 1, 5000);
    scanWindow = (uint16_t)constrain(line.substring(second + 1, third > 0 ? third : line.length()).toInt(), 1, scanInterval);
    if (third > 0) {
      logLimit = (uint16_t)constrain(line.substring(third + 1).toInt(), 1, 500);
    }
    emitAck("SET_SCAN");
  } else if (line.startsWith("SET_WIFI ")) {
    String rest = line.substring(9);
    int split = rest.indexOf(' ');
    if (split <= 0) {
      emitError("bad_command", "usage: SET_WIFI ssid password");
      return;
    }
    wifiSsid = rest.substring(0, split);
    wifiPassword = rest.substring(split + 1);
    saveStringConfig("wifi_ssid", wifiSsid);
    saveStringConfig("wifi_pass", wifiPassword);
    WiFi.disconnect(true);
    emitAck("SET_WIFI");
  } else if (line.startsWith("SET_API_URL ")) {
    apiUrl = line.substring(12);
    apiUrl.trim();
    saveStringConfig("api_url", apiUrl);
    emitAck("SET_API_URL");
  } else if (line.startsWith("SET_API_TOKEN ")) {
    apiToken = line.substring(14);
    apiToken.trim();
    saveStringConfig("api_token", apiToken);
    emitAck("SET_API_TOKEN");
  } else if (line.startsWith("SET_DEVICE_ID ")) {
    deviceId = line.substring(14);
    deviceId.trim();
    saveStringConfig("device_id", deviceId);
    emitAck("SET_DEVICE_ID");
  } else if (line.startsWith("SET_GATEWAY_ID ")) {
    gatewayId = line.substring(15);
    gatewayId.trim();
    saveStringConfig("gateway_id", gatewayId);
    emitAck("SET_GATEWAY_ID");
  } else if (line.startsWith("SET_UPLOAD_INTERVAL ")) {
    uploadIntervalSeconds = (uint16_t)constrain(line.substring(20).toInt(), MIN_UPLOAD_INTERVAL_SECONDS, 3600);
    prefs.putUShort("upload_s", uploadIntervalSeconds);
    nextUploadMs = 0;
    emitAck("SET_UPLOAD_INTERVAL");
  } else if (line.startsWith("SET_TLS_INSECURE ")) {
    int value = line.substring(17).toInt();
    tlsInsecure = value != 0;
    prefs.putBool("tls_insec", tlsInsecure);
    emitAck("SET_TLS_INSECURE");
  } else if (line.startsWith("SET_PRODUCTION ")) {
    int value = line.substring(15).toInt();
    productionEnabled = value != 0;
    prefs.putBool("prod", productionEnabled);
    nextUploadMs = 0;
    emitAck("SET_PRODUCTION");
  } else if (line == "START_AP") {
    if (!provisioningPortalActive) {
      startProvisioningPortal(WiFi.status() == WL_CONNECTED);
    }
    emitAck("START_AP");
  } else if (line == "WRITE_STATUS") {
    JsonDocument doc;
    doc["type"] = "write_status";
    doc["uptime_ms"] = millis() - bootMs;
    doc["device_id"] = deviceId;
    doc["gateway_id"] = gatewayId;
    doc["write_pairing_code_active"] = writePairingCodeExpiresInSeconds() > 0;
    doc["write_pairing_code_expires_in_seconds"] = writePairingCodeExpiresInSeconds();
    doc["safety"] = "authorization_only_no_ble_write";
    printJson(doc);
  } else if (line == "GENERATE_WRITE_CODE") {
    String code = generateWritePairingCode();
    String response;
    if (!postWritePairingCode(code, response)) {
      emitError("write_code_failed", "failed to register write pairing code with API");
      return;
    }
    writePairingCode = code;
    writePairingCodeExpiresMs = millis() + WRITE_PAIRING_CODE_TTL_MS;
    JsonDocument doc;
    doc["type"] = "write_pairing_code";
    doc["uptime_ms"] = millis() - bootMs;
    doc["device_id"] = deviceId;
    doc["gateway_id"] = gatewayId;
    doc["code"] = writePairingCode;
    doc["expires_in_seconds"] = writePairingCodeExpiresInSeconds();
    doc["safety"] = "authorization_only_no_ble_write";
    printJson(doc);
  } else if (line == "SCAN_WIFI") {
    runWifiScan();
  } else if (line == "SCAN_BLE") {
    runBleDiscovery(false);
  } else if (line == "BLE_CANDIDATES") {
    emitBleCandidates("ble_candidates");
  } else if (line == "SCAN_ONCE") {
    runScanOnce();
  } else if (line == "DISCOVER_GATT") {
    runGattDiscovery();
  } else if (line == "READ_MAIN_ONCE") {
    runModbusRead(0, 95, "main_registers_0_94");
  } else if (line == "READ_STATS_ONCE") {
    runModbusReadInput(0, 8, "today_statistics_0_7");
  } else if (line.startsWith("READ_RANGE ")) {
    runManualReadRange(line);
  } else if (line == "WRITE_TARGET_SOC_144_TO_6 CONFIRM") {
    runTargetSoc144To6Test();
  } else if (line == "UPLOAD_ONCE") {
    runUploadOnce();
  } else if (line == "READ_CELLS_ONCE") {
    runModbusRead(250, 50, "battery_cells_250_299");
  } else {
    emitError("unknown_command", "send HELP for command list");
  }
}

void setup() {
  bootMs = millis();
  Serial0.begin(SERIAL_BAUD);
  delay(500);
  esp_task_wdt_init(WATCHDOG_TIMEOUT_SECONDS, true);
  esp_task_wdt_add(nullptr);

  loadConfig();
  if (productionEnabled) {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  } else {
    WiFi.mode(WIFI_OFF);
  }
  delay(100);

  BLEDevice::init("LumentreeBLEBridge");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  bleScan->setActiveScan(activeScan);
  bleScan->setInterval(scanInterval);
  bleScan->setWindow(scanWindow);

  if (wifiSsid.length() == 0) {
    startProvisioningPortal(false);
  }

  emitStatus("boot");
  printHelp();
}

void loop() {
  if (Serial0.available()) {
    String line = Serial0.readStringUntil('\n');
    handleCommand(line);
  }
  handleProvisioningPortal();
  maybeRunProductionUpload();
  pollPendingCommand();
  maybeEmitHeartbeat();
  feedWatchdog();
  delay(10);
}
