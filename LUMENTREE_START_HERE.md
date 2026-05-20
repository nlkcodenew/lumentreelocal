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
