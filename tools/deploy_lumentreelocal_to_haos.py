#!/usr/bin/env python3
"""Deploy the repo's Lumentree Local custom component into the HAOS VM.

This script treats the repo copy at `custom_components/lumentreelocal/` as the
source of truth, copies it into the running HAOS VM through `virsh console`,
backs up the currently deployed directory inside HAOS, and optionally restarts
Home Assistant Core.
"""

from __future__ import annotations

import argparse
import base64
from datetime import datetime
import io
import os
from pathlib import Path
import tarfile
import time
import urllib.error
import urllib.request

import pexpect

REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = REPO_ROOT / "custom_components" / "lumentreelocal"
HAOS_COMPONENT_DIR = Path("/mnt/data/supervisor/homeassistant/custom_components/lumentreelocal")
HAOS_BACKUP_ROOT = Path("/mnt/data/supervisor/homeassistant/.component_backups")
DEFAULT_VM_NAME = "HAOS"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vm", default=DEFAULT_VM_NAME, help="libvirt VM name")
    parser.add_argument(
        "--source",
        default=str(SOURCE_DIR),
        help="source custom component directory on the host",
    )
    parser.add_argument(
        "--skip-restart",
        action="store_true",
        help="copy files but do not restart Home Assistant Core",
    )
    parser.add_argument(
        "--ha-url",
        default=os.getenv("HA_URL", ""),
        help="optional Home Assistant base URL for post-restart verification",
    )
    parser.add_argument(
        "--ha-token",
        default=os.getenv("HA_TOKEN", ""),
        help="optional Home Assistant token for post-restart verification",
    )
    parser.add_argument(
        "--restart-timeout",
        type=int,
        default=180,
        help="seconds to wait for the HA API after restart",
    )
    return parser.parse_args()


def build_archive_bytes(source_dir: Path) -> bytes:
    if not source_dir.is_dir():
        raise SystemExit(f"source directory does not exist: {source_dir}")

    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as tar:
        for path in sorted(source_dir.rglob("*")):
            if "__pycache__" in path.parts:
                continue
            if path.name.endswith(".pyc"):
                continue
            arcname = Path("lumentreelocal") / path.relative_to(source_dir)
            tar.add(path, arcname=str(arcname), recursive=False)
    return buffer.getvalue()


def wait_for_prompt(console: pexpect.spawn) -> None:
    console.sendline("")
    console.expect([r"# ", r"homeassistant login: "], timeout=30)
    if console.after == "homeassistant login: ":
        console.sendline("root")
        console.expect(r"# ", timeout=30)


def send_root_command(console: pexpect.spawn, command: str, timeout: int = 60) -> None:
    console.sendline(command)
    console.expect(r"# ", timeout=timeout)


def deploy_archive_via_console(vm_name: str, archive_b64: str, skip_restart: bool) -> str:
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup_dir = str(HAOS_BACKUP_ROOT / f"lumentreelocal-{timestamp}")
    temp_b64 = "/tmp/lumentreelocal.tar.gz.b64"
    temp_tgz = "/tmp/lumentreelocal.tar.gz"

    console = pexpect.spawn(f"virsh console {vm_name}", encoding="utf-8", timeout=30)
    try:
        try:
            console.expect("Connected to domain", timeout=10)
        except pexpect.EOF as err:
            message = console.before or ""
            if "Active console session exists" in message:
                raise SystemExit(
                    f"cannot open virsh console for {vm_name}: an active console session already exists"
                ) from err
            raise
        wait_for_prompt(console)

        send_root_command(console, f"rm -f {temp_b64} {temp_tgz}", timeout=30)

        for chunk_start in range(0, len(archive_b64), 300):
            chunk = archive_b64[chunk_start : chunk_start + 300]
            send_root_command(console, f"printf '%s\\n' '{chunk}' >> {temp_b64}", timeout=30)

        send_root_command(console, f"base64 -d {temp_b64} > {temp_tgz}", timeout=60)
        send_root_command(console, f"mkdir -p {HAOS_COMPONENT_DIR.parent}", timeout=30)
        send_root_command(console, f"mkdir -p {HAOS_BACKUP_ROOT}", timeout=30)
        send_root_command(
            console,
            (
                f"if [ -d {HAOS_COMPONENT_DIR} ]; then "
                f"rm -rf {backup_dir} && cp -a {HAOS_COMPONENT_DIR} {backup_dir}; "
                "fi"
            ),
            timeout=60,
        )
        send_root_command(
            console,
            f"rm -rf {HAOS_COMPONENT_DIR} && tar -xzf {temp_tgz} -C {HAOS_COMPONENT_DIR.parent}",
            timeout=60,
        )
        send_root_command(console, f"test -f {HAOS_COMPONENT_DIR}/manifest.json", timeout=30)

        if not skip_restart:
            console.sendline("ha core restart")
            console.expect(r"Processing", timeout=20)

        return backup_dir
    finally:
        try:
            console.sendcontrol("]")
            console.expect("virsh #", timeout=5)
            console.sendline("quit")
        except Exception:
            pass
        console.close(force=True)


def wait_for_ha_api(ha_url: str, ha_token: str, timeout_seconds: int) -> bool:
    if not ha_url:
        return False

    headers = {}
    if ha_token:
        headers["Authorization"] = f"Bearer {ha_token}"
    request = urllib.request.Request(f"{ha_url.rstrip('/')}/api/", headers=headers)
    deadline = time.time() + timeout_seconds

    while time.time() < deadline:
        try:
            with urllib.request.urlopen(request, timeout=10) as response:
                if response.status == 200:
                    return True
        except (urllib.error.URLError, TimeoutError):
            pass
        time.sleep(5)
    return False


def main() -> int:
    args = parse_args()
    source_dir = Path(args.source).resolve()
    archive_bytes = build_archive_bytes(source_dir)
    archive_b64 = base64.b64encode(archive_bytes).decode("ascii")

    print(f"Deploying {source_dir} to VM {args.vm}")
    backup_dir = deploy_archive_via_console(args.vm, archive_b64, args.skip_restart)
    print(f"Created HAOS backup: {backup_dir}")

    if args.skip_restart:
        print("Skipped Home Assistant restart")
        return 0

    if args.ha_url:
        healthy = wait_for_ha_api(args.ha_url, args.ha_token, args.restart_timeout)
        if not healthy:
            print("Home Assistant API did not come back within the timeout", flush=True)
            return 1
        print("Home Assistant API is back online")
    else:
        print("Restart requested without HA API verification")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
