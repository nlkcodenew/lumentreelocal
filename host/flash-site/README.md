# Lumentree Flash Site

Static WebSerial flash site for the dedicated ESP32-S3 Lumentree firmware.

Default local bind:

```text
http://127.0.0.1:8790
```

Run manually:

```bash
cd /home/mrlinh/esp32-lumentree
python3 host/flash-site/server.py --host 127.0.0.1 --port 8790
```

The site serves files from:

```text
host/flash-site/public
```

Current UX requirements for the public page:

1. Default language is Vietnamese.
2. Provide a direct Vietnamese/English toggle on the page.
3. The "After Flash" flow must cover both:
   - initial provisioning through `http://192.168.4.1`
   - write access pairing through `Write Access -> Generate write pairing code`
4. The page must explain that Home Assistant write entities stay read-only
   until the user enters the pairing code in `Lumentree Local` integration
   options.

Refresh firmware artifacts after a new build:

```bash
cd /home/mrlinh/esp32-lumentree
./tools/sync_flash_site_artifacts.sh
```

## Firmware Version Sync

Source of truth for the public flash firmware version:

```text
firmware/platformio.ini
  -> [env:esp32-s3-8mb-nopsram-release]
  -> LUMENTREE_FIRMWARE_VERSION
```

The flash site does not keep its own separate version string. The sync script
extracts the version from the selected PlatformIO environment and writes it into
`host/flash-site/public/firmware/flash-manifest.json`.

Public firmware release workflow:

1. Update `LUMENTREE_FIRMWARE_VERSION` inside
   `firmware/platformio.ini` for `env:esp32-s3-8mb-nopsram-release`.
2. Build the release firmware:

   ```bash
   cd /home/mrlinh/esp32-lumentree/firmware
   pio run -e esp32-s3-8mb-nopsram-release
   ```

3. Flash and validate on a real ESP32-S3 device before publish.
4. Sync website artifacts:

   ```bash
   cd /home/mrlinh/esp32-lumentree
   ./tools/sync_flash_site_artifacts.sh
   ```

5. Verify the public manifest:

   ```bash
   curl https://flash-lumentree.jonah.io.vn/firmware/flash-manifest.json
   ```

6. Confirm production health from the tested device reports the same firmware
   version.
