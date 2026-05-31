#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLEDevice.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteService.h>
#include <BLEScan.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
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
#define LUMENTREE_DEFAULT_DEVICE_ID ""
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
#ifndef LUMENTREE_DISABLE_BACKGROUND_TELEMETRY
#define LUMENTREE_DISABLE_BACKGROUND_TELEMETRY 0
#endif
#ifndef LUMENTREE_DISABLE_TELEMETRY_UPLOADS
#define LUMENTREE_DISABLE_TELEMETRY_UPLOADS 0
#endif
#ifndef LUMENTREE_USE_TELEMETRY_TASK
#define LUMENTREE_USE_TELEMETRY_TASK 0
#endif
#ifndef LUMENTREE_MAIN_TELEMETRY_CHUNK_REGISTERS
#define LUMENTREE_MAIN_TELEMETRY_CHUNK_REGISTERS 95
#endif
#ifndef LUMENTREE_USE_FAST_MAIN_CACHE
#define LUMENTREE_USE_FAST_MAIN_CACHE 0
#endif
#ifndef LUMENTREE_MAIN_FULL_REFRESH_INTERVAL_MS
#define LUMENTREE_MAIN_FULL_REFRESH_INTERVAL_MS 60000UL
#endif
#ifndef LUMENTREE_SETTINGS_POLL_INTERVAL_MS
#define LUMENTREE_SETTINGS_POLL_INTERVAL_MS 60000UL
#endif
#ifndef LUMENTREE_STATS_POLL_INTERVAL_MS
#define LUMENTREE_STATS_POLL_INTERVAL_MS (5UL * 60UL * 1000UL)
#endif
#ifndef LUMENTREE_DISABLE_COMMAND_POLLING
#define LUMENTREE_DISABLE_COMMAND_POLLING 0
#endif
#ifndef LUMENTREE_USE_COMMAND_POLL_TASK
#define LUMENTREE_USE_COMMAND_POLL_TASK 0
#endif

#ifndef LUMENTREE_FIRMWARE_NAME
#define LUMENTREE_FIRMWARE_NAME "lumentree-ble-bridge"
#endif
#ifndef LUMENTREE_FIRMWARE_VERSION
#define LUMENTREE_FIRMWARE_VERSION "0.0.0-dev"
#endif
#ifndef LUMENTREE_COMMAND_POLL_INTERVAL_MS
#define LUMENTREE_COMMAND_POLL_INTERVAL_MS 10000UL
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
static const uint16_t MIN_UPLOAD_INTERVAL_SECONDS = 1;
static const uint16_t DEFAULT_UPLOAD_INTERVAL_SECONDS = LUMENTREE_DEFAULT_UPLOAD_INTERVAL_SECONDS;
static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
static const uint16_t PROVISIONING_DNS_PORT = 53;
static const uint32_t WATCHDOG_TIMEOUT_SECONDS = 60;
static const unsigned long HEARTBEAT_INTERVAL_MS = 300000;
static const unsigned long HTTP_STARTUP_QUIET_MS = 30000;
static const unsigned long AUTO_DISCOVERY_RETRY_MS = 60000;
static const unsigned long COMMAND_POLL_INTERVAL_MS = LUMENTREE_COMMAND_POLL_INTERVAL_MS;
static const unsigned long FAST_TELEMETRY_INTERVAL_MS = 5000;
static const unsigned long TELEMETRY_UPLOAD_RETRY_BACKOFF_MS = 2000;
static const unsigned long HTTPS_MIN_GAP_MS = 2500;
static const unsigned long TELEMETRY_BLE_GAP_MS = 1500;
static const unsigned long WRITE_LANE_RESUME_DELAY_MS = 8000;
static const uint32_t HTTP_BUSY_SKIP_TIMEOUT_MS = 25;
static const unsigned long WRITE_PAIRING_CODE_TTL_MS = 600000;
static const unsigned long MAIN_FULL_REFRESH_INTERVAL_MS = 300000UL;
static const unsigned long SETTINGS_UPLOAD_INTERVAL_MS = 900000UL;
static const unsigned long STATS_UPLOAD_INTERVAL_MS = 1800000UL;
static const uint16_t SCHEDULE_STATE_START_REGISTER = 130;
static const uint16_t SCHEDULE_STATE_REGISTER_COUNT = 47;
static const uint16_t MAIN_TELEMETRY_TOTAL_REGISTERS = 95;
static const uint16_t MAIN_TELEMETRY_RESPONSE_DATA_BYTES = MAIN_TELEMETRY_TOTAL_REGISTERS * 2;
static const uint8_t RUNTIME_PROBE_COUNT = 7;
static const uint8_t RUNTIME_TRACE_SIZE = 16;

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
static bool backgroundTelemetryDisabled = LUMENTREE_DISABLE_BACKGROUND_TELEMETRY != 0;
static bool telemetryUploadsDisabled = LUMENTREE_DISABLE_TELEMETRY_UPLOADS != 0;
static bool telemetryTaskEnabled = LUMENTREE_USE_TELEMETRY_TASK != 0;
static bool commandPollingDisabled = LUMENTREE_DISABLE_COMMAND_POLLING != 0;
static bool commandPollTaskEnabled = LUMENTREE_USE_COMMAND_POLL_TASK != 0;
static bool bleConnectionEnabled = true;
static bool provisioningPortalActive = false;
static bool portalServerStarted = false;
static bool mdnsStarted = false;
static String provisioningApSsid;
static String localHostname;
static esp_ble_addr_type_t targetAddressType = BLE_ADDR_TYPE_PUBLIC;
static bool targetAddressTypeKnown = false;
static BLEClient* discoveryClient = nullptr;
static BLEClient* modbusClient = nullptr;
static BLERemoteCharacteristic* modbusCharacteristic = nullptr;
static unsigned long bootMs = 0;
static unsigned long nextUploadMs = 0;
static unsigned long nextSettingsPollMs = 0;
static unsigned long nextStatsPollMs = 0;
static unsigned long lastHeartbeatMs = 0;
static unsigned long lastAutoDiscoveryMs = 0;
static unsigned long lastGatewayStatusPostMs = 0;
static unsigned long lastCommandPollMs = 0;
static unsigned long lastSettingsUploadMs = 0;
static unsigned long lastStatsUploadMs = 0;
static unsigned long nextDirtyTelemetryFlushRetryMs = 0;
static unsigned long nextMainFullRefreshMs = 0;
static unsigned long lastHttpsAttemptMs = 0;
static unsigned long lastBleTelemetryActionMs = 0;
static unsigned long telemetryResumeAfterWriteMs = 0;
static bool forceMainFullRefresh = true;
static uint16_t nextMainTelemetryStartRegister = 0;
static const size_t RUNTIME_LOG_BUFFER_SIZE = 200;
static String runtimeLogBuffer[RUNTIME_LOG_BUFFER_SIZE];
static size_t runtimeLogNextIndex = 0;
static size_t runtimeLogCount = 0;
static bool otaInProgress = false;
static bool otaLastOk = false;
static bool otaAwaitingValidation = false;
static bool otaValidationDone = false;
static String otaLastError;
static String otaLastUrl;
static String otaLastVersion;
static size_t otaLastWrittenBytes = 0;
static unsigned long otaStartedMs = 0;
static unsigned long otaFinishedMs = 0;
static unsigned long otaBootValidateAfterMs = 0;
static volatile bool modbusNotifyReceived = false;
static volatile unsigned long modbusLastNotifyMs = 0;
static uint16_t modbusNotifyCount = 0;
static std::string modbusResponseBytes;
static bool modbusNotifyRegistered = false;
static unsigned long modbusReconnectBackoffUntilMs = 0;
static uint8_t modbusReconnectFailureCount = 0;
static String writePairingCode;
static unsigned long writePairingCodeExpiresMs = 0;
static String readPairingToken;
static unsigned long readPairingTokenExpiresMs = 0;
static uint64_t pendingCommandResultId = 0;
static String pendingCommandResultStatus;
static String pendingCommandResultError;
static String pendingCommandResultPayload;
static SemaphoreHandle_t modbusOperationMutex = nullptr;
static SemaphoreHandle_t httpOperationMutex = nullptr;
static TaskHandle_t telemetryTaskHandle = nullptr;
static TaskHandle_t commandPollTaskHandle = nullptr;
static portMUX_TYPE runtimeProbeMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool writeLaneActive = false;
enum RuntimeProbeId : uint8_t {
  PROBE_LOOP = 0,
  PROBE_PORTAL = 1,
  PROBE_COMMAND_POLL = 2,
  PROBE_WIFI_CONNECT = 3,
  PROBE_TELEMETRY_UPLOAD = 4,
  PROBE_BLE_REQUEST = 5,
  PROBE_TELEMETRY_SCHEDULER = 6,
};

struct ModbusReadResult {
  bool ok = false;
  String payloadHex;
  size_t length = 0;
  uint16_t notifyCount = 0;
};

struct TelemetrySnapshot {
  bool valid = false;
  bool dirty = false;
  String payloadHex;
  size_t length = 0;
  uint16_t notifyCount = 0;
  uint16_t startRegister = 0;
  uint16_t registerCount = 0;
  String label;
  String safety;
  unsigned long observedMs = 0;
};

struct MainTelemetryCache {
  bool valid = false;
  bool registersValid[MAIN_TELEMETRY_TOTAL_REGISTERS] = {};
  uint8_t data[MAIN_TELEMETRY_RESPONSE_DATA_BYTES] = {};
  uint16_t validCount = 0;
  unsigned long updatedMs = 0;
  unsigned long fullRefreshMs = 0;
};

struct MainTelemetryReadRange {
  uint16_t startRegister = 0;
  uint16_t registerCount = 0;
  const char* label = "";

  MainTelemetryReadRange(uint16_t start, uint16_t count, const char* rangeLabel)
    : startRegister(start), registerCount(count), label(rangeLabel) {}
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

struct ScheduleSlotState {
  bool enabled = false;
  uint16_t start = 0;
  uint16_t end = 0;
};

struct ScheduleState {
  ScheduleSlotState mainsCharge[2];
  ScheduleSlotState discharge[4];
};

struct RuntimeProbe {
  const char* name = "";
  uint32_t slowThresholdMs = 0;
  uint32_t calls = 0;
  uint32_t slowCalls = 0;
  uint32_t failures = 0;
  int32_t lastHttpStatus = 0;
  uint32_t lastDurationMs = 0;
  uint32_t maxDurationMs = 0;
  uint32_t lastStartMs = 0;
  uint32_t lastEndMs = 0;
  bool inFlight = false;
};

struct RuntimeTraceEntry {
  const char* name = "";
  uint32_t startedMs = 0;
  uint32_t durationMs = 0;
  bool ok = false;
};

static RuntimeProbe makeRuntimeProbe(const char* name, uint32_t slowThresholdMs) {
  RuntimeProbe probe;
  probe.name = name;
  probe.slowThresholdMs = slowThresholdMs;
  return probe;
}

static std::vector<BleCandidate> bleCandidates;
static String pairingStatus = "unconfigured";
static TelemetrySnapshot latestMainTelemetry;
static TelemetrySnapshot latestSettingsTelemetry;
static TelemetrySnapshot latestStatsTelemetry;
static MainTelemetryCache mainTelemetryCache;
static uint8_t nextFastMainRangeIndex = 0;
static RuntimeProbe runtimeProbes[RUNTIME_PROBE_COUNT] = {
  makeRuntimeProbe("loop", 100),
  makeRuntimeProbe("portal", 50),
  makeRuntimeProbe("command_poll", 250),
  makeRuntimeProbe("wifi_connect", 500),
  makeRuntimeProbe("telemetry_upload", 500),
  makeRuntimeProbe("ble_request", 500),
  makeRuntimeProbe("telemetry_scheduler", 250),
};
static RuntimeTraceEntry runtimeTrace[RUNTIME_TRACE_SIZE];
static uint8_t runtimeTraceNextIndex = 0;
static bool runtimeTraceWrapped = false;
static const MainTelemetryReadRange FAST_MAIN_TELEMETRY_RANGES[] = {
  {11, 14, "fast_main_registers_11_24"},
  {50, 25, "fast_main_registers_50_74"},
};

static void addCandidatesJson(JsonArray array);
static void addRuntimeProbeJson(JsonArray array);
static void addRuntimeTraceJson(JsonArray array);
static bool runBleDiscovery(bool allowAutoBind);
static bool ensureWifiConnected();
static bool postGatewayCandidates();
static bool postGatewayStatus(const char* reason);
static bool postTelemetry(const String& payloadHex, uint16_t notifyCount, size_t payloadLength, uint16_t startRegister, uint16_t registerCount, const char* label, const char* safety);
static bool postReadPairingToken(const String& token, String& response);
static bool postWritePairingCode(const String& code, String& response);
static bool uploadSettingsSnapshotNow(const char* reason);
static long readPairingTokenExpiresInSeconds();
static long writePairingCodeExpiresInSeconds();
static String defaultGatewayId();
static String defaultLocalHostname();
static String localPortalUrl();
static void ensureMdnsStarted();
static void handleProvisioningPortal();
static void pollPendingCommand();
static bool retryPendingCommandResult();
static ModbusReadResult runModbusRead(uint16_t startRegister, uint16_t registerCount, const char* label);
static ModbusReadResult runModbusReadInput(uint16_t startRegister, uint16_t registerCount, const char* label);
static bool ensureModbusSession();
static void resetModbusSession(const char* reason);
static void invalidateModbusSession(const char* reason);
static bool runModbusRequest(const String& commandHex, const char* label, const char* startEventType, const char* requestSafety, const char* responseSafety, ModbusReadResult& result);
static bool lockModbusOperation(uint32_t timeoutMs);
static void unlockModbusOperation();
static bool lockHttpOperation(uint32_t timeoutMs);
static void unlockHttpOperation();
static bool httpStartupQuietElapsed(unsigned long now);
static void telemetryTaskLoop(void* parameter);
static void commandPollTaskLoop(void* parameter);
static void handleCommand(String line);
static bool applyLanConfig(
  const String* nextSsid,
  const String* nextPassword,
  const String* nextDeviceId,
  const String* nextTargetMac,
  const String* nextApiUrl,
  const String* nextApiToken,
  const String* nextGatewayId,
  const bool* nextProductionEnabled,
  bool restartWifi,
  String& error
);
static void setBleConnectionEnabled(bool enabled, const char* reason);
static uint16_t mainTelemetryChunkRegisterCount();
static uint16_t mainTelemetryChunkLengthForStart(uint16_t startRegister);
static void buildMainTelemetryLabel(uint16_t startRegister, uint16_t registerCount, char* buffer, size_t bufferSize);
static uint32_t beginRuntimeProbe(RuntimeProbeId id);
static void finishRuntimeProbe(RuntimeProbeId id, uint32_t startedMs, bool ok);
static void updateTelemetrySnapshot(TelemetrySnapshot& snapshot, const ModbusReadResult& result, uint16_t startRegister, uint16_t registerCount, const char* label, const char* safety);
static bool flushTelemetrySnapshot(TelemetrySnapshot& snapshot);
static bool flushOneDirtyTelemetrySnapshot();
static void flushDirtyTelemetrySnapshots();
static void maybeRunTelemetryScheduler();
static bool runFastMainCacheScheduler(unsigned long now);
static bool mergeMainTelemetryCache(const ModbusReadResult& result, uint16_t startRegister, uint16_t registerCount, bool fullRefresh);
static bool updateMainTelemetrySnapshotFromCache(const char* safety);
static String buildMainTelemetryCachePayloadHex();
static bool extractModbusResponseBytes(const String& payloadHex, uint8_t functionCode, std::string& responseBytes);
static bool readRegisterFromResult(const ModbusReadResult& result, uint16_t startRegister, uint16_t registerAddress, uint16_t& value);
static bool loadScheduleState(ScheduleState& state, String& errorMessage);
static bool validateScheduleConflicts(const ScheduleState& state, String& errorMessage);
static bool validateScheduleEnableChange(const char* group, int slot, bool enabled, String& errorMessage);
static bool validateScheduleTimeChange(const char* group, int slot, bool isStart, uint16_t requestedTime, String& errorMessage);
static void persistPendingCommandResult(uint64_t commandId, const char* status, JsonDocument& resultDoc, const char* error);
static void clearPendingCommandResult();
static const char* commandResultStatusForOutcome(bool ok, JsonDocument& result);
static void maybeAccelerateAfterWriteCommand(const char* status, JsonDocument& result);
static void modbusNotifyCallback(BLERemoteCharacteristic* chr, uint8_t* data, size_t length, bool isNotify);
static void printJson(JsonDocument& doc);
static void appendRuntimeLogLine(const String& line);
static bool otaSupported();
static bool performOtaFromUrl(const String& url, const String& md5, String& errorMessage);
static void maybeValidateOtaBoot();
static void emitHttpClientDiag(const char* lane, const char* phase, const String& endpoint, int httpStatus, size_t bodyLength);
static void printLine(const String& line);
static void emitError(const char* code, const char* message);
static bool isAllowedLanCommand(const String& line);
static String bytesToHex(const std::string& data);
static bool hexToBytes(const String& hex, std::string& out);

static void feedWatchdog() {
  esp_task_wdt_reset();
}

static bool lockModbusOperation(uint32_t timeoutMs) {
  if (modbusOperationMutex == nullptr) {
    return true;
  }
  return xSemaphoreTake(modbusOperationMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

static void unlockModbusOperation() {
  if (modbusOperationMutex != nullptr) {
    xSemaphoreGive(modbusOperationMutex);
  }
}

static bool lockHttpOperation(uint32_t timeoutMs) {
  if (httpOperationMutex == nullptr) {
    return true;
  }
  return xSemaphoreTake(httpOperationMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

static void unlockHttpOperation() {
  if (httpOperationMutex != nullptr) {
    xSemaphoreGive(httpOperationMutex);
  }
}

static bool httpStartupQuietElapsed(unsigned long now) {
  return now >= HTTP_STARTUP_QUIET_MS;
}

static uint32_t beginRuntimeProbe(RuntimeProbeId id) {
  uint32_t now = millis();
  portENTER_CRITICAL(&runtimeProbeMux);
  runtimeProbes[id].lastStartMs = now;
  runtimeProbes[id].inFlight = true;
  portEXIT_CRITICAL(&runtimeProbeMux);
  return now;
}

static void finishRuntimeProbe(RuntimeProbeId id, uint32_t startedMs, bool ok) {
  uint32_t now = millis();
  uint32_t duration = now - startedMs;
  portENTER_CRITICAL(&runtimeProbeMux);
  RuntimeProbe& probe = runtimeProbes[id];
  probe.calls++;
  if (!ok) {
    probe.failures++;
  }
  if (duration >= probe.slowThresholdMs) {
    probe.slowCalls++;
  }
  probe.lastDurationMs = duration;
  if (duration > probe.maxDurationMs) {
    probe.maxDurationMs = duration;
  }
  probe.lastEndMs = now;
  probe.inFlight = false;

  runtimeTrace[runtimeTraceNextIndex].name = probe.name;
  runtimeTrace[runtimeTraceNextIndex].startedMs = startedMs;
  runtimeTrace[runtimeTraceNextIndex].durationMs = duration;
  runtimeTrace[runtimeTraceNextIndex].ok = ok;
  runtimeTraceNextIndex = (uint8_t)((runtimeTraceNextIndex + 1) % RUNTIME_TRACE_SIZE);
  if (runtimeTraceNextIndex == 0) {
    runtimeTraceWrapped = true;
  }
  portEXIT_CRITICAL(&runtimeProbeMux);
}

static void addRuntimeProbeJson(JsonArray array) {
  portENTER_CRITICAL(&runtimeProbeMux);
  for (uint8_t i = 0; i < RUNTIME_PROBE_COUNT; i++) {
    const RuntimeProbe& probe = runtimeProbes[i];
    JsonObject item = array.add<JsonObject>();
    item["name"] = probe.name;
    item["slow_threshold_ms"] = probe.slowThresholdMs;
    item["calls"] = probe.calls;
    item["slow_calls"] = probe.slowCalls;
    item["failures"] = probe.failures;
    item["last_http_status"] = probe.lastHttpStatus;
    item["last_duration_ms"] = probe.lastDurationMs;
    item["max_duration_ms"] = probe.maxDurationMs;
    item["last_start_ms"] = probe.lastStartMs;
    item["last_end_ms"] = probe.lastEndMs;
    item["in_flight"] = probe.inFlight;
  }
  portEXIT_CRITICAL(&runtimeProbeMux);
}

static void addRuntimeTraceJson(JsonArray array) {
  portENTER_CRITICAL(&runtimeProbeMux);
  uint8_t count = runtimeTraceWrapped ? RUNTIME_TRACE_SIZE : runtimeTraceNextIndex;
  uint8_t start = runtimeTraceWrapped ? runtimeTraceNextIndex : 0;
  for (uint8_t i = 0; i < count; i++) {
    const RuntimeTraceEntry& entry = runtimeTrace[(start + i) % RUNTIME_TRACE_SIZE];
    if (entry.name == nullptr || strlen(entry.name) == 0) {
      continue;
    }
    JsonObject item = array.add<JsonObject>();
    item["name"] = entry.name;
    item["started_ms"] = entry.startedMs;
    item["duration_ms"] = entry.durationMs;
    item["ok"] = entry.ok;
  }
  portEXIT_CRITICAL(&runtimeProbeMux);
}

static void setRuntimeProbeHttpStatus(RuntimeProbeId id, int32_t httpStatus) {
  portENTER_CRITICAL(&runtimeProbeMux);
  runtimeProbes[id].lastHttpStatus = httpStatus;
  portEXIT_CRITICAL(&runtimeProbeMux);
}

static uint16_t mainTelemetryChunkRegisterCount() {
  uint16_t configured = (uint16_t)LUMENTREE_MAIN_TELEMETRY_CHUNK_REGISTERS;
  if (configured < 1) {
    return 1;
  }
  if (configured > MAIN_TELEMETRY_TOTAL_REGISTERS) {
    return MAIN_TELEMETRY_TOTAL_REGISTERS;
  }
  return configured;
}

static uint16_t mainTelemetryChunkLengthForStart(uint16_t startRegister) {
  uint16_t chunkSize = mainTelemetryChunkRegisterCount();
  if (startRegister >= MAIN_TELEMETRY_TOTAL_REGISTERS) {
    return chunkSize;
  }
  uint16_t remaining = MAIN_TELEMETRY_TOTAL_REGISTERS - startRegister;
  return chunkSize < remaining ? chunkSize : remaining;
}

static void buildMainTelemetryLabel(uint16_t startRegister, uint16_t registerCount, char* buffer, size_t bufferSize) {
  uint16_t endRegister = registerCount == 0 ? startRegister : (uint16_t)(startRegister + registerCount - 1);
  snprintf(buffer, bufferSize, "main_registers_%u_%u", startRegister, endRegister);
}

static void emitBleSessionEvent(const char* type, const char* reason) {
  JsonDocument doc;
  doc["type"] = type;
  doc["uptime_ms"] = millis() - bootMs;
  doc["target_mac"] = targetMac;
  if (reason != nullptr && strlen(reason) > 0) {
    doc["reason"] = reason;
  }
  doc["connected"] = modbusClient != nullptr && modbusClient->isConnected();
  doc["backoff_until_ms"] = modbusReconnectBackoffUntilMs;
  doc["failure_count"] = modbusReconnectFailureCount;
  printJson(doc);
}

static unsigned long telemetryIntervalMs() {
  unsigned long configured = (unsigned long)uploadIntervalSeconds * 1000UL;
  return configured < FAST_TELEMETRY_INTERVAL_MS ? FAST_TELEMETRY_INTERVAL_MS : configured;
}

static bool httpsGapReady(unsigned long now) {
  return lastHttpsAttemptMs == 0 || now - lastHttpsAttemptMs >= HTTPS_MIN_GAP_MS;
}

static void markHttpsAttempt(unsigned long now) {
  lastHttpsAttemptMs = now;
}

static bool bleTelemetryGapReady(unsigned long now) {
  return lastBleTelemetryActionMs == 0 || now - lastBleTelemetryActionMs >= TELEMETRY_BLE_GAP_MS;
}

static void markBleTelemetryAction(unsigned long now) {
  lastBleTelemetryActionMs = now;
}

static void markTelemetrySnapshotDirty(TelemetrySnapshot& snapshot) {
  if (snapshot.valid) {
    snapshot.dirty = true;
  }
}

static void updateTelemetrySnapshot(
  TelemetrySnapshot& snapshot,
  const ModbusReadResult& result,
  uint16_t startRegister,
  uint16_t registerCount,
  const char* label,
  const char* safety
) {
  if (!result.ok) return;
  snapshot.valid = true;
  snapshot.dirty = true;
  snapshot.payloadHex = result.payloadHex;
  snapshot.length = result.length;
  snapshot.notifyCount = result.notifyCount;
  snapshot.startRegister = startRegister;
  snapshot.registerCount = registerCount;
  snapshot.label = label != nullptr ? label : "";
  snapshot.safety = safety != nullptr ? safety : "";
  snapshot.observedMs = millis();
  nextDirtyTelemetryFlushRetryMs = 0;
}

static bool flushTelemetrySnapshot(TelemetrySnapshot& snapshot) {
  if (!snapshot.valid || !snapshot.dirty) return true;
  if (telemetryUploadsDisabled) return true;
  bool ok = postTelemetry(
    snapshot.payloadHex,
    snapshot.notifyCount,
    snapshot.length,
    snapshot.startRegister,
    snapshot.registerCount,
    snapshot.label.c_str(),
    snapshot.safety.c_str()
  );
  if (ok) {
    snapshot.dirty = false;
    nextDirtyTelemetryFlushRetryMs = 0;
  } else {
    nextDirtyTelemetryFlushRetryMs = millis() + TELEMETRY_UPLOAD_RETRY_BACKOFF_MS;
  }
  return ok;
}

static bool flushOneDirtyTelemetrySnapshot() {
  if (latestMainTelemetry.valid && latestMainTelemetry.dirty) {
    return flushTelemetrySnapshot(latestMainTelemetry);
  }
  if (latestSettingsTelemetry.valid && latestSettingsTelemetry.dirty) {
    return flushTelemetrySnapshot(latestSettingsTelemetry);
  }
  if (latestStatsTelemetry.valid && latestStatsTelemetry.dirty) {
    return flushTelemetrySnapshot(latestStatsTelemetry);
  }
  return false;
}

static void flushDirtyTelemetrySnapshots() {
  flushTelemetrySnapshot(latestMainTelemetry);
  flushTelemetrySnapshot(latestSettingsTelemetry);
  flushTelemetrySnapshot(latestStatsTelemetry);
}

static void resetModbusSession(const char* reason) {
  if (modbusClient != nullptr && modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(100);
  }
  modbusCharacteristic = nullptr;
  modbusNotifyRegistered = false;
  modbusReconnectFailureCount = 0;
  modbusReconnectBackoffUntilMs = 0;
  emitBleSessionEvent("ble_session_reset", reason);
}

static void invalidateModbusSession(const char* reason) {
  if (modbusClient != nullptr && modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(100);
  }
  modbusCharacteristic = nullptr;
  modbusNotifyRegistered = false;
  static const unsigned long backoffStepsMs[] = {1000, 2000, 5000, 10000, 30000};
  size_t stepIndex = modbusReconnectFailureCount;
  if (stepIndex >= (sizeof(backoffStepsMs) / sizeof(backoffStepsMs[0]))) {
    stepIndex = (sizeof(backoffStepsMs) / sizeof(backoffStepsMs[0])) - 1;
  }
  modbusReconnectBackoffUntilMs = millis() + backoffStepsMs[stepIndex];
  if (modbusReconnectFailureCount < 255) {
    modbusReconnectFailureCount++;
  }
  emitBleSessionEvent("ble_session_invalidated", reason);
}

static bool ensureModbusSession() {
  if (!bleConnectionEnabled) {
    return false;
  }
  if (targetMac.length() == 0) {
    emitError("target_not_set", "run SET_TARGET before reading BLE Modbus data");
    return false;
  }
  if (!targetAddressTypeKnown) {
    emitError("address_type_unknown", "scan the target once before reading BLE Modbus data");
    return false;
  }

  if (modbusClient != nullptr && modbusClient->isConnected() && modbusCharacteristic != nullptr) {
    return true;
  }

  unsigned long now = millis();
  if ((long)(now - modbusReconnectBackoffUntilMs) < 0) {
    return false;
  }

  if (modbusClient == nullptr) {
    modbusClient = BLEDevice::createClient();
  } else if (modbusClient->isConnected()) {
    modbusClient->disconnect();
    delay(100);
  }

  emitBleSessionEvent("ble_session_connecting", "direct_target_reconnect");
  bool connected = modbusClient->connect(BLEAddress(targetMac.c_str()), targetAddressType);
  if (!connected) {
    emitError("gatt_connect_failed", "could not connect to target for BLE Modbus session");
    invalidateModbusSession("gatt_connect_failed");
    return false;
  }

  BLERemoteService* service = modbusClient->getService(BLEUUID(LUMENTREE_VENDOR_SERVICE_UUID));
  if (service == nullptr) {
    emitError("gatt_service_missing", "FFE0 service not found");
    invalidateModbusSession("gatt_service_missing");
    return false;
  }

  BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(LUMENTREE_VENDOR_CHARACTERISTIC_UUID));
  if (chr == nullptr) {
    emitError("gatt_characteristic_missing", "FFE1 characteristic not found");
    invalidateModbusSession("gatt_characteristic_missing");
    return false;
  }
  if (!chr->canWrite() && !chr->canWriteNoResponse()) {
    emitError("gatt_write_missing", "FFE1 cannot carry Modbus traffic");
    invalidateModbusSession("gatt_write_missing");
    return false;
  }

  modbusCharacteristic = chr;
  modbusNotifyRegistered = false;
  if (chr->canNotify() || chr->canIndicate()) {
    chr->registerForNotify(modbusNotifyCallback);
    delay(100);
    modbusNotifyRegistered = true;
  }

  modbusReconnectFailureCount = 0;
  modbusReconnectBackoffUntilMs = 0;
  emitBleSessionEvent("ble_session_ready", "session_established");
  return true;
}

static bool runModbusRequest(
  const String& commandHex,
  const char* label,
  const char* startEventType,
  const char* requestSafety,
  const char* responseSafety,
  ModbusReadResult& result
) {
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_BLE_REQUEST);
  if (!lockModbusOperation(10000)) {
    finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, false);
    emitError("modbus_busy", "BLE Modbus lane is busy");
    return false;
  }

  std::string commandBytes;
  if (!hexToBytes(commandHex, commandBytes)) {
    unlockModbusOperation();
    finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, false);
    emitError("command_build_failed", "could not build Modbus request");
    return false;
  }
  if (!ensureModbusSession()) {
    unlockModbusOperation();
    finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, false);
    return false;
  }
  if (modbusCharacteristic == nullptr) {
    invalidateModbusSession("characteristic_not_ready");
    unlockModbusOperation();
    finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, false);
    return false;
  }

  JsonDocument sent;
  sent["type"] = startEventType;
  sent["uptime_ms"] = millis() - bootMs;
  sent["target_mac"] = targetMac;
  sent["label"] = label;
  sent["command_hex"] = commandHex;
  sent["write_with_response"] = modbusCharacteristic->canWrite();
  sent["safety"] = requestSafety;
  printJson(sent);

  modbusNotifyReceived = false;
  modbusLastNotifyMs = 0;
  modbusNotifyCount = 0;
  modbusResponseBytes.clear();

  modbusCharacteristic->writeValue((uint8_t*)commandBytes.data(), commandBytes.length(), modbusCharacteristic->canWrite());

  unsigned long deadline = millis() + 5000;
  while (millis() < deadline) {
    feedWatchdog();
    if (!telemetryTaskEnabled) {
      handleProvisioningPortal();
    }
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
    response["safety"] = responseSafety;
    printJson(response);
    unlockModbusOperation();
    finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, true);
    return true;
  }

  if (modbusCharacteristic->canRead()) {
    std::string value = modbusCharacteristic->readValue();
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
      readDoc["safety"] = responseSafety;
      printJson(readDoc);
      unlockModbusOperation();
      finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, true);
      return true;
    }
  }

  invalidateModbusSession("request_timeout_or_empty_response");
  unlockModbusOperation();
  finishRuntimeProbe(PROBE_BLE_REQUEST, probeStartedMs, false);
  return false;
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

static bool extractModbusResponseBytes(const String& payloadHex, uint8_t functionCode, std::string& responseBytes) {
  responseBytes.clear();
  String normalized = payloadHex;
  normalized.trim();
  normalized.toLowerCase();
  if (normalized.length() == 0) return false;

  const char* prefix = functionCode == 0x04 ? "0104" : "0103";
  int separatorIndex = normalized.indexOf("2b2b2b2b");
  if (separatorIndex >= 0) {
    normalized = normalized.substring(separatorIndex + 8);
  }
  int prefixIndex = normalized.indexOf(prefix);
  if (prefixIndex > 0) {
    normalized = normalized.substring(prefixIndex);
  }
  if (!normalized.startsWith(prefix)) return false;
  return hexToBytes(normalized, responseBytes);
}

static bool mergeMainTelemetryCache(
  const ModbusReadResult& result,
  uint16_t startRegister,
  uint16_t registerCount,
  bool fullRefresh
) {
  if (!result.ok || registerCount == 0 || startRegister >= MAIN_TELEMETRY_TOTAL_REGISTERS) {
    return false;
  }
  if ((uint32_t)startRegister + registerCount > MAIN_TELEMETRY_TOTAL_REGISTERS) {
    return false;
  }

  std::string responseBytes;
  if (!extractModbusResponseBytes(result.payloadHex, 0x03, responseBytes)) {
    return false;
  }
  if (responseBytes.length() < 5) {
    return false;
  }
  const uint8_t* response = (const uint8_t*)responseBytes.data();
  if (response[0] != 0x01 || response[1] != 0x03) {
    return false;
  }
  uint16_t byteCount = response[2];
  uint16_t expectedBytes = registerCount * 2;
  if (byteCount < expectedBytes || responseBytes.length() < (size_t)(3 + expectedBytes + 2)) {
    return false;
  }

  size_t sourceOffset = 3;
  size_t targetOffset = (size_t)startRegister * 2;
  memcpy(mainTelemetryCache.data + targetOffset, response + sourceOffset, expectedBytes);
  for (uint16_t index = 0; index < registerCount; index++) {
    uint16_t registerIndex = startRegister + index;
    if (!mainTelemetryCache.registersValid[registerIndex]) {
      mainTelemetryCache.registersValid[registerIndex] = true;
      mainTelemetryCache.validCount++;
    }
  }
  mainTelemetryCache.valid = mainTelemetryCache.validCount == MAIN_TELEMETRY_TOTAL_REGISTERS;
  mainTelemetryCache.updatedMs = millis();
  if (fullRefresh && mainTelemetryCache.valid) {
    mainTelemetryCache.fullRefreshMs = mainTelemetryCache.updatedMs;
  }
  return mainTelemetryCache.valid;
}

static String buildMainTelemetryCachePayloadHex() {
  if (!mainTelemetryCache.valid) return "";
  uint8_t frame[3 + MAIN_TELEMETRY_RESPONSE_DATA_BYTES + 2] = {};
  frame[0] = 0x01;
  frame[1] = 0x03;
  frame[2] = (uint8_t)MAIN_TELEMETRY_RESPONSE_DATA_BYTES;
  memcpy(frame + 3, mainTelemetryCache.data, MAIN_TELEMETRY_RESPONSE_DATA_BYTES);
  uint16_t crc = crc16Modbus(frame, 3 + MAIN_TELEMETRY_RESPONSE_DATA_BYTES);
  frame[3 + MAIN_TELEMETRY_RESPONSE_DATA_BYTES] = (uint8_t)(crc & 0xFF);
  frame[3 + MAIN_TELEMETRY_RESPONSE_DATA_BYTES + 1] = (uint8_t)((crc >> 8) & 0xFF);
  return bytesToHex(frame, sizeof(frame));
}

static bool updateMainTelemetrySnapshotFromCache(const char* safety) {
  String payloadHex = buildMainTelemetryCachePayloadHex();
  if (payloadHex.length() == 0) return false;

  ModbusReadResult cached;
  cached.ok = true;
  cached.payloadHex = payloadHex;
  cached.length = 3 + MAIN_TELEMETRY_RESPONSE_DATA_BYTES + 2;
  cached.notifyCount = 0;
  updateTelemetrySnapshot(
    latestMainTelemetry,
    cached,
    0,
    MAIN_TELEMETRY_TOTAL_REGISTERS,
    "main_registers_0_94",
    safety
  );
  return true;
}

static bool runFastMainCacheScheduler(unsigned long now) {
  if (!bleConnectionEnabled) {
    nextUploadMs = millis() + telemetryIntervalMs();
    return true;
  }
  bool fullRefreshDue = forceMainFullRefresh || !mainTelemetryCache.valid;

  if (fullRefreshDue) {
    JsonDocument plan;
    plan["type"] = "telemetry_scheduler_plan";
    plan["uptime_ms"] = millis() - bootMs;
    plan["tier"] = "main_full_refresh";
    plan["cache_valid"] = mainTelemetryCache.valid;
    plan["cache_valid_registers"] = mainTelemetryCache.validCount;
    plan["interval_ms"] = telemetryIntervalMs();
    printJson(plan);
    ModbusReadResult result = runModbusRead(0, MAIN_TELEMETRY_TOTAL_REGISTERS, "main_registers_0_94_full_refresh");
    bool ok = mergeMainTelemetryCache(result, 0, MAIN_TELEMETRY_TOTAL_REGISTERS, true);
    if (ok) {
      markBleTelemetryAction(millis());
      updateMainTelemetrySnapshotFromCache("function_03_read_only_full_cache_refresh");
      forceMainFullRefresh = false;
      nextMainFullRefreshMs = millis();
      nextUploadMs = millis() + telemetryIntervalMs();
    } else {
      emitError("main_cache_refresh_failed", "could not refresh full main telemetry cache");
      forceMainFullRefresh = true;
      nextMainFullRefreshMs = millis();
      if (mainTelemetryCache.valid) {
        markTelemetrySnapshotDirty(latestMainTelemetry);
      }
    }
    return ok;
  }

  if ((long)(now - nextUploadMs) < 0) {
    return true;
  }

  const size_t rangeCount = sizeof(FAST_MAIN_TELEMETRY_RANGES) / sizeof(FAST_MAIN_TELEMETRY_RANGES[0]);
  const MainTelemetryReadRange& range = FAST_MAIN_TELEMETRY_RANGES[nextFastMainRangeIndex % rangeCount];
  nextFastMainRangeIndex = (uint8_t)((nextFastMainRangeIndex + 1) % rangeCount);

  JsonDocument plan;
  plan["type"] = "telemetry_scheduler_plan";
  plan["uptime_ms"] = millis() - bootMs;
  plan["tier"] = "main_fast_range";
  plan["label"] = range.label;
  plan["start_register"] = range.startRegister;
  plan["register_count"] = range.registerCount;
  plan["cache_valid"] = mainTelemetryCache.valid;
  plan["cache_valid_registers"] = mainTelemetryCache.validCount;
  plan["interval_ms"] = telemetryIntervalMs();
  printJson(plan);

  ModbusReadResult result = runModbusRead(range.startRegister, range.registerCount, range.label);
  bool ok = mergeMainTelemetryCache(result, range.startRegister, range.registerCount, false);
  if (ok) {
    markBleTelemetryAction(millis());
    updateMainTelemetrySnapshotFromCache("function_03_read_only_fast_cache_merge");
  } else {
    emitError("main_cache_fast_read_failed", "could not merge fast main telemetry range");
    if (mainTelemetryCache.valid) {
      markTelemetrySnapshotDirty(latestMainTelemetry);
    }
  }
  nextUploadMs = millis() + telemetryIntervalMs();
  return ok;
}

static void printJson(JsonDocument& doc) {
  String line;
  serializeJson(doc, line);
  Serial0.println(line);
  appendRuntimeLogLine(line);
}

static void appendRuntimeLogLine(const String& line) {
  runtimeLogBuffer[runtimeLogNextIndex] = line;
  runtimeLogNextIndex = (runtimeLogNextIndex + 1) % RUNTIME_LOG_BUFFER_SIZE;
  if (runtimeLogCount < RUNTIME_LOG_BUFFER_SIZE) {
    runtimeLogCount++;
  }
}

static void emitHttpClientDiag(const char* lane, const char* phase, const String& endpoint, int httpStatus, size_t bodyLength) {
  JsonDocument doc;
  doc["type"] = "http_client_diag";
  doc["uptime_ms"] = millis() - bootMs;
  doc["lane"] = lane != nullptr ? lane : "";
  doc["phase"] = phase != nullptr ? phase : "";
  doc["http_status"] = httpStatus;
  doc["body_length"] = bodyLength;
  doc["endpoint"] = endpoint;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["wifi_rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["free_heap"] = ESP.getFreeHeap();
  doc["min_free_heap"] = ESP.getMinFreeHeap();
  doc["largest_8bit_block"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  doc["tls_insecure"] = tlsInsecure;
  printJson(doc);
}

static void printLine(const String& line) {
  Serial0.println(line);
}

static bool otaSupported() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* update = esp_ota_get_next_update_partition(nullptr);
  return running != nullptr && update != nullptr;
}

static bool performOtaFromUrl(const String& url, const String& md5, String& errorMessage) {
  errorMessage = "";
  if (!otaSupported()) {
    errorMessage = "OTA partition layout is not available on this board";
    return false;
  }
  if (url.length() == 0) {
    errorMessage = "url is required";
    return false;
  }
  if (!ensureWifiConnected()) {
    errorMessage = "wifi is not connected";
    return false;
  }
  if (!lockHttpOperation(HTTP_BUSY_SKIP_TIMEOUT_MS)) {
    errorMessage = "HTTP lane is busy";
    return false;
  }

  otaInProgress = true;
  otaLastOk = false;
  otaLastError = "";
  otaLastUrl = url;
  otaStartedMs = millis();
  otaFinishedMs = 0;
  otaLastWrittenBytes = 0;

  JsonDocument start;
  start["type"] = "ota_start";
  start["uptime_ms"] = millis() - bootMs;
  start["url"] = url;
  start["safety"] = "dual_slot_ota_with_pending_verify_rollback";
  printJson(start);

  HTTPClient http;
  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  bool beginOk = http.begin(secureClient, url);
  if (!beginOk) {
    unlockHttpOperation();
    otaInProgress = false;
    errorMessage = "OTA HTTP begin failed";
    otaLastError = errorMessage;
    JsonDocument fail;
    fail["type"] = "ota_failed";
    fail["reason"] = errorMessage;
    fail["uptime_ms"] = millis() - bootMs;
    printJson(fail);
    return false;
  }

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    http.end();
    unlockHttpOperation();
    otaInProgress = false;
    errorMessage = "OTA download failed, http status " + String(status);
    otaLastError = errorMessage;
    JsonDocument fail;
    fail["type"] = "ota_failed";
    fail["reason"] = errorMessage;
    fail["http_status"] = status;
    fail["uptime_ms"] = millis() - bootMs;
    printJson(fail);
    return false;
  }

  int contentLength = http.getSize();
  if (!Update.begin(contentLength > 0 ? (size_t)contentLength : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    http.end();
    unlockHttpOperation();
    otaInProgress = false;
    errorMessage = "Update.begin failed: " + String(Update.errorString());
    otaLastError = errorMessage;
    return false;
  }
  if (md5.length() > 0 && !Update.setMD5(md5.c_str())) {
    Update.abort();
    http.end();
    unlockHttpOperation();
    otaInProgress = false;
    errorMessage = "invalid md5 format";
    otaLastError = errorMessage;
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0;
  int remaining = contentLength;
  uint8_t buffer[2048];
  while (http.connected() && (remaining > 0 || contentLength < 0)) {
    feedWatchdog();
    size_t available = stream->available();
    if (available == 0) {
      delay(5);
      continue;
    }
    size_t toRead = available;
    if (toRead > sizeof(buffer)) toRead = sizeof(buffer);
    int readLen = stream->readBytes(buffer, toRead);
    if (readLen <= 0) {
      delay(1);
      continue;
    }
    size_t writeLen = Update.write(buffer, (size_t)readLen);
    if (writeLen != (size_t)readLen) {
      Update.abort();
      http.end();
      unlockHttpOperation();
      otaInProgress = false;
      errorMessage = "OTA write failed";
      otaLastError = errorMessage;
      return false;
    }
    written += writeLen;
    if (remaining > 0) {
      remaining -= readLen;
    }
  }
  otaLastWrittenBytes = written;
  if (contentLength > 0 && written != (size_t)contentLength) {
    Update.abort();
    http.end();
    unlockHttpOperation();
    otaInProgress = false;
    errorMessage = "written bytes mismatch";
    otaLastError = errorMessage;
    return false;
  }
  if (!Update.end(true)) {
    http.end();
    unlockHttpOperation();
    otaInProgress = false;
    errorMessage = "Update.end failed: " + String(Update.errorString());
    otaLastError = errorMessage;
    return false;
  }

  http.end();
  unlockHttpOperation();
  otaInProgress = false;
  otaLastOk = true;
  otaLastError = "";
  otaFinishedMs = millis();
  otaAwaitingValidation = true;
  otaValidationDone = false;
  otaBootValidateAfterMs = millis() + 30000UL;
  otaLastVersion = String(FW_NAME) + "/" + FW_VERSION;
  prefs.putBool("ota_last_ok", true);
  prefs.putString("ota_last_url", otaLastUrl);
  prefs.putString("ota_last_ver", otaLastVersion);
  prefs.putULong("ota_last_ms", otaFinishedMs);

  JsonDocument ok;
  ok["type"] = "ota_downloaded";
  ok["uptime_ms"] = millis() - bootMs;
  ok["written_bytes"] = written;
  ok["content_length"] = contentLength;
  ok["rebooting"] = true;
  ok["safety"] = "boot_pending_verify_then_mark_valid_or_rollback";
  printJson(ok);
  return true;
}

static void maybeValidateOtaBoot() {
  if (!otaAwaitingValidation || otaValidationDone) return;
  unsigned long now = millis();
  if ((long)(now - otaBootValidateAfterMs) < 0) return;
  if (WiFi.status() != WL_CONNECTED) return;

  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  JsonDocument doc;
  doc["type"] = "ota_boot_validation";
  doc["uptime_ms"] = millis() - bootMs;
  doc["result"] = err == ESP_OK ? "valid" : "failed";
  doc["esp_err"] = (int)err;
  printJson(doc);

  otaValidationDone = err == ESP_OK;
  otaAwaitingValidation = err != ESP_OK;
  if (err == ESP_OK) {
    prefs.putBool("ota_boot_validated", true);
    prefs.putULong("ota_boot_validated_ms", millis());
  }
}

static bool isAllowedLanCommand(const String& line) {
  if (line == "HELP" || line == "STATUS" || line == "CONFIG" || line == "START_AP"
      || line == "SCAN_WIFI" || line == "SCAN_BLE" || line == "BLE_CANDIDATES"
      || line == "READ_MAIN_ONCE" || line == "READ_STATS_ONCE" || line == "UPLOAD_ONCE"
      || line == "READ_CELLS_ONCE") {
    return true;
  }
  return line.startsWith("READ_RANGE ")
    || line.startsWith("SET_UPLOAD_INTERVAL ")
    || line.startsWith("SET_DEVICE_ID ")
    || line.startsWith("SET_TARGET_MAC ")
    || line.startsWith("SET_WIFI ")
    || line.startsWith("SET_BLE_CONNECTION ")
    || line.startsWith("SET_PRODUCTION ")
    || line.startsWith("SET_BACKGROUND_TELEMETRY ")
    || line.startsWith("SET_TELEMETRY_UPLOADS ");
}

static void loadConfig() {
  prefs.begin("lumentree", false);
  localHostname = defaultLocalHostname();
  wifiSsid = prefs.getString("wifi_ssid", LUMENTREE_DEFAULT_WIFI_SSID);
  wifiPassword = prefs.getString("wifi_pass", LUMENTREE_DEFAULT_WIFI_PASSWORD);
  apiUrl = prefs.getString("api_url", LUMENTREE_DEFAULT_API_URL);
  apiToken = prefs.getString("api_token", LUMENTREE_DEFAULT_API_TOKEN);
  deviceId = prefs.getString("device_id", LUMENTREE_DEFAULT_DEVICE_ID);
  gatewayId = prefs.getString("gateway_id", LUMENTREE_DEFAULT_GATEWAY_ID);
  if (gatewayId.length() == 0) {
    gatewayId = defaultGatewayId();
  }
  targetMac = normalizeMac(prefs.getString("target_mac", LUMENTREE_DEFAULT_TARGET_MAC));
  uploadIntervalSeconds = prefs.getUShort("upload_s", DEFAULT_UPLOAD_INTERVAL_SECONDS);
  productionEnabled = prefs.getBool("prod", LUMENTREE_DEFAULT_PRODUCTION_ENABLED != 0);
  tlsInsecure = prefs.getBool("tls_insec", LUMENTREE_DEFAULT_TLS_INSECURE != 0);
  bleConnectionEnabled = prefs.getBool("ble_en", true);
  uploadIntervalSeconds = (uint16_t)constrain(uploadIntervalSeconds, MIN_UPLOAD_INTERVAL_SECONDS, 3600);
  targetAddressTypeKnown = targetMac.length() > 0;
  pairingStatus = targetMac.length() > 0 ? "paired" : "unconfigured";
  pendingCommandResultId = (uint64_t)prefs.getULong64("pend_cmd_id", 0);
  pendingCommandResultStatus = prefs.getString("pend_cmd_st", "");
  pendingCommandResultError = prefs.getString("pend_cmd_er", "");
  pendingCommandResultPayload = prefs.getString("pend_cmd_js", "");
}

static void saveStringConfig(const char* key, const String& value) {
  prefs.putString(key, value);
}

static bool applyLanConfig(
  const String* nextSsid,
  const String* nextPassword,
  const String* nextDeviceId,
  const String* nextTargetMac,
  const String* nextApiUrl,
  const String* nextApiToken,
  const String* nextGatewayId,
  const bool* nextProductionEnabled,
  bool restartWifi,
  String& error
) {
  error = "";
  bool targetChanged = false;
  bool wifiChanged = false;

  if (nextSsid != nullptr) {
    String value = *nextSsid;
    value.trim();
    wifiSsid = value;
    saveStringConfig("wifi_ssid", wifiSsid);
    wifiChanged = true;
  }
  if (nextPassword != nullptr) {
    wifiPassword = *nextPassword;
    saveStringConfig("wifi_pass", wifiPassword);
    wifiChanged = true;
  }
  if (nextDeviceId != nullptr) {
    String value = *nextDeviceId;
    value.trim();
    if (value.length() == 0) {
      error = "device_id must not be empty";
      return false;
    }
    deviceId = value;
    saveStringConfig("device_id", deviceId);
  }
  if (nextTargetMac != nullptr) {
    String value = normalizeMac(*nextTargetMac);
    if (value.length() == 0) {
      error = "target_mac must not be empty";
      return false;
    }
    if (value != targetMac) {
      targetChanged = true;
    }
    targetMac = value;
    targetAddressTypeKnown = true;
    pairingStatus = "paired";
    saveStringConfig("target_mac", targetMac);
  }
  if (nextApiUrl != nullptr) {
    String value = *nextApiUrl;
    value.trim();
    apiUrl = value;
    saveStringConfig("api_url", apiUrl);
  }
  if (nextApiToken != nullptr) {
    String value = *nextApiToken;
    value.trim();
    apiToken = value;
    saveStringConfig("api_token", apiToken);
  }
  if (nextGatewayId != nullptr) {
    String value = *nextGatewayId;
    value.trim();
    if (value.length() == 0) {
      gatewayId = defaultGatewayId();
    } else {
      gatewayId = value;
    }
    saveStringConfig("gateway_id", gatewayId);
  }
  if (nextProductionEnabled != nullptr) {
    productionEnabled = *nextProductionEnabled;
    prefs.putBool("prod", productionEnabled);
  }

  if (targetChanged) {
    resetModbusSession("lan_config_target_changed");
    postGatewayStatus("lan_config_target_changed");
  }
  if (wifiChanged && restartWifi) {
    WiFi.disconnect(true);
  }

  return true;
}

static void setBleConnectionEnabled(bool enabled, const char* reason) {
  bleConnectionEnabled = enabled;
  prefs.putBool("ble_en", bleConnectionEnabled);
  if (!bleConnectionEnabled) {
    resetModbusSession(reason != nullptr ? reason : "ble_disabled");
  }
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
  doc["local_hostname"] = localHostname;
  doc["local_url"] = WiFi.status() == WL_CONNECTED ? localPortalUrl() : "";
  doc["api_url"] = apiUrl;
  doc["api_token_configured"] = apiToken.length() > 0;
  doc["device_id"] = deviceId;
  doc["gateway_id"] = gatewayId;
  doc["target_mac"] = targetMac;
  doc["pairing_status"] = pairingStatus;
  doc["candidate_count"] = bleCandidates.size();
  doc["upload_interval_seconds"] = uploadIntervalSeconds;
  doc["tls_insecure"] = tlsInsecure;
  doc["background_telemetry_disabled"] = backgroundTelemetryDisabled;
  doc["telemetry_uploads_disabled"] = telemetryUploadsDisabled;
  doc["telemetry_task_enabled"] = telemetryTaskEnabled;
  doc["main_telemetry_chunk_registers"] = mainTelemetryChunkRegisterCount();
  doc["fast_main_cache_enabled"] = LUMENTREE_USE_FAST_MAIN_CACHE != 0;
  doc["main_telemetry_cache_valid"] = mainTelemetryCache.valid;
  doc["main_telemetry_cache_valid_registers"] = mainTelemetryCache.validCount;
  doc["main_full_refresh_interval_ms"] = MAIN_FULL_REFRESH_INTERVAL_MS;
  doc["settings_poll_interval_ms"] = SETTINGS_UPLOAD_INTERVAL_MS;
  doc["command_poll_interval_ms"] = COMMAND_POLL_INTERVAL_MS;
  doc["command_polling_disabled"] = commandPollingDisabled;
  doc["command_poll_task_enabled"] = commandPollTaskEnabled;
  doc["provisioning_portal_active"] = provisioningPortalActive;
  doc["provisioning_ap_ssid"] = provisioningApSsid;
  doc["read_pairing_token_active"] = readPairingTokenExpiresInSeconds() > 0;
  doc["read_pairing_token_expires_in_seconds"] = readPairingTokenExpiresInSeconds();
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
  doc["local_hostname"] = localHostname;
  doc["local_url"] = WiFi.status() == WL_CONNECTED ? localPortalUrl() : "";
  doc["api_url"] = apiUrl;
  doc["api_token_configured"] = apiToken.length() > 0;
  doc["device_id"] = deviceId;
  doc["gateway_id"] = gatewayId;
  doc["upload_interval_seconds"] = uploadIntervalSeconds;
  doc["production_enabled"] = productionEnabled;
  doc["background_telemetry_disabled"] = backgroundTelemetryDisabled;
  doc["telemetry_uploads_disabled"] = telemetryUploadsDisabled;
  doc["telemetry_task_enabled"] = telemetryTaskEnabled;
  doc["main_telemetry_chunk_registers"] = mainTelemetryChunkRegisterCount();
  doc["fast_main_cache_enabled"] = LUMENTREE_USE_FAST_MAIN_CACHE != 0;
  doc["main_telemetry_cache_valid"] = mainTelemetryCache.valid;
  doc["main_telemetry_cache_valid_registers"] = mainTelemetryCache.validCount;
  doc["main_full_refresh_interval_ms"] = MAIN_FULL_REFRESH_INTERVAL_MS;
  doc["settings_poll_interval_ms"] = SETTINGS_UPLOAD_INTERVAL_MS;
  doc["command_poll_interval_ms"] = COMMAND_POLL_INTERVAL_MS;
  doc["command_polling_disabled"] = commandPollingDisabled;
  doc["command_poll_task_enabled"] = commandPollTaskEnabled;
  doc["provisioning_portal_active"] = provisioningPortalActive;
  doc["provisioning_ap_ssid"] = provisioningApSsid;
  doc["read_pairing_token_active"] = readPairingTokenExpiresInSeconds() > 0;
  doc["read_pairing_token_expires_in_seconds"] = readPairingTokenExpiresInSeconds();
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

static String defaultGatewayId() {
  uint64_t chipId = ESP.getEfuseMac();
  char buffer[40];
  snprintf(buffer, sizeof(buffer), "esp32-lumentree-%06llx", (unsigned long long)(chipId & 0xFFFFFF));
  return String(buffer);
}

static String defaultLocalHostname() {
  uint64_t chipId = ESP.getEfuseMac();
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "lumentree-%04llx", (unsigned long long)(chipId & 0xFFFF));
  return String(buffer);
}

static String localPortalUrl() {
  return "http://" + localHostname + ".local";
}

static void ensureMdnsStarted() {
  if (WiFi.status() != WL_CONNECTED || localHostname.length() == 0) return;
  if (mdnsStarted) return;
  if (!MDNS.begin(localHostname.c_str())) {
    emitError("mdns_start_failed", "could not start mDNS responder");
    return;
  }
  MDNS.addService("http", "tcp", 80);
  mdnsStarted = true;

  JsonDocument doc;
  doc["type"] = "mdns_started";
  doc["uptime_ms"] = millis() - bootMs;
  doc["hostname"] = localHostname;
  doc["local_url"] = localPortalUrl();
  printJson(doc);
}

static String generatePairingToken() {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  String token;
  token.reserve(8);
  for (int i = 0; i < 8; i++) {
    token += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  }
  return token;
}

static String generateReadPairingToken() {
  return generatePairingToken();
}

static String generateWritePairingCode() {
  return generatePairingToken();
}

static long readPairingTokenExpiresInSeconds() {
  if (readPairingToken.length() == 0 || readPairingTokenExpiresMs == 0) return 0;
  long remaining = (long)(readPairingTokenExpiresMs - millis());
  if (remaining <= 0) return 0;
  return remaining / 1000;
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
    "main{max-width:640px;margin:0 auto;padding:20px}"
    "h1{font-size:22px;margin:10px 0 4px;color:#61d394}"
    "p{color:#a7b4bf;font-size:14px;line-height:1.45}"
    "label{display:block;margin-top:14px;font-size:13px;color:#b9c6cf}"
    "input{width:100%;box-sizing:border-box;padding:11px;margin-top:5px;border:1px solid #314454;border-radius:8px;background:#0a1118;color:#fff;font-size:15px}"
    "button{margin-top:12px;padding:11px 14px;border:0;border-radius:8px;background:#61d394;color:#07100b;font-weight:700;font-size:15px;cursor:pointer}"
    "button.secondary{background:#263746;color:#d8e2ea}"
    ".actions{display:flex;gap:10px;flex-wrap:wrap}"
    ".net{padding:10px;border-bottom:1px solid #263746}.net:last-child{border-bottom:0}"
    ".net small{display:block;color:#8da0ad;margin-top:3px}"
    ".token{font-size:24px;color:#61d394;letter-spacing:2px;font-weight:700}"
    "pre{white-space:pre-wrap;background:#0a1118;border:1px solid #263746;padding:12px;border-radius:8px;margin-top:18px;color:#c8d6df;font-size:12px}"
    "section{background:#12202b;border:1px solid #203241;border-radius:12px;padding:16px;margin-top:16px}"
    "</style></head><body><main><h1>Lumentree Local</h1>"
  );
  if (provisioningPortalActive) {
    page += F(
      "<p>First-time onboarding mode. Enter Wi-Fi only. After the ESP32 joins Wi-Fi, reopen this portal on the local network using the canonical URL shown below. If .local resolution fails on your network, use the router DHCP client list to find the ESP32 IP.</p>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>Next Step</h2><p>After reboot, open <b>"
    );
    page += htmlEscape(localPortalUrl());
    page += F(
      "</b></p></section>"
      "<section><div class='actions'><button type='button' onclick='scan()'>Scan Wi-Fi</button></div><div id='nets'></div></section>"
      "<section><form onsubmit='saveWifi(event)'><label>Wi-Fi SSID</label><input id='ssid' name='ssid' value='"
    );
    page += htmlEscape(wifiSsid);
    page += F(
      "' required><label>Wi-Fi Password</label><input id='pass' name='password' type='password' placeholder='Leave blank to keep current password'><button type='submit'>Save Wi-Fi & Reboot</button></form></section>"
    );
  } else {
    page += F(
      "<p>Local network mode. Use the canonical local URL shown below. Scan nearby inverters, choose the intended device, then generate the required read token and optional write token for Home Assistant.</p>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>Local URL</h2><p><b>"
    );
    page += htmlEscape(localPortalUrl());
    page += F(
      "</b><br><span style='color:#8da0ad'>IP is only a fallback if .local does not resolve on your network.</span></p></section>"
      "<section><div class='actions'><button type='button' onclick='bleScan()'>Scan BLE</button><button type='button' class='secondary' onclick='status()'>Refresh Status</button></div><div id='summary'></div></section>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>Wi-Fi Config</h2><form onsubmit='saveLocalConfig(event)'><label>Wi-Fi SSID</label><input id='local_ssid' name='local_ssid' value='"
    );
    page += htmlEscape(wifiSsid);
    page += F(
      "'><label>Wi-Fi Password</label><input id='local_pass' name='local_pass' type='password' placeholder='Leave blank to keep current password'><button type='submit'>Save Wi-Fi On LAN</button></form></section>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>BLE Session</h2><div class='actions'><button type='button' onclick='setBleConnection(true)'>Enable BLE</button><button type='button' class='secondary' onclick='setBleConnection(false)'>Disable BLE</button></div><p>Disabling BLE keeps local web and server connectivity alive, but stops inverter reads until BLE is enabled again.</p></section>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>BLE Candidates</h2><div id='ble'></div></section>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>LAN Control</h2><div class='actions'><button type='button' onclick='runCommand(`STATUS`)'>STATUS</button><button type='button' class='secondary' onclick='runCommand(`READ_MAIN_ONCE`)'>READ MAIN</button><button type='button' class='secondary' onclick='readLogs()'>Refresh Logs</button><button type='button' class='secondary' onclick='rebootDevice()'>Reboot</button></div><label>Command</label><input id='cmd' placeholder='READ_RANGE 0 10'><button type='button' onclick='submitCommand()'>Run Command</button></section>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>Read Access</h2><p>Home Assistant setup now requires a read pairing token. Tokens use uppercase letters for display, but they are not case-sensitive.</p><button type='button' onclick='readToken()'>Generate read pairing token</button><div id='read'></div></section>"
      "<section><h2 style='font-size:17px;margin:0 0 6px;color:#e8edf2'>Write Access</h2><p>Write remains optional. Only generate a write token when Home Assistant should be allowed to change inverter settings. Tokens use uppercase letters for display, but they are not case-sensitive.</p><button type='button' onclick='writeCode()'>Generate write pairing token</button><div id='write'></div></section>"
    );
  }
  page += F(
    "<pre id='out'></pre><script>"
    "function q(id){return document.getElementById(id)}"
    "function scan(){fetch('/api/scan').then(r=>r.json()).then(d=>{let n=q('nets');if(!n)return;n.innerHTML=(d.networks||[]).map(x=>`<div class=net onclick=\\\"q('ssid').value='${x.ssid.replace(/'/g,'&#39;')}'\\\">${x.ssid}<small>${x.rssi} dBm ${x.secure?'locked':'open'}</small></div>`).join('')||'<div class=net>No networks</div>'})}"
    "function renderSummary(d){let s=q('summary');if(!s)return;s.innerHTML=`<p>Gateway: <b>${d.gateway_id||'unknown'}</b><br>Local URL: <b>${d.local_url||'n/a'}</b><br>Wi-Fi: <b>${d.wifi_connected?'connected':'disconnected'}</b><br>IP fallback: <b>${d.ip||'n/a'}</b><br>BLE enabled: <b>${d.ble_connection_enabled?'yes':'no'}</b><br>BLE session: <b>${d.ble_session_connected?'connected':'idle'}</b><br>Device ID: <b>${d.device_id||'unbound'}</b><br>Target MAC: <b>${d.target_mac||'unbound'}</b><br>Pairing: <b>${d.pairing_status||'unknown'}</b></p>`;let ls=q('local_ssid');if(ls&&d.wifi_configured&&(!ls.value||ls.value!==d.wifi_ssid)){ls.value=d.wifi_ssid||''}}"
    "function esc(v){return String(v||'').replace(/[&<>\"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[m]))}"
    "function renderBle(d){let b=q('ble');if(!b)return;let list=d.candidates||[];b.innerHTML=list.map((x,i)=>`<div class=net><b>${esc(x.name||x.mac)}</b><small>${esc(x.mac)} address_type=${x.address_type}</small><small>${x.rssi} dBm score ${x.score||0}</small><button type='button' onclick='selectCandidateByIndex(${i})'>Use This Device</button></div>`).join('')||'<div class=net>No candidates yet</div>'}"
    "function selectCandidateByIndex(index){fetch('/api/status').then(r=>r.json()).then(d=>{let list=d.candidates||[];if(index<0||index>=list.length){throw new Error('candidate index out of range')}let x=list[index];return selectCandidate(x.name||'',x.mac||'',x.address_type||0)})}"
    "function renderRead(d){let r=q('read');if(!r)return;if(!d.read_pairing_token_active){r.innerHTML='<p>No active read pairing token.</p>';return}if(d.read_pairing_token){r.innerHTML=`<p class=\\\"token\\\">${d.read_pairing_token}</p><p>Expires in ${d.read_pairing_token_expires_in_seconds}s. Enter this together with Device ID in Home Assistant. Uppercase is shown for clarity, but the token is not case-sensitive.</p>`}else{r.innerHTML=`<p>Read pairing token active. Expires in ${d.read_pairing_token_expires_in_seconds}s.</p>`}}"
    "function renderWrite(d){let w=q('write');if(!w)return;if(!d.write_pairing_code_active){w.innerHTML='<p>No active write pairing token.</p>';return}if(d.write_pairing_code){w.innerHTML=`<p class=\\\"token\\\">${d.write_pairing_code}</p><p>Expires in ${d.write_pairing_code_expires_in_seconds}s. Optional for Home Assistant write access. Uppercase is shown for clarity, but the token is not case-sensitive.</p>`}else{w.innerHTML=`<p>Write pairing token active. Expires in ${d.write_pairing_code_expires_in_seconds}s.</p>`}}"
    "function bleScan(){q('out').textContent='Scanning BLE...';fetch('/api/ble_scan',{method:'POST'}).then(r=>r.json()).then(d=>{renderSummary(d);renderBle(d);q('out').textContent=JSON.stringify(d,null,2)})}"
    "function selectCandidate(deviceId,mac,addressType){q('out').textContent='Binding selected candidate...';fetch('/api/select_candidate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({device_id:deviceId,mac:mac,address_type:addressType})}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);status()})}"
    "function runCommand(command){q('out').textContent='Running command...';fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:command})}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);setTimeout(readLogs,500)})}"
    "function submitCommand(){let command=q('cmd').value.trim();if(!command){return}runCommand(command)}"
    "function readLogs(){fetch('/api/logs').then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2)})}"
    "function rebootDevice(){q('out').textContent='Rebooting...';fetch('/api/reboot',{method:'POST'}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2)})}"
    "function saveLocalConfig(e){e.preventDefault();let data={ssid:q('local_ssid').value,password:q('local_pass').value,restart_wifi:true,reboot:false};fetch('/api/configure',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);q('local_pass').value='';setTimeout(status,1000)})}"
    "function setBleConnection(enabled){q('out').textContent=(enabled?'Enabling':'Disabling')+' BLE...';fetch('/api/ble_connection',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:enabled})}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);setTimeout(status,800)})}"
    "function readToken(){q('out').textContent='Generating read pairing token...';fetch('/api/read_token',{method:'POST'}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);renderRead(d)})}"
    "function writeCode(){q('out').textContent='Generating write pairing token...';fetch('/api/write_code',{method:'POST'}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);renderWrite(d)})}"
    "function saveWifi(e){e.preventDefault();let data={ssid:q('ssid').value,password:q('pass').value};fetch('/api/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}).then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2)})}"
    "function status(){fetch('/api/status').then(r=>r.json()).then(d=>{q('out').textContent=JSON.stringify(d,null,2);renderSummary(d);renderBle(d);renderRead(d);renderWrite(d)})}"
    "status();</script></main></body></html>"
  );
  server.send(200, "text/html; charset=utf-8", page);
}

static void setupProvisioningWebServer() {
  if (portalServerStarted) return;
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
    doc["wifi_ssid"] = wifiSsid.length() > 0 ? wifiSsid : "";
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["local_hostname"] = localHostname;
    doc["local_url"] = localPortalUrl();
    doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    doc["api_url"] = apiUrl;
    doc["api_token_configured"] = apiToken.length() > 0;
    doc["device_id"] = deviceId;
    doc["gateway_id"] = gatewayId;
    doc["target_mac"] = targetMac;
    doc["pairing_status"] = pairingStatus;
    doc["candidate_count"] = bleCandidates.size();
    doc["read_pairing_token_active"] = readPairingTokenExpiresInSeconds() > 0;
    doc["read_pairing_token_expires_in_seconds"] = readPairingTokenExpiresInSeconds();
    doc["write_pairing_code_active"] = writePairingCodeExpiresInSeconds() > 0;
    doc["write_pairing_code_expires_in_seconds"] = writePairingCodeExpiresInSeconds();
    JsonArray candidates = doc["candidates"].to<JsonArray>();
    addCandidatesJson(candidates);
    doc["upload_interval_seconds"] = uploadIntervalSeconds;
    doc["production_enabled"] = productionEnabled;
    doc["background_telemetry_disabled"] = backgroundTelemetryDisabled;
    doc["telemetry_uploads_disabled"] = telemetryUploadsDisabled;
    doc["telemetry_task_enabled"] = telemetryTaskEnabled;
    doc["main_telemetry_chunk_registers"] = mainTelemetryChunkRegisterCount();
    doc["fast_main_cache_enabled"] = LUMENTREE_USE_FAST_MAIN_CACHE != 0;
    doc["main_telemetry_cache_valid"] = mainTelemetryCache.valid;
    doc["main_telemetry_cache_valid_registers"] = mainTelemetryCache.validCount;
    doc["main_full_refresh_interval_ms"] = MAIN_FULL_REFRESH_INTERVAL_MS;
    doc["settings_poll_interval_ms"] = SETTINGS_UPLOAD_INTERVAL_MS;
    doc["command_poll_interval_ms"] = COMMAND_POLL_INTERVAL_MS;
    doc["command_polling_disabled"] = commandPollingDisabled;
    doc["command_poll_task_enabled"] = commandPollTaskEnabled;
    doc["ble_connection_enabled"] = bleConnectionEnabled;
    doc["ble_session_connected"] = modbusClient != nullptr && modbusClient->isConnected() && modbusCharacteristic != nullptr;
    doc["ota_supported"] = otaSupported();
    doc["ota_in_progress"] = otaInProgress;
    doc["ota_last_ok"] = otaLastOk;
    doc["ota_last_error"] = otaLastError;
    doc["ota_last_url"] = otaLastUrl;
    doc["ota_last_version"] = otaLastVersion;
    doc["ota_last_written_bytes"] = otaLastWrittenBytes;
    doc["ota_started_ms"] = otaStartedMs;
    doc["ota_finished_ms"] = otaFinishedMs;
    doc["ota_awaiting_validation"] = otaAwaitingValidation;
    doc["ota_validation_done"] = otaValidationDone;
    JsonArray runtimeProbesJson = doc["runtime_probes"].to<JsonArray>();
    addRuntimeProbeJson(runtimeProbesJson);
    JsonArray runtimeTraceJson = doc["runtime_trace"].to<JsonArray>();
    addRuntimeTraceJson(runtimeTraceJson);
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/logs", HTTP_GET, []() {
    JsonDocument doc;
    doc["ok"] = true;
    size_t limit = 100;
    if (server.hasArg("limit")) {
      int requested = server.arg("limit").toInt();
      if (requested > 0) limit = (size_t)requested;
    }
    if (limit > runtimeLogCount) limit = runtimeLogCount;
    JsonArray logs = doc["logs"].to<JsonArray>();
    for (size_t i = 0; i < limit; i++) {
      size_t offsetFromNewest = limit - 1 - i;
      size_t idx = (runtimeLogNextIndex + RUNTIME_LOG_BUFFER_SIZE - 1 - offsetFromNewest) % RUNTIME_LOG_BUFFER_SIZE;
      logs.add(runtimeLogBuffer[idx]);
    }
    doc["count"] = runtimeLogCount;
    doc["limit"] = limit;
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/ota_status", HTTP_GET, []() {
    JsonDocument doc;
    doc["ok"] = true;
    doc["ota_supported"] = otaSupported();
    doc["ota_in_progress"] = otaInProgress;
    doc["ota_last_ok"] = otaLastOk;
    doc["ota_last_error"] = otaLastError;
    doc["ota_last_url"] = otaLastUrl;
    doc["ota_last_version"] = otaLastVersion;
    doc["ota_last_written_bytes"] = otaLastWrittenBytes;
    doc["ota_started_ms"] = otaStartedMs;
    doc["ota_finished_ms"] = otaFinishedMs;
    doc["ota_awaiting_validation"] = otaAwaitingValidation;
    doc["ota_validation_done"] = otaValidationDone;
    String body;
    serializeJson(doc, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/ota", HTTP_POST, []() {
    if (!otaSupported()) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"ota_not_supported_on_current_partition_layout\"}");
      return;
    }
    if (otaInProgress) {
      server.send(409, "application/json", "{\"ok\":false,\"error\":\"ota_already_in_progress\"}");
      return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }
    String url = doc["url"] | "";
    String md5 = doc["md5"] | "";
    url.trim();
    md5.trim();
    String otaError;
    bool ok = performOtaFromUrl(url, md5, otaError);
    if (!ok) {
      JsonDocument result;
      result["ok"] = false;
      result["error"] = otaError;
      String body;
      serializeJson(result, body);
      server.send(502, "application/json", body);
      return;
    }
    JsonDocument result;
    result["ok"] = true;
    result["rebooting"] = true;
    result["ota_last_url"] = otaLastUrl;
    result["ota_written_bytes"] = otaLastWrittenBytes;
    result["safety"] = "pending_verify_boot_gate_with_auto_rollback";
    String body;
    serializeJson(result, body);
    server.send(200, "application/json", body);
    delay(400);
    ESP.restart();
  });

  server.on("/api/ota/rollback", HTTP_POST, []() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running == nullptr || esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"rollback_only_available_while_pending_verify\"}");
      return;
    }
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    JsonDocument result;
    result["ok"] = err == ESP_OK;
    result["esp_err"] = (int)err;
    result["rebooting"] = err == ESP_OK;
    String body;
    serializeJson(result, body);
    server.send(err == ESP_OK ? 200 : 500, "application/json", body);
  });

  server.on("/api/command", HTTP_POST, []() {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }
    String command = doc["command"] | "";
    command.trim();
    if (command.length() == 0) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"command is required\"}");
      return;
    }
    if (!isAllowedLanCommand(command)) {
      server.send(403, "application/json", "{\"ok\":false,\"error\":\"command not allowed over lan\"}");
      return;
    }
    handleCommand(command);
    JsonDocument result;
    result["ok"] = true;
    result["command"] = command;
    String body;
    serializeJson(result, body);
    server.send(200, "application/json", body);
  });

  server.on("/api/reboot", HTTP_POST, []() {
    JsonDocument result;
    result["ok"] = true;
    result["rebooting"] = true;
    result["local_hostname"] = localHostname;
    result["local_url"] = localPortalUrl();
    String body;
    serializeJson(result, body);
    server.send(200, "application/json", body);
    delay(500);
    ESP.restart();
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

  server.on("/api/read_token", HTTP_POST, []() {
    if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0 || deviceId.length() == 0 || targetMac.length() == 0) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"gateway_id, device_id, target_mac, api_url, and api_token are required\"}");
      return;
    }
    String token = generateReadPairingToken();
    String response;
    if (!postReadPairingToken(token, response)) {
      JsonDocument doc;
      doc["ok"] = false;
      doc["error"] = "failed to register read pairing token with API";
      doc["response"] = response.substring(0, 300);
      String body;
      serializeJson(doc, body);
      server.send(502, "application/json", body);
      return;
    }
    readPairingToken = token;
    readPairingTokenExpiresMs = millis() + WRITE_PAIRING_CODE_TTL_MS;
    JsonDocument doc;
    doc["ok"] = true;
    doc["device_id"] = deviceId;
    doc["gateway_id"] = gatewayId;
    doc["target_mac"] = targetMac;
    doc["read_pairing_token_active"] = true;
    doc["read_pairing_token"] = readPairingToken;
    doc["read_pairing_token_expires_in_seconds"] = readPairingTokenExpiresInSeconds();
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

  server.on("/api/select_candidate", HTTP_POST, []() {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }
    String nextDeviceId = doc["device_id"] | "";
    String nextTargetMac = doc["mac"] | "";
    int nextAddressType = doc["address_type"] | 0;
    nextDeviceId.trim();
    nextTargetMac = normalizeMac(nextTargetMac);
    if (nextDeviceId.length() == 0 || nextTargetMac.length() == 0) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"device_id and mac are required\"}");
      return;
    }
    bool found = false;
    for (const BleCandidate& candidate : bleCandidates) {
      if (candidate.name == nextDeviceId && candidate.mac == nextTargetMac) {
        found = true;
        break;
      }
    }
    if (!found) {
      server.send(404, "application/json", "{\"ok\":false,\"error\":\"candidate not found\"}");
      return;
    }
    deviceId = nextDeviceId;
    targetMac = nextTargetMac;
    targetAddressType = (esp_ble_addr_type_t)nextAddressType;
    targetAddressTypeKnown = true;
    pairingStatus = "paired";
    resetModbusSession("portal_candidate_selected");
    saveStringConfig("device_id", deviceId);
    saveStringConfig("target_mac", targetMac);
    postGatewayStatus("portal_candidate_selected");
    JsonDocument result;
    result["ok"] = true;
    result["device_id"] = deviceId;
    result["target_mac"] = targetMac;
    result["pairing_status"] = pairingStatus;
    String body;
    serializeJson(result, body);
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
    nextSsid.trim();

    if (nextSsid.length() > 0) {
      wifiSsid = nextSsid;
      saveStringConfig("wifi_ssid", wifiSsid);
    }
    if (nextPassword.length() > 0) {
      wifiPassword = nextPassword;
      saveStringConfig("wifi_pass", wifiPassword);
    }
    productionEnabled = true;
    prefs.putBool("prod", productionEnabled);

    JsonDocument result;
    result["ok"] = true;
    result["rebooting"] = true;
    result["local_hostname"] = localHostname;
    result["local_url"] = localPortalUrl();
    String body;
    serializeJson(result, body);
    server.send(200, "application/json", body);
    delay(500);
    ESP.restart();
  });

  server.on("/api/configure", HTTP_POST, []() {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
      return;
    }

    String ssidValue;
    String passwordValue;
    String deviceIdValue;
    String targetMacValue;
    String apiUrlValue;
    String apiTokenValue;
    String gatewayIdValue;
    bool productionValue = productionEnabled;

    const String* nextSsid = nullptr;
    const String* nextPassword = nullptr;
    const String* nextDeviceId = nullptr;
    const String* nextTargetMac = nullptr;
    const String* nextApiUrl = nullptr;
    const String* nextApiToken = nullptr;
    const String* nextGatewayId = nullptr;
    const bool* nextProductionEnabled = nullptr;

    if (doc["ssid"].is<JsonVariant>()) {
      ssidValue = doc["ssid"] | "";
      nextSsid = &ssidValue;
    }
    if (doc["password"].is<JsonVariant>()) {
      passwordValue = doc["password"] | "";
      nextPassword = &passwordValue;
    }
    if (doc["device_id"].is<JsonVariant>()) {
      deviceIdValue = doc["device_id"] | "";
      nextDeviceId = &deviceIdValue;
    }
    if (doc["target_mac"].is<JsonVariant>()) {
      targetMacValue = doc["target_mac"] | "";
      nextTargetMac = &targetMacValue;
    }
    if (doc["api_url"].is<JsonVariant>()) {
      apiUrlValue = doc["api_url"] | "";
      nextApiUrl = &apiUrlValue;
    }
    if (doc["api_token"].is<JsonVariant>()) {
      apiTokenValue = doc["api_token"] | "";
      nextApiToken = &apiTokenValue;
    }
    if (doc["gateway_id"].is<JsonVariant>()) {
      gatewayIdValue = doc["gateway_id"] | "";
      nextGatewayId = &gatewayIdValue;
    }
    if (doc["production_enabled"].is<bool>()) {
      productionValue = doc["production_enabled"].as<bool>();
      nextProductionEnabled = &productionValue;
    }

    bool restartWifi = doc["restart_wifi"] | true;
    bool reboot = doc["reboot"] | false;
    String applyError;
    if (!applyLanConfig(
          nextSsid,
          nextPassword,
          nextDeviceId,
          nextTargetMac,
          nextApiUrl,
          nextApiToken,
          nextGatewayId,
          nextProductionEnabled,
          restartWifi,
          applyError)) {
      JsonDocument result;
      result["ok"] = false;
      result["error"] = applyError;
      String body;
      serializeJson(result, body);
      server.send(400, "application/json", body);
      return;
    }

    JsonDocument result;
    result["ok"] = true;
    result["rebooting"] = reboot;
    result["wifi_restarted"] = restartWifi && (nextSsid != nullptr || nextPassword != nullptr);
    result["wifi_ssid"] = wifiSsid;
    result["device_id"] = deviceId;
    result["target_mac"] = targetMac;
    result["gateway_id"] = gatewayId;
    result["production_enabled"] = productionEnabled;
    result["local_url"] = localPortalUrl();
    String body;
    serializeJson(result, body);
    server.send(200, "application/json", body);
    if (reboot) {
      delay(500);
      ESP.restart();
    }
  });

  server.on("/api/ble_connection", HTTP_POST, []() {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    if (error || !doc["enabled"].is<bool>()) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"enabled boolean is required\"}");
      return;
    }
    bool enabled = doc["enabled"].as<bool>();
    setBleConnectionEnabled(enabled, enabled ? "ble_enabled_via_lan" : "ble_disabled_via_lan");
    JsonDocument result;
    result["ok"] = true;
    result["ble_connection_enabled"] = bleConnectionEnabled;
    result["ble_session_connected"] = modbusClient != nullptr && modbusClient->isConnected() && modbusCharacteristic != nullptr;
    result["device_id"] = deviceId;
    result["target_mac"] = targetMac;
    String body;
    serializeJson(result, body);
    server.send(200, "application/json", body);
  });

  server.onNotFound(sendPortalPage);
  server.begin();
  portalServerStarted = true;
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
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_PORTAL);
  if (provisioningPortalActive) {
    dnsServer.processNextRequest();
  }
  if (portalServerStarted && (provisioningPortalActive || WiFi.status() == WL_CONNECTED)) {
    server.handleClient();
  }
  finishRuntimeProbe(PROBE_PORTAL, probeStartedMs, true);
}

static void stopProvisioningPortal() {
  if (!provisioningPortalActive) return;
  dnsServer.stop();
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
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_WIFI_CONNECT);
  if (WiFi.status() == WL_CONNECTED) {
    finishRuntimeProbe(PROBE_WIFI_CONNECT, probeStartedMs, true);
    return true;
  }
  if (wifiSsid.length() == 0) {
    finishRuntimeProbe(PROBE_WIFI_CONNECT, probeStartedMs, false);
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
  WiFi.setHostname(localHostname.c_str());
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
    finishRuntimeProbe(PROBE_WIFI_CONNECT, probeStartedMs, false);
    return false;
  }

  JsonDocument done;
  done["type"] = "wifi_connected";
  done["uptime_ms"] = millis() - bootMs;
  done["ssid"] = wifiSsid;
  done["ip"] = WiFi.localIP().toString();
  done["local_hostname"] = localHostname;
  done["local_url"] = localPortalUrl();
  done["rssi"] = WiFi.RSSI();
  printJson(done);
  ensureMdnsStarted();
  stopProvisioningPortal();
  finishRuntimeProbe(PROBE_WIFI_CONNECT, probeStartedMs, true);
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
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_TELEMETRY_UPLOAD);
  if (apiUrl.length() == 0) {
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, false);
    emitError("api_url_not_configured", "set API URL with SET_API_URL");
    return false;
  }
  if (apiToken.length() == 0) {
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, false);
    emitError("api_token_not_configured", "set API token with SET_API_TOKEN");
    return false;
  }
  if (deviceId.length() == 0) {
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, false);
    emitError("device_id_not_configured", "set device id with SET_DEVICE_ID");
    return false;
  }
  if (!ensureWifiConnected()) {
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, false);
    return false;
  }

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

  if (!lockHttpOperation(HTTP_BUSY_SKIP_TIMEOUT_MS)) {
    setRuntimeProbeHttpStatus(PROBE_TELEMETRY_UPLOAD, -2);
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, true);
    return false;
  }
  unsigned long httpNow = millis();
  if (!httpsGapReady(httpNow)) {
    setRuntimeProbeHttpStatus(PROBE_TELEMETRY_UPLOAD, -3);
    unlockHttpOperation();
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, true);
    return false;
  }
  markHttpsAttempt(httpNow);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    emitHttpClientDiag("telemetry_upload", "pre_begin", endpoint, 0, body.length());
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) {
    setRuntimeProbeHttpStatus(PROBE_TELEMETRY_UPLOAD, -1);
    emitHttpClientDiag("telemetry_upload", "begin_failed", endpoint, -1, body.length());
    unlockHttpOperation();
    finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, false);
    emitError("http_begin_failed", "could not initialize HTTP client");
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int status = http.POST((uint8_t*)body.c_str(), body.length());
  setRuntimeProbeHttpStatus(PROBE_TELEMETRY_UPLOAD, status);
  if (status < 0) {
    emitHttpClientDiag("telemetry_upload", "request_failed", endpoint, status, body.length());
  }
  String response = http.getString();
  http.end();
  unlockHttpOperation();

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
    if (
      httpStartupQuietElapsed(now)
      && (lastGatewayStatusPostMs == 0 || now - lastGatewayStatusPostMs >= HEARTBEAT_INTERVAL_MS)
    ) {
      postGatewayStatus("telemetry_upload_ok");
    }
  }
  finishRuntimeProbe(PROBE_TELEMETRY_UPLOAD, probeStartedMs, status >= 200 && status < 300);
  return status >= 200 && status < 300;
}

static bool uploadSettingsSnapshotNow(const char* reason) {
  ModbusReadResult settings = runModbusRead(95, 95, "settings_registers_95_189");
  if (!settings.ok) {
    emitError("settings_read_empty", "no settings Modbus response payload to upload");
    return false;
  }

  bool ok = postTelemetry(
    settings.payloadHex,
    settings.notifyCount,
    settings.length,
    95,
    95,
    "settings_registers_95_189",
    "function_03_read_only_no_setting_write"
  );
  if (ok) {
    lastSettingsUploadMs = millis();
  }
  updateTelemetrySnapshot(
    latestSettingsTelemetry,
    settings,
    95,
    95,
    "settings_registers_95_189",
    "function_03_read_only_no_setting_write"
  );
  if (ok) {
    latestSettingsTelemetry.dirty = false;
  }

  JsonDocument log;
  log["type"] = ok ? "settings_upload_forced_ok" : "settings_upload_forced_failed";
  log["uptime_ms"] = millis() - bootMs;
  log["reason"] = reason != nullptr ? reason : "";
  log["safety"] = "post_write_settings_sync_only";
  printJson(log);
  return ok;
}

static bool postGatewayStatus(const char* reason) {
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0) return false;
  if (!ensureWifiConnected()) return false;
  unsigned long now = millis();
  if (!httpStartupQuietElapsed(now)) return false;

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

  if (!lockHttpOperation(10000)) return false;
  unsigned long httpNow = millis();
  if (!httpsGapReady(httpNow)) {
    unlockHttpOperation();
    return false;
  }
  markHttpsAttempt(httpNow);

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    emitHttpClientDiag("gateway_status", "pre_begin", endpoint, 0, body.length());
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) {
    emitHttpClientDiag("gateway_status", "begin_failed", endpoint, -1, body.length());
    unlockHttpOperation();
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int status = http.POST((uint8_t*)body.c_str(), body.length());
  if (status < 0) {
    emitHttpClientDiag("gateway_status", "request_failed", endpoint, status, body.length());
  }
  String response = http.getString();
  http.end();
  unlockHttpOperation();

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

static bool postGatewayCandidates() {
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0) return false;
  if (!ensureWifiConnected()) return false;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/gateways/";
  endpoint += gatewayId;
  endpoint += "/candidates";

  JsonDocument doc;
  JsonArray candidates = doc["candidates"].to<JsonArray>();
  for (const BleCandidate& candidate : bleCandidates) {
    JsonObject item = candidates.add<JsonObject>();
    item["device_id"] = candidate.name.length() > 0 ? candidate.name : deviceId;
    item["mac"] = candidate.mac;
    item["address_type"] = candidate.addressType;
    item["name"] = candidate.name;
    item["rssi"] = candidate.rssi;
    item["service_uuid_match"] = candidate.serviceUuidMatch;
    item["characteristic_uuid_match"] = candidate.vendorGattMatch;
    item["score"] = candidate.score;
  }

  String body;
  serializeJson(doc, body);

  if (!lockHttpOperation(10000)) return false;

  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  HTTPClient http;
  bool beginOk = false;
  if (endpoint.startsWith("https://")) {
    emitHttpClientDiag("command_poll", "pre_begin", endpoint, 0, 0);
    if (tlsInsecure) {
      secureClient.setInsecure();
    }
    beginOk = http.begin(secureClient, endpoint);
  } else {
    beginOk = http.begin(plainClient, endpoint);
  }
  if (!beginOk) {
    unlockHttpOperation();
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int status = http.POST((uint8_t*)body.c_str(), body.length());
  String response = http.getString();
  http.end();
  unlockHttpOperation();

  JsonDocument result;
  result["type"] = status >= 200 && status < 300 ? "gateway_candidates_upload_ok" : "gateway_candidates_upload_failed";
  result["uptime_ms"] = millis() - bootMs;
  result["http_status"] = status;
  result["candidate_count"] = bleCandidates.size();
  result["response"] = response.substring(0, 300);
  printJson(result);
  return status >= 200 && status < 300;
}

static bool postReadPairingToken(const String& token, String& response) {
  response = "";
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0 || deviceId.length() == 0 || targetMac.length() == 0) return false;
  if (!ensureWifiConnected()) return false;

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/gateways/";
  endpoint += gatewayId;
  endpoint += "/read-pairing-token";

  JsonDocument doc;
  doc["gateway_id"] = gatewayId;
  doc["device_id"] = deviceId;
  doc["mac"] = targetMac;
  doc["token"] = token;
  doc["firmware"] = String(FW_NAME) + "/" + FW_VERSION;
  doc["uptime_ms"] = millis() - bootMs;
  doc["pairing_status"] = pairingStatus;

  String body;
  serializeJson(doc, body);

  if (!lockHttpOperation(10000)) return false;

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
    unlockHttpOperation();
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.POST((uint8_t*)body.c_str(), body.length());
  response = http.getString();
  http.end();
  unlockHttpOperation();

  JsonDocument log;
  log["type"] = httpStatus >= 200 && httpStatus < 300 ? "read_pairing_token_registered" : "read_pairing_token_failed";
  log["uptime_ms"] = millis() - bootMs;
  log["http_status"] = httpStatus;
  log["response"] = response.substring(0, 300);
  printJson(log);
  return httpStatus >= 200 && httpStatus < 300;
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

  if (!lockHttpOperation(10000)) return false;

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
    unlockHttpOperation();
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.POST((uint8_t*)body.c_str(), body.length());
  response = http.getString();
  http.end();
  unlockHttpOperation();

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

  if (!lockHttpOperation(10000)) return false;

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
    unlockHttpOperation();
    return false;
  }

  http.setTimeout(8000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.POST((uint8_t*)body.c_str(), body.length());
  String response = http.getString();
  http.end();
  unlockHttpOperation();

  JsonDocument log;
  log["type"] = httpStatus >= 200 && httpStatus < 300 ? "command_result_upload_ok" : "command_result_upload_failed";
  log["uptime_ms"] = millis() - bootMs;
  log["command_id"] = commandId;
  log["http_status"] = httpStatus;
  log["command_status"] = status;
  log["response"] = response.substring(0, 300);
  log["safety"] = "command_result_upload_only";
  printJson(log);
  bool ok = httpStatus >= 200 && httpStatus < 300;
  if (ok) {
    if (pendingCommandResultId == commandId) {
      clearPendingCommandResult();
    }
    return true;
  }
  persistPendingCommandResult(commandId, status, resultDoc, error);
  return false;
}

static void persistPendingCommandResult(uint64_t commandId, const char* status, JsonDocument& resultDoc, const char* error) {
  String resultJson;
  serializeJson(resultDoc, resultJson);
  pendingCommandResultId = commandId;
  pendingCommandResultStatus = status != nullptr ? String(status) : "";
  pendingCommandResultError = error != nullptr ? String(error) : "";
  pendingCommandResultPayload = resultJson;
  prefs.putULong64("pend_cmd_id", pendingCommandResultId);
  prefs.putString("pend_cmd_st", pendingCommandResultStatus);
  prefs.putString("pend_cmd_er", pendingCommandResultError);
  prefs.putString("pend_cmd_js", pendingCommandResultPayload);
}

static void clearPendingCommandResult() {
  pendingCommandResultId = 0;
  pendingCommandResultStatus = "";
  pendingCommandResultError = "";
  pendingCommandResultPayload = "";
  prefs.remove("pend_cmd_id");
  prefs.remove("pend_cmd_st");
  prefs.remove("pend_cmd_er");
  prefs.remove("pend_cmd_js");
}

static bool retryPendingCommandResult() {
  if (pendingCommandResultId == 0 || pendingCommandResultStatus.length() == 0 || pendingCommandResultPayload.length() == 0) {
    return false;
  }
  JsonDocument resultDoc;
  DeserializationError err = deserializeJson(resultDoc, pendingCommandResultPayload);
  if (err) {
    clearPendingCommandResult();
    return false;
  }
  String errorCopy = pendingCommandResultError;
  const char* errorPtr = errorCopy.length() > 0 ? errorCopy.c_str() : nullptr;
  return postCommandResult(pendingCommandResultId, pendingCommandResultStatus.c_str(), resultDoc, errorPtr);
}

static void maybeAccelerateAfterWriteCommand(const char* status, JsonDocument& result) {
  if (status == nullptr) {
    return;
  }
  telemetryResumeAfterWriteMs = millis() + WRITE_LANE_RESUME_DELAY_MS;
  if (strcmp(status, "completed") == 0) {
    forceMainFullRefresh = true;
    uploadSettingsSnapshotNow("post_write_command_completed");
    nextSettingsPollMs = millis() + SETTINGS_UPLOAD_INTERVAL_MS;
  }
  lastCommandPollMs = 0;
  nextUploadMs = telemetryResumeAfterWriteMs + telemetryIntervalMs();

  JsonDocument log;
  log["type"] = "write_lane_accelerated";
  log["uptime_ms"] = millis() - bootMs;
  log["command_status"] = status;
  log["requested_value"] = result["requested_value"];
  log["safety"] = "prioritize_write_lane_over_periodic_read_loop";
  printJson(log);
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
  ModbusReadResult writeResult;
  runModbusRequest(
    commandHex,
    label,
    "ble_modbus_write_multiple_registers_sent",
    "semantic_function_16_single_register_write",
    "function_16_semantic_single_register_write_done",
    writeResult
  );

  String responseHex = writeResult.ok ? writeResult.payloadHex : "";
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
  if (enabled == 1 && !validateScheduleEnableChange("discharge", slot, true, errorMessage)) {
    result["verified"] = false;
    result["would_execute"] = false;
    result["write_enabled"] = false;
    result["ble_write"] = false;
    result["modbus_write"] = false;
    result["safety"] = "schedule_conflict_rejected_before_ble_write";
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
  if (enabled == 1 && !validateScheduleEnableChange("mains_charge", slot, true, errorMessage)) {
    result["verified"] = false;
    result["would_execute"] = false;
    result["write_enabled"] = false;
    result["ble_write"] = false;
    result["modbus_write"] = false;
    result["safety"] = "schedule_conflict_rejected_before_ble_write";
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
  if (!validateScheduleTimeChange("mains_charge", slot, isStart, requestedTime, errorMessage)) {
    result["verified"] = false;
    result["would_execute"] = false;
    result["write_enabled"] = false;
    result["ble_write"] = false;
    result["modbus_write"] = false;
    result["safety"] = errorMessage.startsWith("turn off ")
      ? "time_window_edit_requires_slot_off"
      : "schedule_conflict_rejected_before_ble_write";
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
  if (!validateScheduleTimeChange("discharge", slot, isStart, requestedTime, errorMessage)) {
    result["verified"] = false;
    result["would_execute"] = false;
    result["write_enabled"] = false;
    result["ble_write"] = false;
    result["modbus_write"] = false;
    result["safety"] = errorMessage.startsWith("turn off ")
      ? "time_window_edit_requires_slot_off"
      : "schedule_conflict_rejected_before_ble_write";
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
  bool isWriteCommand = strcmp(mode, "write") == 0;
  if (isWriteCommand) {
    writeLaneActive = true;
    telemetryResumeAfterWriteMs = 0;
  }

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
    if (isWriteCommand) {
      writeLaneActive = false;
    }
    return;
  }

  if (strcmp(mode, "dry_run") == 0 && strcmp(commandName, "dry_run_noop") == 0) {
    result["dry_run"] = true;
    postCommandResult(commandId, "dry_run_completed", result, nullptr);
    if (isWriteCommand) {
      writeLaneActive = false;
    }
    return;
  }

  auto postWriteResult = [&](bool ok, String& errorMessage) {
    const char* status = commandResultStatusForOutcome(ok, result);
    postCommandResult(commandId, status, result, ok ? nullptr : errorMessage.c_str());
    maybeAccelerateAfterWriteCommand(status, result);
    writeLaneActive = false;
  };

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_first_discharge_target_soc") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int targetSoc = payload["target_soc"] | -1;
    String errorMessage;
    bool ok = runSetFirstDischargeTargetSoc((uint16_t)targetSoc, result, errorMessage);
    postWriteResult(ok, errorMessage);
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_discharge_target_soc") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int targetSoc = payload["target_soc"] | -1;
    String errorMessage;
    bool ok = runSetDischargeTargetSoc(slot, (uint16_t)targetSoc, result, errorMessage);
    postWriteResult(ok, errorMessage);
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_discharge_power") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int power = payload["power"] | -1;
    String errorMessage;
    bool ok = runSetDischargePower(slot, (uint16_t)power, result, errorMessage);
    postWriteResult(ok, errorMessage);
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_discharge_time_enable") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int enabled = payload["enabled"] | -1;
    String errorMessage;
    bool ok = runSetDischargeTimeEnable(slot, (uint16_t)enabled, result, errorMessage);
    postWriteResult(ok, errorMessage);
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
    postWriteResult(ok, errorMessage);
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_mains_charge_target_soc") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int targetSoc = payload["target_soc"] | -1;
    String errorMessage;
    bool ok = runSetMainsChargeTargetSoc(slot, (uint16_t)targetSoc, result, errorMessage);
    postWriteResult(ok, errorMessage);
    return;
  }

  if (strcmp(mode, "write") == 0 && strcmp(commandName, "set_mains_charge_time_enable") == 0) {
    JsonObject payload = command["payload"].as<JsonObject>();
    int slot = payload["slot"] | -1;
    int enabled = payload["enabled"] | -1;
    String errorMessage;
    bool ok = runSetMainsChargeTimeEnable(slot, (uint16_t)enabled, result, errorMessage);
    postWriteResult(ok, errorMessage);
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
    postWriteResult(ok, errorMessage);
    return;
  }

  postCommandResult(commandId, "rejected", result, "unsupported command");
  if (isWriteCommand) {
    writeLaneActive = false;
  }
}

static const char* commandResultStatusForOutcome(bool ok, JsonDocument& result) {
  if (ok) {
    return "completed";
  }
  String safety = result["safety"] | "";
  if (safety == "schedule_conflict_rejected_before_ble_write" || safety == "time_window_edit_requires_slot_off") {
    return "rejected";
  }
  return "failed";
}

static void pollPendingCommand() {
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_COMMAND_POLL);
  if (commandPollingDisabled) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  if (!productionEnabled) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  if (apiUrl.length() == 0 || apiToken.length() == 0 || gatewayId.length() == 0 || deviceId.length() == 0) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  if (pendingCommandResultId != 0) {
    retryPendingCommandResult();
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  unsigned long now = millis();
  if (!httpStartupQuietElapsed(now)) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  if (lastCommandPollMs != 0 && now - lastCommandPollMs < COMMAND_POLL_INTERVAL_MS) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  lastCommandPollMs = now;
  if (!ensureWifiConnected()) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, false);
    return;
  }

  String endpoint = apiUrl;
  endpoint.trim();
  while (endpoint.endsWith("/")) endpoint.remove(endpoint.length() - 1);
  endpoint += "/api/lumentree/gateways/";
  endpoint += gatewayId;
  endpoint += "/commands/next?device_id=";
  endpoint += deviceId;

  if (!lockHttpOperation(HTTP_BUSY_SKIP_TIMEOUT_MS)) {
    setRuntimeProbeHttpStatus(PROBE_COMMAND_POLL, -2);
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  unsigned long httpNow = millis();
  if (!httpsGapReady(httpNow)) {
    setRuntimeProbeHttpStatus(PROBE_COMMAND_POLL, -3);
    unlockHttpOperation();
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  markHttpsAttempt(httpNow);

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
    setRuntimeProbeHttpStatus(PROBE_COMMAND_POLL, -1);
    emitHttpClientDiag("command_poll", "begin_failed", endpoint, -1, 0);
    unlockHttpOperation();
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, false);
    return;
  }

  http.setTimeout(8000);
  http.addHeader("Authorization", "Bearer " + apiToken);
  http.addHeader("User-Agent", String(FW_NAME) + "/" + FW_VERSION);

  int httpStatus = http.GET();
  setRuntimeProbeHttpStatus(PROBE_COMMAND_POLL, httpStatus);
  if (httpStatus < 0) {
    emitHttpClientDiag("command_poll", "request_failed", endpoint, httpStatus, 0);
  }
  String response = http.getString();
  http.end();
  unlockHttpOperation();

  if (httpStatus < 200 || httpStatus >= 300) {
    JsonDocument log;
    log["type"] = "command_poll_failed";
    log["uptime_ms"] = millis() - bootMs;
    log["http_status"] = httpStatus;
    log["response"] = response.substring(0, 300);
    printJson(log);
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, false);
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, false);
    emitError("command_poll_invalid_json", "command poll returned invalid JSON");
    return;
  }

  JsonVariant commandValue = doc["command"];
  if (commandValue.isNull()) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
    return;
  }
  if (!commandValue.is<JsonObject>()) {
    finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, false);
    emitError("command_poll_invalid_command", "command payload is invalid");
    return;
  }

  executeCommand(commandValue.as<JsonObject>());
  finishRuntimeProbe(PROBE_COMMAND_POLL, probeStartedMs, true);
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

static bool advertisedNameLooksLikeDeviceId(const String& name) {
  if (name.length() < 5) return false;
  if (!(name[0] == 'P' || name[0] == 'p')) return false;
  for (size_t i = 1; i < name.length(); i++) {
    if (!isDigit(name[i])) return false;
  }
  return true;
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
  bool nameMatch = deviceId.length() > 0 ? advertisedNameMatchesDeviceId(name) : advertisedNameLooksLikeDeviceId(name);
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
    postGatewayCandidates();
    postGatewayStatus("ble_discovery_no_candidate");
    return false;
  }

  if (allowAutoBind && bleCandidates.size() == 1) {
    const BleCandidate& candidate = bleCandidates[0];
    if (deviceId.length() == 0 && candidate.name.length() > 0) {
      deviceId = candidate.name;
      saveStringConfig("device_id", deviceId);
    }
    resetModbusSession("auto_pair_target_selected");
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
    postGatewayCandidates();
    postGatewayStatus("ble_auto_paired");
    return true;
  }

  pairingStatus = "multiple_candidates";
  emitBleCandidates("ble_candidates");
  postGatewayCandidates();
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
  String commandHex = buildReadCommand(1, startRegister, registerCount);

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
  runModbusRequest(
    commandHex,
    label,
    "ble_modbus_read_request_sent",
    "function_03_read_only_no_setting_write",
    "reassembled_function_03_read_response_only",
    result
  );

  JsonDocument done;
  done["type"] = "ble_modbus_read_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["label"] = label;
  done["notify_count"] = modbusNotifyCount;
  done["safety"] = "function_03_read_only_no_setting_write";
  printJson(done);
  return result;
}

static ModbusReadResult runModbusReadInput(uint16_t startRegister, uint16_t registerCount, const char* label) {
  ModbusReadResult result;
  String commandHex = buildReadInputCommand(1, startRegister, registerCount);

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
  runModbusRequest(
    commandHex,
    label,
    "ble_modbus_read_request_sent",
    "function_04_read_only_input_registers",
    "reassembled_function_04_read_response_only",
    result
  );

  JsonDocument done;
  done["type"] = "ble_modbus_read_done";
  done["uptime_ms"] = millis() - bootMs;
  done["target_mac"] = targetMac;
  done["label"] = label;
  done["notify_count"] = modbusNotifyCount;
  done["safety"] = "function_04_read_only_input_registers";
  printJson(done);
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

static uint16_t scheduleEnableRegister(const char* group, int slot) {
  if (strcmp(group, "mains_charge") == 0) {
    return mainsChargeTimeEnableRegisterForSlot(slot);
  }
  return dischargeTimeEnableRegisterForSlot(slot);
}

static uint16_t scheduleStartRegister(const char* group, int slot) {
  if (strcmp(group, "mains_charge") == 0) {
    return mainsChargeTimeStartRegisterForSlot(slot);
  }
  return dischargeTimeStartRegisterForSlot(slot);
}

static uint16_t scheduleEndRegister(const char* group, int slot) {
  if (strcmp(group, "mains_charge") == 0) {
    return mainsChargeTimeEndRegisterForSlot(slot);
  }
  return dischargeTimeEndRegisterForSlot(slot);
}

static int hhmmToMinutes(uint16_t value) {
  return ((int)(value / 100) * 60) + (int)(value % 100);
}

static void expandWindow(uint16_t startHhmm, uint16_t endHhmm, int& startMinutes, int& endMinutes) {
  startMinutes = hhmmToMinutes(startHhmm);
  endMinutes = hhmmToMinutes(endHhmm);
  if (endMinutes <= startMinutes) {
    endMinutes += 1440;
  }
}

static bool scheduleWindowsOverlap(uint16_t leftStart, uint16_t leftEnd, uint16_t rightStart, uint16_t rightEnd) {
  int aStart = 0;
  int aEnd = 0;
  int bStart = 0;
  int bEnd = 0;
  expandWindow(leftStart, leftEnd, aStart, aEnd);
  expandWindow(rightStart, rightEnd, bStart, bEnd);
  const int candidateStarts[] = {bStart, bStart + 1440, bStart - 1440};
  const int candidateEnds[] = {bEnd, bEnd + 1440, bEnd - 1440};
  for (size_t index = 0; index < 3; index++) {
    if (candidateStarts[index] < aEnd && aStart < candidateEnds[index]) {
      return true;
    }
  }
  return false;
}

static bool loadScheduleState(ScheduleState& state, String& errorMessage) {
  ModbusReadResult read = runModbusRead(SCHEDULE_STATE_START_REGISTER, SCHEDULE_STATE_REGISTER_COUNT, "schedule_safety_snapshot");
  if (!read.ok) {
    errorMessage = "could not read schedule registers for safety validation";
    return false;
  }
  for (int slot = 1; slot <= 2; slot++) {
    uint16_t enabledValue = 0;
    uint16_t startValue = 0;
    uint16_t endValue = 0;
    if (!readRegisterFromResult(read, SCHEDULE_STATE_START_REGISTER, mainsChargeTimeEnableRegisterForSlot(slot), enabledValue)
      || !readRegisterFromResult(read, SCHEDULE_STATE_START_REGISTER, mainsChargeTimeStartRegisterForSlot(slot), startValue)
      || !readRegisterFromResult(read, SCHEDULE_STATE_START_REGISTER, mainsChargeTimeEndRegisterForSlot(slot), endValue)) {
      errorMessage = "could not decode mains charge schedule registers for safety validation";
      return false;
    }
    state.mainsCharge[slot - 1].enabled = enabledValue == 1;
    state.mainsCharge[slot - 1].start = startValue;
    state.mainsCharge[slot - 1].end = endValue;
  }
  for (int slot = 1; slot <= 4; slot++) {
    uint16_t enabledValue = 0;
    uint16_t startValue = 0;
    uint16_t endValue = 0;
    if (!readRegisterFromResult(read, SCHEDULE_STATE_START_REGISTER, dischargeTimeEnableRegisterForSlot(slot), enabledValue)
      || !readRegisterFromResult(read, SCHEDULE_STATE_START_REGISTER, dischargeTimeStartRegisterForSlot(slot), startValue)
      || !readRegisterFromResult(read, SCHEDULE_STATE_START_REGISTER, dischargeTimeEndRegisterForSlot(slot), endValue)) {
      errorMessage = "could not decode discharge schedule registers for safety validation";
      return false;
    }
    state.discharge[slot - 1].enabled = enabledValue == 1;
    state.discharge[slot - 1].start = startValue;
    state.discharge[slot - 1].end = endValue;
  }
  return true;
}

static bool validateScheduleConflicts(const ScheduleState& state, String& errorMessage) {
  for (int chargeSlot = 1; chargeSlot <= 2; chargeSlot++) {
    const ScheduleSlotState& charge = state.mainsCharge[chargeSlot - 1];
    if (!charge.enabled) continue;
    for (int dischargeSlot = 1; dischargeSlot <= 4; dischargeSlot++) {
      const ScheduleSlotState& discharge = state.discharge[dischargeSlot - 1];
      if (!discharge.enabled) continue;
      if (scheduleWindowsOverlap(charge.start, charge.end, discharge.start, discharge.end)) {
        errorMessage = "mains charge slot " + String(chargeSlot)
          + " overlaps discharge slot " + String(dischargeSlot);
        return false;
      }
    }
  }
  return true;
}

static bool validateScheduleEnableChange(const char* group, int slot, bool enabled, String& errorMessage) {
  if (!enabled) return true;
  ScheduleState state;
  if (!loadScheduleState(state, errorMessage)) return false;
  if (strcmp(group, "mains_charge") == 0) {
    state.mainsCharge[slot - 1].enabled = true;
  } else {
    state.discharge[slot - 1].enabled = true;
  }
  if (!validateScheduleConflicts(state, errorMessage)) {
    errorMessage = "schedule conflict rejected before BLE write: " + errorMessage;
    return false;
  }
  return true;
}

static bool validateScheduleTimeChange(const char* group, int slot, bool isStart, uint16_t requestedTime, String& errorMessage) {
  ScheduleState state;
  if (!loadScheduleState(state, errorMessage)) return false;
  ScheduleSlotState* target = strcmp(group, "mains_charge") == 0
    ? &state.mainsCharge[slot - 1]
    : &state.discharge[slot - 1];
  if (target->enabled) {
    errorMessage = String("turn off ") + (strcmp(group, "mains_charge") == 0 ? "mains charge" : "discharge")
      + " slot " + String(slot) + " before changing its time window";
    return false;
  }
  if (isStart) {
    target->start = requestedTime;
  } else {
    target->end = requestedTime;
  }
  if (!validateScheduleConflicts(state, errorMessage)) {
    errorMessage = "schedule conflict rejected before BLE write: " + errorMessage;
    return false;
  }
  return true;
}

static bool runModbusWriteSingleRegister(uint16_t registerAddress, uint16_t value, const char* label) {
  if (!lockModbusOperation(10000)) {
    emitError("modbus_busy", "BLE Modbus lane is busy");
    return false;
  }
  if (targetMac.length() == 0) {
    unlockModbusOperation();
    emitError("target_not_set", "run SET_TARGET before writing BLE Modbus data");
    return false;
  }
  if (!targetAddressTypeKnown) {
    unlockModbusOperation();
    emitError("address_type_unknown", "scan the target once before writing BLE Modbus data");
    return false;
  }

  String commandHex = buildWriteSingleRegisterCommand(1, registerAddress, value);
  std::string commandBytes;
  if (!hexToBytes(commandHex, commandBytes)) {
    unlockModbusOperation();
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
    unlockModbusOperation();
    emitError("gatt_connect_failed", "could not connect to target for BLE Modbus write");
    return false;
  }

  BLERemoteService* service = modbusClient->getService(BLEUUID(LUMENTREE_VENDOR_SERVICE_UUID));
  if (service == nullptr) {
    emitError("gatt_service_missing", "FFE0 service not found");
    modbusClient->disconnect();
    unlockModbusOperation();
    return false;
  }

  BLERemoteCharacteristic* chr = service->getCharacteristic(BLEUUID(LUMENTREE_VENDOR_CHARACTERISTIC_UUID));
  if (chr == nullptr) {
    emitError("gatt_characteristic_missing", "FFE1 characteristic not found");
    modbusClient->disconnect();
    unlockModbusOperation();
    return false;
  }
  if (!chr->canWrite() && !chr->canWriteNoResponse()) {
    emitError("gatt_write_missing", "FFE1 cannot carry Modbus write request");
    modbusClient->disconnect();
    unlockModbusOperation();
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
    if (!telemetryTaskEnabled) {
      handleProvisioningPortal();
    }
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
  unlockModbusOperation();
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
  printLine("# Commands:");
  printLine("# HELP");
  printLine("# STATUS");
  printLine("# CONFIG");
  printLine("# SET_TARGET aa:bb:cc:dd:ee:ff");
  printLine("# SET_TARGET_MAC aa:bb:cc:dd:ee:ff");
  printLine("# CLEAR_TARGET");
  printLine("# CLEAR_TARGET_MAC");
  printLine("# SET_ACTIVE 0|1");
  printLine("# SET_CANDIDATE_ONLY 0|1");
  printLine("# SET_SCAN seconds interval window [log_limit]");
  printLine("# SET_WIFI ssid password");
  printLine("# SET_API_URL https://lumentree.jonah.io.vn");
  printLine("# SET_API_TOKEN token");
  printLine("# SET_DEVICE_ID P240819130");
  printLine("# SET_GATEWAY_ID esp32-lumentree");
  printLine("# SET_UPLOAD_INTERVAL seconds");
  printLine("# SET_TLS_INSECURE 0|1");
  printLine("# SET_PRODUCTION 0|1");
  printLine("# SET_BLE_CONNECTION 0|1");
  printLine("# START_AP");
  printLine("# WRITE_STATUS");
  printLine("# GENERATE_WRITE_CODE");
  printLine("# SCAN_WIFI");
  printLine("# SCAN_BLE");
  printLine("# BLE_CANDIDATES");
  printLine("# SCAN_ONCE");
  printLine("# DISCOVER_GATT");
  printLine("# READ_MAIN_ONCE");
  printLine("# READ_STATS_ONCE");
  printLine("# READ_RANGE start_register register_count");
  printLine("# WRITE_TARGET_SOC_144_TO_6 CONFIRM");
  printLine("# UPLOAD_ONCE");
  printLine("# READ_CELLS_ONCE");
  printLine("# Lumentree candidate hints: BLE name contains Device ID, uuid=A018739B-734D-8211-CB80-C9ACD39D13B4, or GATT FFE0/FFE1");
  printLine("# Safety: READ_MAIN_ONCE/READ_RANGE/READ_CELLS_ONCE use Modbus function 03; READ_STATS_ONCE uses function 04.");
  printLine("# WRITE_TARGET_SOC_144_TO_6 is a one-off whitelisted function 06 test with pre/post read verification.");
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

static void maybeRunTelemetryScheduler() {
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_TELEMETRY_SCHEDULER);
  if (!productionEnabled) {
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
    return;
  }
  if (backgroundTelemetryDisabled) {
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
    return;
  }
  if (pendingCommandResultId != 0) {
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
    return;
  }
  if (writeLaneActive) {
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
    return;
  }
  unsigned long now = millis();
  if (telemetryResumeAfterWriteMs != 0 && (long)(now - telemetryResumeAfterWriteMs) < 0) {
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
    return;
  }
  if (nextUploadMs == 0) {
    nextUploadMs = now + 2000;
  }
  if (nextSettingsPollMs == 0) {
    nextSettingsPollMs = now + SETTINGS_UPLOAD_INTERVAL_MS;
  }
  if (nextStatsPollMs == 0) {
    nextStatsPollMs = now + STATS_UPLOAD_INTERVAL_MS;
  }
  if (targetMac.length() == 0 && lastAutoDiscoveryMs != 0 && now - lastAutoDiscoveryMs < AUTO_DISCOVERY_RETRY_MS) {
    flushOneDirtyTelemetrySnapshot();
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
    return;
  }
  if (targetMac.length() == 0) {
    lastAutoDiscoveryMs = now;
    if (!runBleDiscovery(true)) {
      emitError("target_not_paired", "waiting for ESP32 gateway pairing");
      flushOneDirtyTelemetrySnapshot();
      finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, false);
      return;
    }
  }

  bool settingsDue = (long)(now - nextSettingsPollMs) >= 0;
  bool statsDue = (long)(now - nextStatsPollMs) >= 0;
  bool fastDue = (long)(now - nextUploadMs) >= 0;
  bool fullRefreshDue = forceMainFullRefresh || !mainTelemetryCache.valid;

  if (settingsDue && bleTelemetryGapReady(now)) {
    JsonDocument plan;
    plan["type"] = "telemetry_scheduler_plan";
    plan["uptime_ms"] = millis() - bootMs;
    plan["tier"] = "settings";
    plan["start_register"] = 95;
    plan["register_count"] = 95;
    plan["interval_ms"] = SETTINGS_UPLOAD_INTERVAL_MS;
    printJson(plan);
    ModbusReadResult settings = runModbusRead(95, 95, "settings_registers_95_189");
    if (settings.ok) {
      markBleTelemetryAction(millis());
      updateTelemetrySnapshot(
        latestSettingsTelemetry,
        settings,
        95,
        95,
        "settings_registers_95_189",
        "function_03_read_only_no_setting_write"
      );
      lastSettingsUploadMs = millis();
    } else {
      emitError("settings_read_empty", "no settings Modbus response payload to cache");
      markTelemetrySnapshotDirty(latestSettingsTelemetry);
    }
    nextSettingsPollMs = millis() + SETTINGS_UPLOAD_INTERVAL_MS;
    nextUploadMs = millis() + telemetryIntervalMs();
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, settings.ok);
    return;
  }

  if (statsDue && bleTelemetryGapReady(now)) {
    JsonDocument plan;
    plan["type"] = "telemetry_scheduler_plan";
    plan["uptime_ms"] = millis() - bootMs;
    plan["tier"] = "statistics";
    plan["start_register"] = 0;
    plan["register_count"] = 8;
    plan["interval_ms"] = STATS_UPLOAD_INTERVAL_MS;
    printJson(plan);
    ModbusReadResult stats = runModbusReadInput(0, 8, "today_statistics_0_7");
    if (stats.ok) {
      markBleTelemetryAction(millis());
      updateTelemetrySnapshot(
        latestStatsTelemetry,
        stats,
        0,
        8,
        "today_statistics_0_7",
        "function_04_read_only_input_registers"
      );
      lastStatsUploadMs = millis();
    } else {
      emitError("stats_read_empty", "no statistics Modbus response payload to cache");
      markTelemetrySnapshotDirty(latestStatsTelemetry);
    }
    nextStatsPollMs = millis() + STATS_UPLOAD_INTERVAL_MS;
    nextUploadMs = millis() + telemetryIntervalMs();
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, stats.ok);
    return;
  }

  if (LUMENTREE_USE_FAST_MAIN_CACHE != 0) {
    if ((fullRefreshDue || fastDue) && bleTelemetryGapReady(now)) {
      bool ok = runFastMainCacheScheduler(now);
      finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, ok);
      return;
    }
  }

  if (fastDue && bleTelemetryGapReady(now)) {
    uint16_t startRegister = nextMainTelemetryStartRegister;
    uint16_t registerCount = mainTelemetryChunkLengthForStart(startRegister);
    char label[48];
    buildMainTelemetryLabel(startRegister, registerCount, label, sizeof(label));
    ModbusReadResult result = runModbusRead(startRegister, registerCount, label);
    if (result.ok) {
      markBleTelemetryAction(millis());
      updateTelemetrySnapshot(
        latestMainTelemetry,
        result,
        startRegister,
        registerCount,
        label,
        "function_03_read_only_no_setting_write"
      );
    } else {
      emitError("ble_read_empty", "no Modbus response payload for main telemetry");
      markTelemetrySnapshotDirty(latestMainTelemetry);
    }
    nextMainTelemetryStartRegister = (uint16_t)(startRegister + registerCount);
    if (nextMainTelemetryStartRegister >= MAIN_TELEMETRY_TOTAL_REGISTERS) {
      nextMainTelemetryStartRegister = 0;
    }
    nextUploadMs = millis() + telemetryIntervalMs();
    finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, result.ok);
    return;
  }

  now = millis();
  if (nextDirtyTelemetryFlushRetryMs == 0 || (long)(now - nextDirtyTelemetryFlushRetryMs) >= 0) {
    flushOneDirtyTelemetrySnapshot();
  }
  finishRuntimeProbe(PROBE_TELEMETRY_SCHEDULER, probeStartedMs, true);
}

static void telemetryTaskLoop(void* parameter) {
  (void)parameter;
  for (;;) {
    maybeRunTelemetryScheduler();
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}

static void commandPollTaskLoop(void* parameter) {
  (void)parameter;
  for (;;) {
    pollPendingCommand();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
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
    resetModbusSession("target_cleared");
    targetMac = "";
    targetAddressTypeKnown = false;
    pairingStatus = "unconfigured";
    prefs.remove("target_mac");
    emitAck(line == "CLEAR_TARGET" ? "CLEAR_TARGET" : "CLEAR_TARGET_MAC");
  } else if (line.startsWith("SET_TARGET ") || line.startsWith("SET_TARGET_MAC ")) {
    int offset = line.startsWith("SET_TARGET_MAC ") ? 15 : 11;
    String nextTargetMac = line.substring(offset);
    String applyError;
    if (!applyLanConfig(nullptr, nullptr, nullptr, &nextTargetMac, nullptr, nullptr, nullptr, nullptr, false, applyError)) {
      emitError("set_target_mac_failed", applyError.c_str());
      return;
    }
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
    String nextSsid = rest.substring(0, split);
    String nextPassword = rest.substring(split + 1);
    String applyError;
    if (!applyLanConfig(&nextSsid, &nextPassword, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, true, applyError)) {
      emitError("set_wifi_failed", applyError.c_str());
      return;
    }
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
    String nextDeviceId = line.substring(14);
    String applyError;
    if (!applyLanConfig(nullptr, nullptr, &nextDeviceId, nullptr, nullptr, nullptr, nullptr, nullptr, false, applyError)) {
      emitError("set_device_id_failed", applyError.c_str());
      return;
    }
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
  } else if (line.startsWith("SET_BACKGROUND_TELEMETRY ")) {
    int value = line.substring(25).toInt();
    backgroundTelemetryDisabled = value == 0 ? false : true;
    emitAck("SET_BACKGROUND_TELEMETRY");
  } else if (line.startsWith("SET_BLE_CONNECTION ")) {
    int value = line.substring(19).toInt();
    setBleConnectionEnabled(value != 0, value != 0 ? "ble_enabled_via_command" : "ble_disabled_via_command");
    emitAck("SET_BLE_CONNECTION");
  } else if (line.startsWith("SET_TELEMETRY_UPLOADS ")) {
    int value = line.substring(22).toInt();
    telemetryUploadsDisabled = value == 0 ? false : true;
    emitAck("SET_TELEMETRY_UPLOADS");
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
  modbusOperationMutex = xSemaphoreCreateMutex();
  httpOperationMutex = xSemaphoreCreateMutex();

  loadConfig();
  otaLastOk = prefs.getBool("ota_last_ok", false);
  otaLastUrl = prefs.getString("ota_last_url", "");
  otaLastVersion = prefs.getString("ota_last_ver", "");
  otaFinishedMs = prefs.getULong("ota_last_ms", 0);
  otaValidationDone = prefs.getBool("ota_boot_validated", false);
  const esp_partition_t* runningPartition = esp_ota_get_running_partition();
  esp_ota_img_states_t runningState = ESP_OTA_IMG_UNDEFINED;
  if (runningPartition != nullptr && esp_ota_get_state_partition(runningPartition, &runningState) == ESP_OK) {
    if (runningState == ESP_OTA_IMG_PENDING_VERIFY) {
      otaAwaitingValidation = true;
      otaValidationDone = false;
      otaBootValidateAfterMs = millis() + 30000UL;
      JsonDocument otaBoot;
      otaBoot["type"] = "ota_boot_pending_verify";
      otaBoot["uptime_ms"] = 0;
      otaBoot["safety"] = "awaiting_health_gate_before_mark_valid";
      printJson(otaBoot);
    }
  }
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
  setupProvisioningWebServer();

  if (wifiSsid.length() == 0) {
    startProvisioningPortal(false);
  }

  emitStatus("boot");
  printHelp();

  if (telemetryTaskEnabled) {
    xTaskCreate(
      telemetryTaskLoop,
      "telemetry",
      8192,
      nullptr,
      1,
      &telemetryTaskHandle
    );
  }
  if (commandPollTaskEnabled) {
    xTaskCreate(
      commandPollTaskLoop,
      "command-poll",
      8192,
      nullptr,
      1,
      &commandPollTaskHandle
    );
  }
}

void loop() {
  uint32_t probeStartedMs = beginRuntimeProbe(PROBE_LOOP);
  if (Serial0.available()) {
    String line = Serial0.readStringUntil('\n');
    handleCommand(line);
  }
  handleProvisioningPortal();
  if (!commandPollingDisabled && !commandPollTaskEnabled) {
    pollPendingCommand();
  }
  if (!telemetryTaskEnabled) {
    maybeRunTelemetryScheduler();
  }
  maybeValidateOtaBoot();
  maybeEmitHeartbeat();
  feedWatchdog();
  delay(10);
  finishRuntimeProbe(PROBE_LOOP, probeStartedMs, true);
}
