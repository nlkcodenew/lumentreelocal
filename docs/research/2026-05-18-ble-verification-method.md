# BLE Verification Method for Register Writes

## Vấn đề

Sau khi thực hiện nhiều lệnh write qua BLE liên tục, inverter có thể mất
kết nối WiFi/cloud tạm thời, khiến app vendor Lumentree không truy cập được.
Không thể dùng app vendor làm phương pháp kiểm chứng duy nhất.

## Giải pháp: Đọc trực tiếp qua BLE bằng ESP32 serial

ESP32 có thể đọc bất kỳ register nào qua BLE mà không cần inverter có WiFi.
Dùng lệnh serial `READ_RANGE` để verify ngay sau khi write.

### Cách dùng

```bash
# Cấu hình serial
stty -F /dev/ttyACM0 115200 raw -echo

# Gửi lệnh đọc (ví dụ: đọc registers 180-184, tức 7 registers bắt đầu từ 178)
echo "READ_RANGE 178 7" > /dev/ttyACM0

# Đọc response (tìm dòng ble_modbus_response)
timeout 15 cat /dev/ttyACM0 | grep '"type":"ble_modbus_response"' | head -1
```

### Decode response

Response JSON chứa `payload_hex` — một Modbus FC03 response frame:

```
payload_hex: 01030E005507D00DAC07D00DAC0DAC0DAC85FB
             ││││││ ← data bytes ─────────────→ ││││
             ││││└─ byte count (0x0E = 14 bytes = 7 registers)
             ││└─── function code (03 = read)
             └───── slave id (01)                └──── CRC
```

Decode data bytes thành uint16 big-endian:

```python
import struct
payload = "01030E005507D00DAC07D00DAC0DAC0DAC85FB"
data = bytes.fromhex(payload[6:-4])  # skip 0103BE header and CRC
for i in range(len(data)//2):
    reg = start_register + i
    val = struct.unpack(">H", data[i*2:i*2+2])[0]
    print(f"Register {reg}: {val}")
```

### Ví dụ thực tế — Verify discharge power rollback

Sau khi gửi 4 lệnh write rollback discharge power về 3500W, app vendor
không truy cập được. Dùng BLE read trực tiếp:

```
$ echo "READ_RANGE 178 7" > /dev/ttyACM0

Response:
  Register 178: 85
  Register 179: 2000
  Register 180: 3500  ← First discharge power  ✅
  Register 181: 2000
  Register 182: 3500  ← Second discharge power ✅
  Register 183: 3500  ← Third discharge power  ✅
  Register 184: 3500  ← Fourth discharge power ✅
```

Kết quả xác nhận 4/4 registers đã rollback thành công mà không cần vendor app.

### Các register range thường dùng

| Mục đích | Lệnh | Registers |
|----------|-------|-----------|
| Target SOC | `READ_RANGE 144 1` | 144 |
| Discharge power (4 slots) | `READ_RANGE 178 7` | 180, 182, 183, 184 |
| Full settings range | `READ_RANGE 95 95` | 95–189 |
| Main telemetry | `READ_RANGE 0 95` | 0–94 |
| Cell voltages | `READ_RANGE 250 50` | 250–299 |

### Lưu ý

- `READ_RANGE` giới hạn tối đa 95 registers mỗi lần
- Chỉ dùng Modbus FC03 (read-only), không ảnh hưởng inverter
- Không cần inverter có WiFi — hoạt động hoàn toàn qua BLE
- Nên dùng làm bước verify chính trong mọi quy trình test write
- Nếu BLE read thất bại, ESP32 có thể đang bận write — đợi vài giây rồi thử lại

---

*Phát hiện: 2026-05-18, khi vendor app mất kết nối sau chuỗi discharge power write tests*
