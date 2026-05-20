# Time Register Mapping — Next Steps

## Current Status

User đã thay đổi tất cả 4 discharge time slots +1 phút qua vendor app (Bluetooth):
- First: 20:00→08:00 thành 20:01→08:01
- Second: 16:00→20:00 thành 16:01→20:01
- Third: 08:00→14:00 thành 08:01→14:01
- Fourth: 14:00→16:00 thành 14:01→16:01

Các thay đổi này đã hoàn tất trên inverter.

## Baseline Payload (Before Change)

Đã có baseline từ session trước (tất cả 4 slots ở thời gian gốc, all enables ON):

```
0103BE014C0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D005780640000000010000000104B00320064007D0000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A051213072D000000000001000100000000000000000000000003200578057806400014005507D00C8007D00C800C800C80000015680000003D004CBDC1
```

Decoded key registers từ baseline:
- Reg 135 = 1 (First enable ON)
- Reg 137 = 1 (Second enable ON)
- Reg 144 = 6 (Target SOC 6%)
- Reg 151 = 1 (Third enable ON)
- Reg 152 = 1 (Fourth enable ON)
- Reg 180 = 3200 (First discharge power)
- Reg 182 = 3200 (Second discharge power)
- Reg 183 = 3200 (Third discharge power)
- Reg 184 = 3200 (Fourth discharge power)

Các giá trị có thể là time (HH*100+MM format):
- 2000 (20:00), 800 (08:00), 1600 (16:00), 1400 (14:00)

## Việc Cần Làm

### 1. Đọc registers sau khi thay đổi +1 phút

ESP32 hiện đang gặp vấn đề không phản hồi READ_RANGE. Cần:

```bash
# Kiểm tra ESP32 status
python3 << 'PYEOF'
import serial, time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(0.5)
ser.reset_input_buffer()
ser.write(b'STATUS\n')
ser.flush()
time.sleep(2)
print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'))
ser.close()
PYEOF

# Nếu production_enabled=true, tắt đi:
echo "SET_PRODUCTION 0" > /dev/ttyACM0
sleep 3

# Đọc registers 95-189:
python3 << 'PYEOF'
import serial, time, sys
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
time.sleep(0.5)
ser.reset_input_buffer()
ser.write(b'READ_RANGE 95 95\n')
ser.flush()
start = time.time()
buffer = ""
while time.time() - start < 20:
    if ser.in_waiting:
        chunk = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        buffer += chunk
        lines = buffer.split('\n')
        for line in lines[:-1]:
            if '"type":"ble_modbus_response"' in line and '"label":"manual_registers_95"' in line:
                print(line)
                ser.close()
                exit(0)
        buffer = lines[-1]
    time.sleep(0.2)
ser.close()
print("timeout", file=sys.stderr)
exit(1)
PYEOF
```

Nếu ESP32 vẫn không phản hồi: reset ESP32 (unplug/replug USB hoặc nút reset).

### 2. Decode payload và tìm registers thay đổi

Sau khi có response JSON với `payload_hex`, decode và so sánh với baseline:

```python
import struct

# Baseline payload (remove 0103BE header and CRC)
baseline_hex = "014C0000000000000000000115E0164415E0013A0000003A00460001000000000005000A00001388145000001388138800000001000000000000000000010000000000000000032507D005780640000000010000000104B00320064007D0000000190006002800320000005A005A00010001000114B41388157C1388003C000000001A051213072D000000000001000100000000000000000000000003200578057806400014005507D00C8007D00C800C800C80000015680000003D00"

# New payload (extract từ response JSON)
new_hex = "..."  # payload_hex từ response, bỏ 0103BE header và 4 bytes CRC cuối

baseline_data = bytes.fromhex(baseline_hex)
new_data = bytes.fromhex(new_hex)

# So sánh từng register
for i in range(len(baseline_data)//2):
    reg = 95 + i
    baseline_val = struct.unpack(">H", baseline_data[i*2:i*2+2])[0]
    new_val = struct.unpack(">H", new_data[i*2:i*2+2])[0]
    if baseline_val != new_val:
        print(f"Register {reg}: {baseline_val} → {new_val}")
```

### 3. Xác định time register mapping

Tìm các registers thay đổi từ round values sang +1 values:
- 2000 → 2001 (20:00 → 20:01)
- 800 → 801 (08:00 → 08:01)
- 1600 → 1601 (16:00 → 16:01)
- 1400 → 1401 (14:00 → 14:01)

Mỗi slot có 2 time registers (start và end), tổng cộng 8 registers cần tìm.

Expected pattern:
- First slot: time_start register (20:00→20:01), time_end register (08:00→08:01)
- Second slot: time_start register (16:00→16:01), time_end register (20:00→20:01)
- Third slot: time_start register (08:00→08:01), time_end register (14:00→14:01)
- Fourth slot: time_start register (14:00→14:01), time_end register (16:00→16:01)

### 4. Ghi evidence file

Sau khi xác định được mapping, ghi vào:
`docs/evidence/2026-05-18-discharge-time-register-mapping-confirmed.md`

Format tương tự discharge power evidence file, bao gồm:
- Baseline values
- Changed values
- Register mapping table
- Raw payloads

### 5. Implement write commands

Sau khi có mapping, implement 2 commands:
- `set_discharge_time_start` (slot 1-4, time in HHMM format 0000-2359)
- `set_discharge_time_end` (slot 1-4, time in HHMM format 0000-2359)

Pattern giống discharge power commands:
- Firmware: add helper functions, validation, pre-read/write/post-read
- API: add to ALLOWED_WRITE_COMMANDS, validate slot and time format
- Bump firmware version to 0.11.0

### 6. Rollback về baseline

Sau khi test xong, rollback tất cả time settings về giá trị gốc qua vendor app để giữ inverter ở trạng thái known-good.

## Notes

- ESP32 BLE có thể conflict với inverter WiFi — nếu cần vendor app, tắt ESP32 production mode
- Luôn dùng BLE READ_RANGE để verify thay vì dựa vào vendor app
- Time format: HHMM → register value = HH*100 + MM (e.g., 20:01 = 2001)
