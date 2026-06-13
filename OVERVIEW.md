# Lumentree — Hệ thống giám sát inverter năng lượng mặt trời qua BLE

## Tổng quan

Lumentree là hệ thống tự xây dựng (private stack) để giám sát và điều khiển inverter năng lượng mặt trời thông qua BLE (Bluetooth Low Energy), không phụ thuộc vào cloud của nhà sản xuất.

**Luồng dữ liệu chính:**

```
Inverter (BLE Modbus) → ESP32 Gateway → Local API Server (Postgres) → Home Assistant
```

## Kiến trúc hệ thống

### 1. ESP32 Gateway (Firmware)

**Vị trí:** `firmware/src/main.cpp` (~5100 dòng)

ESP32 kết nối BLE tới inverter, đọc thanh ghi Modbus, và upload telemetry lên local server qua HTTP.

| Thành phần | Mô tả |
|---|---|
| BLE Session | Kết nối persistent tới inverter, đọc 95 thanh ghi chính |
| Telemetry Upload | POST JSON lên local server mỗi 3s (env-driven) |
| Command Poll | Nhận lệnh ghi từ server, thực thi qua BLE Modbus write |
| Local Portal | Web UI trên ESP32 để cấu hình WiFi, BLE, xem log |
| AP Mode | Chế độ provisioning ban đầu (192.168.4.1) |

**Các dòng phần cứng hiện tại:**

| Board | Env | Trạng thái | Vai trò |
|---|---|---|---|
| ESP32-S3 8MB | `esp32-s3-8mb-fastbulk-task-poll3s` | Active, production | Gateway chính |
| ESP32-C3 4MB | `esp32-c3-4mb-debug-cmdtask-fastbulk-task` | Experimental | Backup/test |

**Cấu hình runtime (NVS):** `wifi_ssid`, `wifi_pass`, `device_id`, `target_mac`, `api_url`, `api_token`, `gateway_id`, `ble_en`

**Điều khiển LAN:**
- `GET /api/status` — trạng thái toàn diện
- `POST /api/configure` — thay đổi config runtime
- `POST /api/reboot` — restart
- `POST /api/ble_connection` — bật/tắt BLE
- `GET /api/logs` — xem log gần nhất

### 2. Local API Server

**Vị trí:** `host/local-server/server.py` (~2700 dòng)
**Runtime:** Python, ThreadingHTTPServer, PostgreSQL (pgvector/pgvector:pg16 trong Docker)
**Bind:** `0.0.0.0:8787`
**Service:** `lumentree-local-server.service` (systemd)

| Chức năng | Endpoint |
|---|---|
| Nhận telemetry | `POST /api/lumentree/events` |
| Trạng thái thiết bị | `GET /api/lumentree/devices/{id}/latest` |
| Health check | `GET /api/lumentree/devices/{id}/health` |
| Năng lượng | `GET /api/lumentree/devices/{id}/energy` |
| Cài đặt inverter | `GET /api/lumentree/devices/{id}/settings` |
| SSE stream | `GET /api/lumentree/devices/{id}/stream` |
| Quản lý command | `POST /api/lumentree/commands` |
| Write grant | claim/revoke/status endpoints |
| Read grant | claim/revoke endpoints |

**Database schema chính:**
- `lumentree_devices` — thiết bị đã ghép
- `lumentree_telemetry` — dữ liệu đo lường (337k+ samples)
- `lumentree_energy_daily` — tổng hợp năng lượng theo ngày
- `lumentree_commands` — hàng đợi lệnh ghi
- `lumentree_write_grants` / `lumentree_read_grants` — phân quyền

**Bảo mật:** Bearer token cho mọi endpoint. Read/write grant cho multi-user.

### 3. Home Assistant Integration

**Vị trí:** `custom_components/lumentreelocal/`
**Phiên bản:** 0.15.0
**IoT class:** `local_polling`

| File | Vai trò |
|---|---|
| `coordinator.py` | Điều phối fetch data (poll + SSE fallback) |
| `api.py` | HTTP client tới local server |
| `sensor.py` | 40+ sensor entities (PV, grid, battery, load, energy) |
| `switch.py` | Điều khiển on/off (discharge enable, mains charge) |
| `number.py` | Điều khiển số (target SOC, discharge power) |
| `time.py` | Điều khiển thời gian (discharge start/end) |
| `binary_sensor.py` | Trạng thái online, write access |
| `schedule_safety.py` | Kiểm tra overlap lịch charge/discharge |
| `config_flow.py` | Cấu hình integration trong HA UI |

**Tính năng đặc biệt:**
- SSE realtime push + polling fallback
- Guarded semantic write (chỉ cho phép lệnh an toàn đã allowlist)
- Schedule overlap protection (không cho charge và discharge trùng giờ)
- Write grant flow (cần pairing code từ ESP32 để được quyền ghi)

### 4. Flash Site

**Vị trí:** `host/flash-site/`
**Bind:** `127.0.0.1:8790`
**Public:** `https://flash-lumentree.jonah.io.vn`

WebSerial flash site cho phép người dùng flash firmware trực tiếp từ trình duyệt. Hỗ trợ 3 board:
- ESP32-S3 Stable Legacy
- ESP32-S3 Preview
- ESP32-C3 Super Mini (Experimental)

### 5. BLE Collector (Legacy/Debug)

**Vị trí:** `host/ble-collector/`

Tool đọc serial USB từ ESP32 để debug, capture log, hoặc test telemetry pipeline mà không cần Wi-Fi.

## Mạng & Routing

```
┌─────────────────────────────────────────────────────────┐
│ Ubuntu Host (192.168.1.199)                             │
│                                                         │
│  ┌──────────────────┐    ┌─────────────────────────┐   │
│  │ Local Server     │    │ Cloudflare Tunnel        │   │
│  │ 0.0.0.0:8787     │◄───│ lumentree.jonah.io.vn   │   │
│  └────────┬─────────┘    └─────────────────────────┘   │
│           │                                             │
│  ┌────────┴─────────┐                                  │
│  │ PostgreSQL        │                                  │
│  │ Docker :5432      │                                  │
│  └──────────────────┘                                  │
│           │                                             │
│  ┌────────┴─────────────────────┐                      │
│  │ virbr0: 192.168.122.1        │                      │
│  └────────┬─────────────────────┘                      │
│           │                                             │
│  ┌────────┴─────────────────────┐                      │
│  │ HAOS VM: 192.168.122.230     │                      │
│  │ Poll → http://192.168.122.1  │                      │
│  │         :8787                 │                      │
│  └──────────────────────────────┘                      │
└─────────────────────────────────────────────────────────┘

WiFi LAN (192.168.1.0/24):
  ESP32-S3: 192.168.1.151 → http://192.168.1.199:8787
  ESP32-C3: 192.168.1.245 → http://192.168.1.199:8787 (hoặc Cloudflare)
```

**Quy tắc routing:**
- HA poll qua local origin (`192.168.122.1:8787`), không qua Cloudflare
- ESP32 upload trực tiếp local origin (`192.168.1.199:8787`)
- Cloudflare chỉ dùng cho remote browser access

## Luồng Write Command

```
HA UI toggle → HA integration → POST /api/lumentree/commands
  → Server queue command (pending)
  → ESP32 poll /commands/next (mỗi 3s)
  → ESP32 ghi BLE Modbus register
  → ESP32 POST /commands/{id}/complete
  → HA poll thấy thay đổi trong settings
```

**Safety model:**
- Chỉ semantic write (target SOC, discharge power/time, mains charge)
- Không có generic register write API
- Schedule overlap protection ở cả 3 layer (HA, server, firmware)
- Write grant required (pairing code flow)

## USB & Flash

| Port | Board | MAC |
|---|---|---|
| `/dev/ttyACM0` | ESP32-C3 | `9c:cc:01:c0:a3:38` |
| `/dev/ttyACM1` | ESP32-S3 | `3c:dc:75:63:47:5c` |

**Luôn chạy `esptool --port <port> chip-id` trước khi flash.**

## Build & Deploy

```bash
# Build S3
pio run -d firmware -e esp32-s3-8mb-fastbulk-task-poll3s

# Flash S3
pio run -d firmware -e esp32-s3-8mb-fastbulk-task-poll3s -t upload --upload-port /dev/ttyACM1

# Build C3
pio run -d firmware -e esp32-c3-4mb-debug-cmdtask-fastbulk-task

# Flash C3
pio run -d firmware -e esp32-c3-4mb-debug-cmdtask-fastbulk-task -t upload --upload-port /dev/ttyACM0

# Deploy HA integration
python3 tools/deploy_lumentreelocal_to_haos.py --ha-url "$HA_URL" --ha-token "$HA_TOKEN"

# Sync flash site
./tools/sync_flash_site_artifacts.sh
```

## Repository Layout

```
esp32-lumentree/
├── firmware/
│   ├── src/main.cpp              # Firmware source (5100 lines)
│   ├── platformio.ini            # Build environments
│   ├── bin/                      # Exported firmware binaries
│   └── backups/                  # Firmware backup trước khi thay đổi
├── host/
│   ├── local-server/
│   │   ├── server.py             # API server (2700 lines)
│   │   └── lumentree_decode.py   # Modbus register decoder
│   ├── flash-site/               # WebSerial flash website
│   └── ble-collector/            # USB serial debug tool
├── custom_components/
│   └── lumentreelocal/           # HA integration (15 files)
├── tools/                        # Deploy & sync scripts
├── docs/
│   ├── specs/                    # Design specifications
│   ├── plans/                    # Implementation plans
│   ├── status/                   # Session reports & evidence
│   ├── evidence/                 # Register mappings & backups
│   └── archive/                  # Archived session docs
└── OVERVIEW.md                   # File này
```

## Trạng thái hiện tại (2026-06-13)

- **ESP32-S3**: Active, firmware `0.15.3-exp-s3-fastbulk-task-poll3s`, upload trực tiếp local origin
- **ESP32-C3**: Experimental line, firmware `0.15.2-exp-c3-fastbulk-task`
- **Local server**: Running (systemd), PostgreSQL healthy (Docker)
- **HA integration**: v0.15.0, SSE + polling, local origin routing
- **Flash site**: 3 board choices live
- **Total telemetry samples**: 337,000+

## Repos liên quan

| Repo | Mục đích | Branch |
|---|---|---|
| `/home/mrlinh/esp32-lumentree` (private) | Runtime, firmware, server, docs | `local-only` |
| `/home/mrlinh/esp32-lumentree-public` | HACS integration public release | `main` |

## Operational Notes

- Nếu ESP32 mất BLE session >5 phút: reboot qua `POST /api/reboot`
- Nếu HA hiện "cannot connect": kiểm tra local server trước, sau đó mới kiểm tra Cloudflare
- PostgreSQL chạy trong Docker (`infra-postgres`), nếu crash sẽ tự recovery
- Firmware config thay đổi runtime qua `POST /api/configure` (lưu NVS, không cần reflash)
- Backup firmware trước mọi thay đổi quan trọng: `firmware/backups/`
