# H910P v0.4.2 upload rejection — protocol isolation

On 2026-11-09, the owner confirmed **v0.4.2** failed with the *same* on-device error `ERROR : HTTP transfert failure` as v0.4.1.

## Verified

- v0.4.1: 1,533,558 bytes; vendor original: 1,517,029 bytes; v0.4.2: **1,507,635 bytes** (9,394 bytes under OEM). The size overage of 0.4.1 is therefore **not sufficient to explain** the repeat failure.
- The original firmware exposes `/Forms/app_upload_1` and the multipart form name `file` as separate fixed strings.
- The original ARM big-endian application can be disassembled with LLVM after wrapping its decompressed bytes in a correctly addressed ELF32 ARM envelope at `0x08004000`.
- The native form callback appears at `0x08035cac` in the original firmware; it stores a transfer state and redirects to the original upload page. The event/flash-state logic is elsewhere. The browser's previously observed 303 redirect is **not proof of accepted flash contents**.
- The error string is embedded in the OEM firmware at decompressed offset `0x001f4da0`, as distinct from `File CRC was not correct`, `File format was not correct`, and `Write firmware in FLASH failure`.
- An isolated, loopback-only HTTP mock accepted `curl --http1.0 --basic --user admin:admin -H 'Expect:' -H 'Connection: close' -F 'file=@...;filename=Dell_nmc_01_18_0005.bin;type=application/octet-stream'` and verified that the request was HTTP/1.0, included `multipart/form-data; boundary=...`, contained the complete v0.4.2 binary, and returned HTTP 303 without following redirect. **This confirms request construction only, not success against actual hardware.**

## Controlled next test

Try the v0.4.2 image using a single HTTP/1.0 multipart POST without `Expect: 100-continue`, without keep-alive, and without automatic redirect-following. This distinguishes browser-specific transfer behavior from an NMC-side upload failure. Record HTTP status and reload the card's `/app_upload.htm` page to inspect the actual result. A 303 still does not confirm success. No speculative new firmware build is recommended before this result.

The Dell 01.18.0005 release note states the firmware BIN is uploaded using the card's built-in web Firmware Upgrade page. Its 01.03.0006 changelog also mentions an NMC reset when `iGotoFirmwareUploadMode` returns errors, indicating a distinct upload-mode transition. No evidence proves the encountered error's exact underlying cause yet.

**Do not claim solved:** neither the HTTP/1.0 test nor a vendor-original control upload has been tried against the actual H910P. Do not use the results of the local mock to assert device acceptance.
