# Writable Register Candidates — Lumentree Inverter

> **CẢNH BÁO:** Tài liệu này chỉ mang tính nghiên cứu (read-only research).
> KHÔNG write bất kỳ register nào nếu chưa hoàn tất quy trình kiểm chứng
> từng mục (vendor MQTT capture, pre-read, post-read, rollback plan).

## Phương pháp

Dữ liệu thu thập từ:

1. **Register range scan** — Modbus FC03 read-only, ranges 95-189 (firmware 0.7.0)
2. **Vendor MQTT captures** — passive listen trên `listenApp/` và `reportApp/`
3. **Vendor app diff** — so sánh register values trước/sau khi thay đổi setting
4. **Firmware source code** — đã triển khai write cho register 144
5. **Vendor app UI observation** — ghi nhận các tham số hiển thị trên app

## Bối cảnh: App Vendor Lumentree

App vendor có các mục settings chính (quan sát từ UI):

```
Work Control Settings
├── The battery discharges to the loads
│   ├── First discharge time
│   │   ├── Enable toggle (on/off)
│   │   ├── Time start (e.g. 20:00)
│   │   ├── Time end (e.g. 08:00)
│   │   ├── Target SOC (e.g. 6%)           ← registers 144/146/177/178, ĐÃ KIỂM CHỨNG
│   │   └── Discharge power
│   └── Evidence: docs/evidence/2026-05-18-discharge-entities-complete-mapping.md
├── Mains charge the battery
│   └── Enable / parameters
├── Output source priority
├── Charger source priority
└── Battery type / cutoff thresholds
```

---

## 1. ĐÃ KIỂM CHỨNG (Confirmed Writable)

### The battery discharges to the loads

| Field | Value |
|-------|-------|
| **Registers** | 144, 146, 177, 178 |
| **Data type** | uint16 |
| **Unit** | % (phần trăm SOC) |
| **Valid range** | 5–100 |
| **Write function** | Modbus FC16 (Write Multiple Registers) |
| **Vendor evidence** | Vendor-app diff confirmed all four slots |
| **ESP32 test** | Production command IDs 11-14 set `11/22/33/44` to `6/50/20/85`, verified |
| **FC06 kết quả** | Thất bại — inverter không echo, value không đổi |
| **Status** | Production — firmware 0.13.0, API + HASS service/entities |
| **Evidence file** | `docs/evidence/2026-05-18-discharge-entities-complete-mapping.md` |

---

## 2. ỨNG VIÊN CAO (High Confidence Candidates)

Các register sau nằm trong vùng settings 95-189, có giá trị ổn định (không
thay đổi giữa các lần đọc telemetry), và có ý nghĩa rõ ràng khi đối chiếu
với app vendor.

### Register 143 — Mains Charge First Target SOC

| Field | Value |
|-------|-------|
| **Register** | 143 (0x008F) |
| **Observed value** | 29 |
| **Confirmed interpretation** | `Mains charge the battery` first charge target SOC |
| **Unit** | % |
| **Evidence** | Read-only `READ_RANGE 95 95` matched app value `29%` |
| **Status** | Confirmed read mapping; write not approved |
| **Evidence file** | `docs/evidence/2026-05-18-mains-charge-battery-entity-mapping.md` |

### Register 145 — Mains Charge Second Target SOC

| Field | Value |
|-------|-------|
| **Register** | 145 (0x0091) |
| **Observed value** | 69 |
| **Confirmed interpretation** | `Mains charge the battery` second charge target SOC |
| **Unit** | % |
| **Evidence** | Read-only `READ_RANGE 95 95` matched app value `69%` |
| **Status** | Confirmed read mapping; write not approved |
| **Evidence file** | `docs/evidence/2026-05-18-mains-charge-battery-entity-mapping.md` |

### Register 146 — Superseded Candidate

| Field | Value |
|-------|-------|
| **Register** | 146 (0x0092) |
| **Observed value** | 50 |
| **Old interpretation** | SOC mục tiêu khi sạc từ lưới (grid charge target) |
| **Confirmed interpretation** | `The battery discharges to the loads` slot 2 target SOC |
| **Status** | Superseded by vendor-app diff and production write test |
| **Evidence file** | `docs/evidence/2026-05-18-discharge-entities-complete-mapping.md` |

### Register 148 — Max Charge SOC / Charge Cutoff SOC

| Field | Value |
|-------|-------|
| **Register** | 148 (0x0094) |
| **Observed value** | 90 |
| **Interpretation** | SOC tối đa khi sạc (charge cutoff) |
| **Unit (dự đoán)** | % |
| **Range (dự đoán)** | 50–100 |
| **Evidence** | Ổn định; 90% là giá trị charge cutoff phổ biến |
| **Confidence** | ⭐⭐⭐⭐ Cao |
| **Cần kiểm chứng** | Thay đổi max charge setting từ vendor app |

### Register 149 — Battery Reserve / Backup SOC

| Field | Value |
|-------|-------|
| **Register** | 149 (0x0095) |
| **Observed value** | 90 |
| **Interpretation** | SOC dự trữ cho UPS/backup mode |
| **Unit (dự đoán)** | % |
| **Range (dự đoán)** | 5–100 |
| **Evidence** | Ổn định; cùng cluster với 148 |
| **Confidence** | ⭐⭐⭐ Trung bình-Cao |
| **Cần kiểm chứng** | Thay đổi backup reserve từ vendor app |

### Register 180 — Max Discharge Power (Slot 1)

| Field | Value |
|-------|-------|
| **Register** | 180 (0x00B4) |
| **Observed value** | 3500 |
| **Interpretation** | Công suất xả tối đa khe thời gian 1 |
| **Unit (dự đoán)** | W (Watt) |
| **Range (dự đoán)** | 0–5000+ |
| **Evidence** | App vendor hiển thị discharge power = 3500W; register = 3500 |
| **Confidence** | ⭐⭐⭐⭐⭐ Rất cao |
| **Cần kiểm chứng** | Thay đổi discharge power từ vendor app, capture MQTT |

### Register 182 — Max Discharge Power (Slot 2 hoặc duplicate)

| Field | Value |
|-------|-------|
| **Register** | 182 (0x00B6) |
| **Observed value** | 3500 |
| **Interpretation** | Có thể là discharge power cho khe thời gian 2 |
| **Unit (dự đoán)** | W |
| **Evidence** | Cùng giá trị 3500 như reg 180 |
| **Confidence** | ⭐⭐⭐⭐ Cao |
| **Cần kiểm chứng** | Diff khi thay đổi power settings |

### Register 183 — Max Discharge Power (Slot 3 hoặc global)

| Field | Value |
|-------|-------|
| **Register** | 183 (0x00B7) |
| **Observed value** | 3500 |
| **Interpretation** | Có thể là discharge power global hoặc slot 3 |
| **Unit (dự đoán)** | W |
| **Evidence** | Cùng giá trị 3500 |
| **Confidence** | ⭐⭐⭐⭐ Cao |
| **Cần kiểm chứng** | Diff khi thay đổi power settings |

---

## 3. ỨNG VIÊN TRUNG BÌNH (Medium Confidence Candidates)

### Register 100 — First Discharge Time Enable Flag

| Field | Value |
|-------|-------|
| **Register** | 100 (0x0064) |
| **Observed value** | 1 |
| **Interpretation** | Enable/disable first discharge time schedule |
| **Unit (dự đoán)** | Boolean (0=off, 1=on) |
| **Evidence** | App vendor có toggle on/off; reg 100 = 1 (đang bật) |
| **Confidence** | ⭐⭐⭐ Trung bình |
| **Cần kiểm chứng** | Toggle on/off từ vendor app, diff register |
| **Lưu ý** | App user đã ghi nhận "First discharge time toggle: off" nhưng reg = 1 — cần xác minh lại |

### Register 108 — Second Schedule Enable / Grid Charge Enable

| Field | Value |
|-------|-------|
| **Register** | 108 (0x006C) |
| **Observed value** | 1 |
| **Interpretation** | Enable flag cho schedule thứ 2 hoặc grid charge |
| **Unit (dự đoán)** | Boolean (0/1) |
| **Confidence** | ⭐⭐⭐ Trung bình |
| **Cần kiểm chứng** | Toggle tương ứng từ vendor app |

### Register 120 — Third Schedule Enable / Work Mode Flag

| Field | Value |
|-------|-------|
| **Register** | 120 (0x0078) |
| **Observed value** | 1 |
| **Interpretation** | Enable flag cho schedule 3 hoặc work mode setting |
| **Unit (dự đoán)** | Boolean (0/1) |
| **Confidence** | ⭐⭐⭐ Trung bình |

### Register 125 — Fourth Enable Flag

| Field | Value |
|-------|-------|
| **Register** | 125 (0x007D) |
| **Observed value** | 1 |
| **Interpretation** | Enable flag (work mode / output source / charger source) |
| **Unit (dự đoán)** | Boolean (0/1) |
| **Confidence** | ⭐⭐ Trung bình-Thấp |

### Time Window Registers

| Register | Value | Interpretation (dự đoán) |
|----------|-------|--------------------------|
| 160 | 6661 (0x1A05) | Có thể encode giờ phút: 0x1A=26? hoặc packed HH:MM |
| 161 | 4618 (0x120A) | Có thể encode giờ phút: 0x12=18, 0x0A=10 → 18:10? |

**Lưu ý:** Vendor app hiển thị time start=20:00, time end=08:00.
Nếu decode theo byte: reg 160 = `1A 05` → 26:05 (vô lý) hoặc `05 1A` →
5:26. Cần thêm dữ liệu decode. Hoặc có thể là packed format khác.

---

## 4. ỨNG VIÊN THẤP / CẦN NGHIÊN CỨU THÊM (Low Confidence)

### Voltage/Power Configuration Registers

| Register | Value | Interpretation (dự đoán) |
|----------|-------|--------------------------|
| 101 | 5600 | AC charge voltage limit (56.0V × 0.01?) |
| 102 | 5700 | AC charge voltage cutoff (57.0V?) |
| 103 | 5600 | PV charge voltage (56.0V?) |
| 104 | 314 | Dòng sạc? (31.4A × 0.1?) |
| 106 | 58 | Charge current limit? (58A? hoặc 5.8A × 0.1?) |
| 107 | 70 | Grid charge current? |
| 111 | 5 | Min SOC hoặc shutdown SOC |
| 112 | 10 | LƯU Ý: Giá trị = 10 nhưng KHÔNG thay đổi khi target SOC thay đổi 10→13 |
| 114 | 5000 | Battery voltage high (50.00V × 0.01) |
| 115 | 5200 | Battery voltage cutoff (52.00V × 0.01) |
| 117 | 5000 | Battery charge voltage (50.00V?) |
| 118 | 5000 | Battery float voltage (50.00V?) |
| 130 | 805 | Unknown (uptime-like or config version?) |
| 131 | 2000 | Max charge power? (2000W) |
| 132 | 1400 | Charge power limit? (1400W) |
| 133 | 1600 | Charge power cutoff? (1600W) |
| 138 | 1200 | Grid charge power? (1200W) |
| 139 | 800 | Grid charge current × 10? (80.0A?) hoặc power limit |
| 140 | 1600 | PV charge power limit? (1600W) |
| 141 | 2000 | PV charge max power? (2000W) |
| 150 | 1 | Enable flag (charger source priority?) |
| 153 | 5300 | Battery over-voltage protection (53.00V) |
| 154 | 5000 | Battery under-voltage protection (50.00V) |
| 155 | 5500 | Battery high-voltage alarm (55.00V) |
| 156 | 5000 | Battery low-voltage alarm (50.00V) |
| 157 | 60 | Timeout / interval (60s?) |
| 173 | 800 | Min discharge power? (800W) |
| 174 | 1400 | Discharge power limit step? |
| 175 | 1400 | Duplicate hoặc slot 2 |
| 176 | 1600 | Discharge power ceiling? |
| 177 | 20 | SOC reserve cho mode khác? (20%) |
| 178 | 85 | SOC alarm high? (85%) |
| 179 | 2000 | Max charge power global? (2000W) |
| 181 | 2000 | Max charge power slot 2? |
| 184 | 3500 | Discharge power duplicate / slot 4 |
| 186 | 5480 | Voltage threshold (54.80V) |
| 188 | 61 | Unknown |
| 189 | 76 | Unknown |

### Volatile / Telemetry Registers (KHÔNG PHẢI settings)

| Register | Value | Interpretation |
|----------|-------|----------------|
| 95 | 325→331→332 | Counter — tăng khi settings thay đổi; KHÔNG phải writable setting |
| 162 | 11293→13112→14850 | Volatile telemetry counter — KHÔNG phải setting |
| 190–192 | Thay đổi giữa scans | Volatile telemetry — KHÔNG phải settings |

---

## 5. CÁC REGISTER CHẮC CHẮN READ-ONLY (0-94)

Toàn bộ 95 registers 0-94 là telemetry thời gian thực, bao gồm:

| Register | Tên | Loại |
|----------|-----|------|
| 3-12 | Device SN, Battery V/I | Telemetry |
| 13-17 | AC out/in voltage, frequency | Telemetry |
| 18-22 | AC power, PV1 voltage/power | Telemetry |
| 24 | Device temperature | Telemetry |
| 37 | Battery type | Config (read-only) |
| 50 | Battery SOC | Telemetry |
| 53-61 | AC/Grid/Battery power | Telemetry |
| 67-68 | Load power, UPS mode | Telemetry |
| 70 | Master/slave status | Telemetry |
| 72-74 | PV2 voltage/power | Telemetry |

---

## 6. QUY TRÌNH KIỂM CHỨNG CHO TỪNG REGISTER MỚI

Dựa trên quy trình đã thành công với register 144:

```
Bước 1: Baseline read (FC03) trước khi thay đổi
Bước 2: Thay đổi tham số từ vendor app
Bước 3: Capture vendor MQTT (listenApp/reportApp)
Bước 4: Baseline read (FC03) sau khi thay đổi
Bước 5: Diff registers — xác định register changed
Bước 6: Lặp lại với giá trị thứ 3 để confirm
Bước 7: Decode vendor write frame (FC16 expected)
Bước 8: Viết command spec (semantic, range, pre/post read)
Bước 9: Test write (guarded, pre-read, ACK, post-read)
Bước 10: Verify trên vendor app
```

## 7. ĐỀ XUẤT THỨ TỰ ƯU TIÊN KIỂM CHỨNG

Dựa trên giá trị thực tế cho người dùng:

| Ưu tiên | Register | Tham số | Lý do |
|---------|----------|---------|-------|
| 1 | 180 | Discharge Power | Trực tiếp map 3500W từ app; cùng mẫu FC16 |
| 2 | 143 | Reserve/Min SOC | Bảo vệ pin; cùng cluster SOC |
| 3 | 148 | Max Charge SOC | Kéo dài tuổi thọ pin (charge to 90% thay vì 100%) |
| 4 | 100 | Enable first discharge | Toggle on/off schedule |
| 5 | 146 | Grid Charge Target SOC | Tối ưu hóa sạc từ lưới |
| 6 | 160-161 | Time window | Lên lịch xả pin |
| 7 | 101-107 | Voltage/current limits | Nâng cao — ảnh hưởng phần cứng |

---

## 8. GHI CHÚ AN TOÀN

- **FC16 là bắt buộc** — FC06 đã thất bại với register 144; vendor app dùng FC16
- **Không generic register writer** — chỉ semantic commands
- **Pre-read + ACK + post-read** cho mọi write
- **Rollback value** phải được ghi lại trước khi write
- **Một command một lúc** — không batch write nhiều registers
- **Các register voltage/current (101-107, 114-118, 153-156)** có rủi ro cao,
  write sai có thể gây hư hại phần cứng — để cuối cùng và cần extra caution

---

*Tạo: 2026-05-18*
*Firmware reference: lumentree-ble-bridge/0.9.0*
*Data source: register scan at target SOC = 10%, before/after diffs at 10→13, 13→16*
*Inverter: P240819130*
