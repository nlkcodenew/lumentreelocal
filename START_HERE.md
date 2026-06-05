# START HERE: Private Runtime Repo

Repo này dùng để sửa và vận hành runtime local
(`firmware + host + HA runtime + docs nội bộ`).

## Cấu trúc chuẩn

- `firmware/`: source firmware ESP32 để sửa/build/flash
- `firmware/src/main.cpp`: luồng scheduler/command/telemetry chính
- `firmware/platformio.ini`: các env build cho từng line thiết bị
- `host/local-server/`: local API server + Postgres bridge
- `host/flash-site/`: site phục vụ firmware nội bộ
- `custom_components/lumentreelocal/`: HA integration
- `docs/evidence/device-backups/archives/`: nơi lưu toàn bộ file backup `.tar.gz`

## Không cần đi tìm source firmware ở nơi khác

Luồng chuẩn là:
1. Sửa code trong `firmware/src/`
2. Build bằng env tương ứng trong `firmware/platformio.ini`
3. Flash đúng cổng USB thiết bị
4. Kiểm tra `http://<ip-board>/api/status`

## Lệnh chuẩn

Check baseline trước khi sửa:

```bash
git -C /home/mrlinh/esp32-lumentree whereami
curl http://127.0.0.1:8787/health
curl http://192.168.122.1:8787/health
```

Build S3 poll-3s:

```bash
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-s3-8mb-fastbulk-task-poll3s
```

Flash S3 poll-3s:

```bash
python -m esptool --port /dev/ttyACM1 chip-id
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-s3-8mb-fastbulk-task-poll3s -t upload --upload-port /dev/ttyACM1
```

Build current C3 experimental runtime line:

```bash
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-c3-4mb-debug-cmdtask-fastbulk-task
```

Flash current C3 experimental runtime line:

```bash
python -m esptool --port /dev/ttyACM0 chip-id
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-c3-4mb-debug-cmdtask-fastbulk-task -t upload --upload-port /dev/ttyACM0
```

Xem cổng serial:

```bash
platformio device list
```

## Mapping env theo thiết bị

- `esp32-s3-8mb-fastbulk-task-poll3s`: S3 test line poll 3s
- `esp32-s3-8mb-fastbulk-task-poll5s`: S3 test line poll 5s
- `esp32-s3-8mb-fastbulk-task-poll7s`: S3 test line poll 7s
- `esp32-c3-4mb-experimental`: C3 baseline experimental line
- `esp32-c3-4mb-debug-cmdtask-fastbulk-task`: C3 current debug/runtime line

## USB mapping đã xác minh

- `/dev/ttyACM0` = `ESP32-C3`
- `/dev/ttyACM1` = `ESP32-S3`
- Không flash theo trí nhớ. Luôn chạy `esptool ... chip-id` trước.

## Quy ước repo

- Backup artifacts chỉ đặt trong `docs/evidence/device-backups/archives/`
- Không để file `.tar.gz` ở root repo
- Trước khi flash production-like: commit trước, flash sau

## Baseline chốt hiện tại (2026-05-31)

- `ESP32-S3` line đã có:
  - timing env-driven (không hardcode cứng interval)
  - SSE từ local server sang HA integration + polling fallback
- Bin S3 chốt:
  - `firmware/bin/s3/lumentree-s3-poll3s-sse-envdriven-a180f64.bin`
- Báo cáo session:
  - `docs/status/2026-05-31-s3-sse-rollout-and-stability-final.md`
- HA nội bộ phải đi local origin:
  - `HAOS VM -> http://192.168.122.1:8787`
  - không dùng Cloudflare URL cho polling nội bộ nếu local origin còn sống
