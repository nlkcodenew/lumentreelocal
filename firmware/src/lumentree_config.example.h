#pragma once

// Optional compile-time defaults. Copy this file to lumentree_config.h for
// local builds if you want firmware defaults before serial provisioning.
// Do not commit lumentree_config.h; it can contain Wi-Fi and API secrets.

#define LUMENTREE_DEFAULT_WIFI_SSID ""
#define LUMENTREE_DEFAULT_WIFI_PASSWORD ""
#define LUMENTREE_DEFAULT_API_URL "https://lumentree.jonah.io.vn"
#define LUMENTREE_DEFAULT_API_TOKEN ""
#define LUMENTREE_DEFAULT_DEVICE_ID "P240819130"
#define LUMENTREE_DEFAULT_TARGET_MAC "d8:13:2a:ee:58:d6"
#define LUMENTREE_DEFAULT_GATEWAY_ID "esp32-lumentree"
#define LUMENTREE_DEFAULT_UPLOAD_INTERVAL_SECONDS 15
#define LUMENTREE_DEFAULT_PRODUCTION_ENABLED 0

// Set to 0 only if you provide and use certificate validation in firmware.
#define LUMENTREE_DEFAULT_TLS_INSECURE 1
