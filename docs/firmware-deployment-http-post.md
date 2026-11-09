# H910P firmware installation — HTTP/1.0 POST

**Pinned candidate:** Dell H910P custom UI v0.5.3 on original firmware 01.18.0005.

- Expected BIN name: `H910P_Custom_UI_v0.5.3.bin`
- Expected byte count: `1516340`
- Expected SHA-256: `3a5d34ddf7fa9699b044e4e4d9f96562f1b80e1167db35f0ce352d0013bfe422`
- Device IP shown below is the owner's current LAN test card. For distribution replace with the owner's device IP.
- The NMC is assumed reachable and still able to serve its original native firmware upload handler.

The original Dell firmware uploader is invoked through `POST /Forms/app_upload_1` with multipart field `file` using HTTP/1.0, `Expect:` disabled, `Connection: close`. This method **previously worked on the owner's physical card**. No physical v0.5.3 upload has yet been performed. A `302` or `303` is only a redirect, NOT firmware acceptance. A physical-card status page reporting `SUCCESS : Firmware was successfully written in FLASH` is required to establish that the image was written.

Run this command block on Linux, after saving the verified BIN into Downloads. It keeps the interactive shell open and does not write any other device settings:

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.3.bin"
EXPECTED="3a5d34ddf7fa9699b044e4e4d9f96562f1b80e1167db35f0ce352d0013bfe422"

if [ ! -f "$BIN" ]; then
  echo "Firmware BIN not found: $BIN"
elif [ "$(sha256sum "$BIN" | cut -d' ' -f1)" != "$EXPECTED" ]; then
  echo "Checksum mismatch — upload canceled."
else
  curl --http1.0 \
    --basic --user 'admin:admin' \
    --header 'Expect:' \
    --header 'Connection: close' \
    --connect-timeout 15 \
    --max-time 300 \
    --form "file=@${BIN};filename=Dell_nmc_01_18_0005.bin;type=application/octet-stream" \
    --dump-header "$HOME/Downloads/H910P-v0.5.3-upload-headers.txt" \
    --output /dev/null \
    --write-out '\nHTTP Status: %{http_code}\nBytes Sent: %{size_upload}\nElapsed: %{time_total}s\n' \
    'http://192.168.0.174/Forms/app_upload_1'
  STATUS=$?
  if [ "$STATUS" -ne 0 ]; then
    echo "Transfer failed (curl exit $STATUS). Do not assume the firmware was written."
  else
    echo "POST ended. Check http://192.168.0.174/app_upload.htm for the explicit flash result."
  fi
fi
```

After an explicit flash-write success message and reboot, verify `/index.htm`, the Dell sign-in form, native dashboard, event history and management read-only pages. **Do not issue power controls or live recipient configuration changes until the new build is verified on the physical device.**

Do not flash this image to unqualified NMC hardware or assume recovery from an unbootable image is possible. Keep previously captured v0.5.2 and factory-firmware images in user-controlled backups. An accessible native `/app_upload.htm` is the minimum post-boot recovery path, not a bootloader recovery guarantee.
