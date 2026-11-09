# v0.5.0 upload attempt: early HTTP 401 and connection reset

**Observed on physical H910P, 2026-11-09. Status: unresolved; NOT flashed.**

## Evidence

Owner used the same `curl --http1.0 --basic --user admin:admin` method that successfully flashed v0.4.2. For the valid v0.5.0 image, curl returned:

- `curl: (56) Recv failure: Connection reset by peer`
- `HTTP status: 401` (Unauthorized)
- `Bytes sent: 131072`
- `Elapsed: 0.081255s`

The complete v0.5.0 BIN is **1,512,695 bytes**, SHA-256 `b9d3a17c8d26267d6d9e4c678097c63911d73c219ae8d250e972e31465eca77b`. The initial 128-KiB upload represents only about 8.7% of the BIN. The NMC rejected the HTTP request before the full body was transmitted. This is **not** proof that the new firmware was accepted, validated or written.

The earlier successful 0.4.2 upload sent 1,507,862 bytes, returned HTTP 302 and took 5.707875 s. Crucially, `/app_upload.htm` then explicitly reported a successful flash and hardware rebooted. **An HTTP redirect alone never establishes success**.

## Binary investigation

Independently decompressed and compared both existing BINs offline using `h910p_rebuild.py` and `rompager_map.py`.

- Both decompressed applications are exactly 2,757,180 bytes and have 159 RomPager resources.
- The original `/app_upload.htm` RomPager descriptor, every fixed static text slot on that page, and every dynamic item pointer are byte-identical.
- `/Forms/app_upload_1`, `firmwareUploadSubmit`, and the `ERROR : HTTP transfert failure` message occur at the same offsets and have identical surrounding bytes in both images.
- Both image checksums and source build round-trip checks passed.
- **v0.5.0 has not been loaded by the card**, so modifications in that candidate cannot cause the current running server to return 401.

This strongly isolates the observed failure to the live card's HTTP authentication/access/request handling state, **not** a firmware-image checksum or the new JavaScript UI contents. It does not prove whether credentials were rejected, a request was unauthorized for this particular handler, a temporary firmware state occurred, or another access-control mechanism returned 401.

## Next safe evidence collection (NO firmware POST)

The prior command saved the HTTP response headers at `~/Downloads/H910P-v0.5.0-upload-headers.txt`. Inspect the status and any `WWW-Authenticate` challenge there (do not share credentials). Compare with the result of a read-only authenticated GET of `/app_upload.htm`. If the GET also returns 401, resolve the credential/HTTP-auth state first. If GET returns 200, it may be publicly readable, so **does not prove POST permission**; further handler-specific research is needed before another firmware upload.

Do not repeat the POST or change the BIN until the cause of the 401 is established.

## Confirmed authenticated-GET comparison

The operator supplied another GET check immediately after the failed v0.5.0 upload:

- `GET /app_upload.htm`, no credentials: **HTTP 401**, `WWW-Authenticate: Basic realm="Network Management Card :"`.
- `GET /app_upload.htm`, `--basic --user admin:admin`: **HTTP 200**.
- `POST /Forms/app_upload_1`, same credential flag: **HTTP 401** before body completion.

**Interpretation:** Credentials work for this protected *GET*. A generic incorrect username/password cannot account for the observed difference. This does not prove POST authorization or capability. Do not retry firmware flash solely on this basis.

## A specific OEM capability switch to check before further uploads

Dell's H910P manual explicitly documents **Network Settings → Firmware Upload** (default **Enabled**), with the description: authorizes remote updating of embedded firmware. Its state can be **Enabled/Disabled**; confirm whether it is enabled on this running card without making a change.

Independent inspection of the original firmware confirms the network configuration form `/set_net.htm` and a backend property named **`FirmwareUpload`**, plus distinct original strings **`ERROR : Firmware upload is not available`** and **`Configure the firmware upload setting on`**. These are credible evidence that upload permission can be separately disabled even when the firmware status page is readable.

**Evidence-based next check:** Log in and *read* `http://192.168.0.174/set_net.htm` (OEM Network Settings) to inspect `Firmware Upload`. Do **not** change IP, DHCP, credentials, or other network settings. If Disabled, this is a plausible cause to verify before any controlled change; if Enabled, investigate POST-specific authentication/access handling instead. It is **not established** that disabling it yields HTTP 401 specifically, so do not claim a confirmed root cause.

Sources: Dell Network Management Card User's Guide, 'Card Defaults' table and 'Network Settings' section (https://manualzilla.com/doc/6875280/dell-network-management-card-user-s-guide); original OEM NET+OS 01.18.0005 BIN unpacked locally, `/Forms/set_net_1` and `/set_net.htm` resource strings.

## Independent byte-level verification, 2026-11-09

Compared the OEM 01.18.0005 image with the running 0.4.2 image using their actual decompressed 2,757,180-byte applications. Exactly **28,511 application bytes differ**. **Every changed byte** is inside existing fixed-static `/index.htm` and 16 legacy `/Help/*.htm` resource content slots or those Help resources' MIME metadata. **No executable instruction bytes differ.** The ROM resource descriptors and data of `/app_upload.htm`, `/set_net.htm`, `/default.htm`, and the endpoint string `/Forms/app_upload_1` are unchanged; the persistent configuration's *runtime* state is not available in the firmware BIN and has **not** been read from the live device.

The OEM user manual specifically confirms **Network Settings → Firmware Upload: Enabled/Disabled** and that setting changes require a card restart. The high-confidence differentiator is a **read-only GET** to the existing `/set_net.htm` rendered form to inspect the actual current value before more firmware POSTs. The HTTP 401's precise branch remains unproven; do not assert the switch is off or silently enable it. Source: https://manualzilla.com/doc/6875280/dell-network-management-card-user-s-guide
