# Lumentree BLE Serial Collector

This collector reads JSONL from the dedicated `firmware` firmware over USB serial.

It can run in log-only mode first, then optionally store events in Postgres.

## Install

```bash
cd host/ble-collector
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
```

## Log Only

```bash
python collector.py --port /dev/ttyACM0 --send STATUS --scan-once
```

Bounded capture to a local JSONL file:

```bash
python collector.py --port /dev/ttyACM0 --duration 20 --jsonl-out logs/lumentree-ble.jsonl --send STATUS --scan-once
```

Read realtime registers once through the dedicated firmware:

```bash
python collector.py --port /dev/ttyACM0 --duration 15 --send STATUS --read-main-once
```

Continuously read realtime registers every 30 seconds:

```bash
python collector.py \
  --port /dev/ttyACM0 \
  --send 'SET_TARGET d8:13:2a:ee:58:d6' \
  --scan-once \
  --read-main-once \
  --read-main-interval 30
```

## Store To Postgres

Set a local DSN outside Git, for example in your shell or private `.env`:

```bash
export LUMENTREE_BLE_POSTGRES_DSN='postgresql://USER:PASSWORD@localhost:5432/DBNAME'
python collector.py --port /dev/ttyACM0 --init-db --send STATUS --scan-once
```

The table is `lumentree_ble_events`.

## Forward To Local Lumentree Server

After starting `host/local-server/server.py`, the collector can also
POST raw BLE serial events to the local API:

```bash
export LUMENTREE_API_URL='https://lumentree.jonah.io.vn'
export LUMENTREE_API_TOKEN='change-this-to-a-long-random-token'
export LUMENTREE_DEVICE_ID='P240819130'

python collector.py \
  --port /dev/ttyACM0 \
  --send 'SET_TARGET d8:13:2a:ee:58:d6' \
  --scan-once \
  --read-main-once \
  --read-main-interval 30
```

This keeps the ESP32 in USB/evidence mode while testing the same API that Home
Assistant will later read.
