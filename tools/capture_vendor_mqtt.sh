#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage:
  tools/capture_vendor_mqtt.sh DEVICE_ID [OUTPUT_FILE]

Read-only capture for Lumentree vendor MQTT traffic.
It subscribes to reportApp/DEVICE_ID and listenApp/DEVICE_ID only.
It never publishes a command.

Environment overrides:
  LUMENTREE_VENDOR_MQTT_HOST       default: lesvr.suntcn.com
  LUMENTREE_VENDOR_MQTT_PORT       default: 1886
  LUMENTREE_VENDOR_MQTT_USERNAME   default: appuser
  LUMENTREE_VENDOR_MQTT_PASSWORD   default: app666
EOF
}

device_id="${1:-}"
if [[ -z "${device_id}" || "${device_id}" == "-h" || "${device_id}" == "--help" ]]; then
  usage
  exit 2
fi

output_file="${2:-docs/evidence/vendor-mqtt-captures/$(date +%Y%m%d-%H%M%S)-${device_id}.log}"
host="${LUMENTREE_VENDOR_MQTT_HOST:-lesvr.suntcn.com}"
port="${LUMENTREE_VENDOR_MQTT_PORT:-1886}"
username="${LUMENTREE_VENDOR_MQTT_USERNAME:-appuser}"
password="${LUMENTREE_VENDOR_MQTT_PASSWORD:-app666}"

mkdir -p "$(dirname "${output_file}")"

echo "# read-only vendor MQTT capture"
echo "# device_id=${device_id}"
echo "# host=${host}"
echo "# topics=reportApp/${device_id},listenApp/${device_id}"
echo "# output=${output_file}"
echo "# started_at=$(date --iso-8601=seconds)"

mosquitto_sub \
  -h "${host}" \
  -p "${port}" \
  -u "${username}" \
  -P "${password}" \
  -t "reportApp/${device_id}" \
  -t "listenApp/${device_id}" \
  -F '%I %t %x' \
  | tee -a "${output_file}"
