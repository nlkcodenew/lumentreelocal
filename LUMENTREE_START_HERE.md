# Lumentree Start Here

Every new session must read this file first.

Run this first if you are already in a shell:

```bash
git -C /home/mrlinh/esp32-lumentree whereami
git -C /home/mrlinh/esp32-lumentree-public whereami
```

## Choose The Correct Checkout

Use the private runtime checkout when the task involves any of these:

- firmware
- flash-site
- host runtime
- local API server
- operational docs
- evidence
- handoff
- production recovery

Private runtime checkout:

```text
/home/mrlinh/esp32-lumentree
branch = local-only
push = private/main
repo = https://github.com/nlkcodenew/lumentreelocal-private
```

Use the public HACS checkout when the task involves only these:

- Home Assistant integration code
- HACS metadata
- public README
- public changelog
- public-safe release work

Public HACS checkout:

```text
/home/mrlinh/esp32-lumentree-public
branch = main
push = origin/main
repo = https://github.com/nlkcodenew/lumentreelocal
```

## Hard Rule

- If the task mentions firmware, flash, host, runtime, docs, recovery, or server:
  use the private runtime checkout.
- If the task is only integration/HACS/public documentation:
  use the public HACS checkout.
- If unsure, choose the private runtime checkout first and confirm scope there.

## Next File To Read

After choosing a checkout, immediately read:

- `/home/mrlinh/esp32-lumentree/START_HERE.md`
- or `/home/mrlinh/esp32-lumentree-public/START_HERE.md`

## Current Private Runtime Baseline (2026-05-31)

- Latest private runtime branch head includes:
  - S3 poll3s env-driven telemetry timing
  - server SSE stream endpoint
  - HA SSE listener with polling fallback
- Final session report:
  - `/home/mrlinh/esp32-lumentree/docs/status/2026-05-31-s3-sse-rollout-and-stability-final.md`
- Final S3 firmware bin from this session:
  - `/home/mrlinh/esp32-lumentree/firmware/bin/s3/lumentree-s3-poll3s-sse-envdriven-a180f64.bin`
- First checks for next session:
  1. `curl http://127.0.0.1:8787/health`
  2. `curl http://192.168.1.151/api/status`
  3. verify HA integration against local server origin (not Cloudflare path)
