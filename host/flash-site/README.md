# Lumentree Flash Site

Static WebSerial flash site for the Lumentree firmware line.

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
5. The page must expose two board choices:
   - `ESP32-S3` stable
   - `ESP32-C3 Super Mini` experimental

Refresh firmware artifacts after a new build:

```bash
cd /home/mrlinh/esp32-lumentree
./tools/sync_flash_site_artifacts.sh
```

## Firmware Version Sync

Source of truth for the public flash firmware versions:

```text
firmware/platformio.ini
  -> [env:esp32-s3-8mb-nopsram-release]
  -> LUMENTREE_FIRMWARE_VERSION
firmware/platformio.ini
  -> [env:esp32-c3-4mb-experimental]
  -> LUMENTREE_FIRMWARE_VERSION
```

The flash site does not keep its own separate version string. The sync script
extracts the version from the selected PlatformIO environments and writes them
into:

- `host/flash-site/public/firmware/esp32-s3/flash-manifest.json`
- `host/flash-site/public/firmware/esp32-c3/flash-manifest.json`
- `host/flash-site/public/firmware/flash-manifest.json`

For backward compatibility, the legacy flat files under
`host/flash-site/public/firmware/` are also kept aligned with the stable
`ESP32-S3` line.

Public firmware release workflow:

1. Update `LUMENTREE_FIRMWARE_VERSION` inside
   `firmware/platformio.ini` for `env:esp32-s3-8mb-nopsram-release`.
2. Build the release firmware(s):

   ```bash
   cd /home/mrlinh/esp32-lumentree/firmware
   pio run -e esp32-s3-8mb-nopsram-release
   pio run -e esp32-c3-4mb-experimental
   ```

3. Flash and validate on real hardware before publish.
   - `ESP32-S3` remains the stable line.
   - `ESP32-C3 Super Mini` must stay labeled experimental unless it has passed
     real runtime validation.
4. Sync website artifacts:

   ```bash
   cd /home/mrlinh/esp32-lumentree
   ./tools/sync_flash_site_artifacts.sh
   ```

5. Verify the public manifest(s):

   ```bash
   curl https://flash-lumentree.jonah.io.vn/firmware/flash-manifest.json
   curl https://flash-lumentree.jonah.io.vn/firmware/esp32-s3/flash-manifest.json
   curl https://flash-lumentree.jonah.io.vn/firmware/esp32-c3/flash-manifest.json
   ```

6. Confirm production health from the tested device reports the same firmware
   version.
