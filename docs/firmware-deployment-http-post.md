# Firmware deployment — successful H910P HTTP/1.0 POST method

**Approved manual firmware-update workflow.** Successfully confirmed on physical H910P with v0.4.2, November 9, 2026. v0.5.0 is a new offline-tested firmware candidate; do not label it hardware-proven until the card actually accepts, reboots and serves it.

The tested request used an authenticated HTTP/1.0 multipart POST to `/Forms/app_upload_1`, form field `file`, octet-stream MIME, standard Dell `.bin` filename, disabled `Expect:`, `Connection: close`, and **no redirect-following**. The NMC responded 302, but actual positive evidence came from the separate status page:

> SUCCESS : Firmware was successfully written in FLASH.
> Restart in progress. Wait a few minutes and click on Refresh button.

The card then rebooted and served `/index.htm` for v0.4.2. Previous stock browser HTTP/1.1 uploads returned `ERROR : HTTP transfert failure`; the protocol-level cause has not been isolated.

## Exact v0.5.0 candidate

- Filename: `H910P_Custom_UI_v0.5.0.bin`
- SHA-256: `b9d3a17c8d26267d6d9e4c678097c63911d73c219ae8d250e972e31465eca77b`
- Size: 1,512,695 bytes
- Dell firmware version unchanged: `01.18.0005`
- Device used in previous proof: `192.168.0.174`, H910P.

For the owner, this is the exact command pattern; it first checks the expected image hash and file length:

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.0.bin"
EXPECTED="b9d3a17c8d26267d6d9e4c678097c63911d73c219ae8d250e972e31465eca77b"
if [ ! -f "$BIN" ]; then
  printf 'Firmware file not found: %s\n' "$BIN"
elif [ "$(sha256sum "$BIN" | cut -d' ' -f1)" != "$EXPECTED" ]; then
  echo 'Firmware checksum mismatch. Upload canceled.'
elif [ "$(stat -c %s "$BIN")" -ne 1512695 ]; then
  echo 'Unexpected firmware size. Upload canceled.'
else
  curl --http1.0 --basic --user 'admin:admin' \
    --header 'Expect:' --header 'Connection: close' \
    --connect-timeout 15 --max-time 300 \
    --form "file=@${BIN};filename=Dell_nmc_01_18_0005.bin;type=application/octet-stream" \
    --dump-header "$HOME/Downloads/H910P-v0.5.0-upload-headers.txt" \
    --output /dev/null \
    --write-out '\nHTTP status: %{http_code}\nBytes sent: %{size_upload}\nElapsed: %{time_total}s\n' \
    'http://192.168.0.174/Forms/app_upload_1'
fi
```

**A 302/303 redirect alone does not mean success.** Open `http://192.168.0.174/app_upload.htm` and look for **`SUCCESS : Firmware was successfully written in FLASH`**, then after restart confirm `/index.htm` and correct UPS telemetry. Do not flash a second image while the NMC is still restarting or writing. Do not attempt a browser-based HTTP/1.1 upload as a substitute for this verified request without a separate test.

This is not Option A (future GitHub UI-only updater). Full-firmware POST stays the working manual deployment method until that implementation is tested. Recovery using this handler is only possible while the NMC boots and remains reachable.
