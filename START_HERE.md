# START HERE: Private Runtime Repo

Repo này dùng để sửa và vận hành runtime local (firmware + host + docs nội bộ).

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

Build S3 poll-3s:

```bash
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-s3-8mb-fastbulk-task-poll3s
```

Flash S3 poll-3s:

```bash
platformio run -d /home/mrlinh/esp32-lumentree/firmware -e esp32-s3-8mb-fastbulk-task-poll3s -t upload --upload-port /dev/ttyACM0
```

Xem cổng serial:

```bash
platformio device list
```

## Mapping env theo thiết bị

- `esp32-s3-8mb-fastbulk-task-poll3s`: S3 test line poll 3s
- `esp32-s3-8mb-fastbulk-task-poll5s`: S3 test line poll 5s
- `esp32-s3-8mb-fastbulk-task-poll7s`: S3 test line poll 7s
- `esp32-c3-4mb-experimental`: C3 experimental line

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
