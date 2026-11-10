# Dell H910P v0.5.7 firmware installation — Linux

**Experimental firmware.** Latest confirmed working user installation remains v0.5.6. This candidate has not been flashed on physical H910P hardware in this pass.

## Firmware identity

- File: `H910P_Custom_UI_v0.5.7.bin`
- Bytes: 1,516,937
- SHA-256: `3cd000b680868d960b8fdbf3c897be188ba2cd4f3784b3ad69b5d47f736dfdb6`
- Base Dell firmware: 01.18.0005

Do not flash onto critical equipment without an accepted recovery procedure. An HTTP 200 or 302 after upload is not proof the firmware is written or that the UI starts. Allow the card to finish writing and restart before browsing the UI.

## Operator-controlled HTTP/1.0 command block

The example targets the known test NMC at `192.168.0.174`. It prompts for the configured NMC password and does not silently suppress the firmware endpoint response code.

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.7.bin"
EXPECTED="3cd000b680868d960b8fdbf3c897be188ba2cd4f3784b3ad69b5d47f736dfdb6"
NMC="192.168.0.174"

printf '%s  %s\n' "$EXPECTED" "$BIN" | sha256sum -c - &&
test "$(wc -c < "$BIN")" -eq 1516937 &&
curl --http1.0 \
  --basic \
  --user admin \
  --header 'Expect:' \
  --header 'Connection: close' \
  --connect-timeout 15 \
  --max-time 300 \
  --show-error \
  --form "file=@${BIN};filename=Dell_nmc_01_18_0005.bin;type=application/octet-stream" \
  --dump-header "$HOME/Downloads/H910P-v0.5.7-upload-headers.txt" \
  --output "$HOME/Downloads/H910P-v0.5.7-upload-response.html" \
  --write-out '\nHTTP Status: %{http_code}\nBytes Sent: %{size_upload}\nElapsed: %{time_total}s\n' \
  "http://${NMC}/Forms/app_upload_1"
```

After the card processes the image, open `http://192.168.0.174/app_upload.htm` to look for the **literal Dell success message** `SUCCESS : Firmware was successfully written in FLASH`. Then open `http://192.168.0.174/default.htm` in a private browser window and verify dashboard, login and live UPS values. Check `index.htm?view=network` and `default.htm?view=notification` as deep-link examples.

Do not equate the browser's first login or missing authorization dialog with credential safety. This card uses HTTP Basic authentication; deploying the UI on a plain HTTP management network exposes credentials to network observers. Use trusted management LAN/VPN restrictions.

## Read-only post-flash verification

```bash
python3 tools/probe_live_nmc.py \
  --nmc http://192.168.0.174 \
  --firmware ./H910P_Custom_UI_v0.5.7.bin \
  --manifest build/VALIDATION.json \
  --user admin \
  --output ./h910p-v0.5.7-verification.json
```

This probe downloads selected static resources for comparison; it never submits UPS power or configuration writes. The diagnostic does not substitute for visual boot and management tests.
