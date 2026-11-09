# H910P 0.4.1 upload failure — offline forensic diagnosis

**Observed on 2026-11-09:** Operator attempted to upload `H910P-v0.4.1-custom-UI.bin` through the stock NMC web interface; interface displayed `ERROR : HTTP transfert failure`. No proof image transfer completed or flash write started.

## Verified firmware-level findings

- Exact English error text is present verbatim in the **unmodified** Dell `01.18.0005` decompressed application near offset `0x1F4E00` (run a search to compute exact offset).
- Dell's firmware supplies **different** errors for `ERROR : File name not correct (must be *.bin)`, `ERROR : File format was not correct`, `ERROR : File CRC was not correct`, `ERROR : Write firmware in FLASH failure`, and `ERROR : HTTP transfert failure`.
- Firmware contains an original upload endpoint `/Forms/app_upload_1`, corresponding page `/app_upload.htm`, multipart field `file`, and `firmwareUploadSubmit()` function diagnostics.
- Original BIN is 1,517,029 bytes. Modified v0.4.1 BIN is 1,533,558 bytes (+16,529). Both have a valid NET+OS header and independently recomputed CRC; offline decoder round-trips both.
- The failure cannot be conclusively attributed to version numbering, byte size, CRC, file format, HTTP browser compatibility, or firmware acceptance using the error string alone. Those are different hypotheses.

## Next evidence needed

Inspect the **single actual POST** to `/Forms/app_upload_1` in browser Developer Tools > Network while selecting the modified BIN. Record HTTP status, whether the request completed or was canceled, upload size, elapsed time and response text/error. Avoid sharing complete HAR/cookies/authorization headers or firmware POST body. When the request fails without an HTTP response, a browser network error code and NMC system log at the same time are useful.

Do **not** begin by changing the image's version or CRC: that would not be evidence-driven. If the upload actually completes and card returns a specific image-format/CRC/flash error, address that corresponding stage. If the request is interrupted or exceeds an HTTP server limit, investigate the NMC's transport and payload size separately.

Vendor references: [Dell 01.18.0005 download](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn); [Dell NMC manual](https://usermanual.wiki/Dell/1508079828delllineinteractiverackups1920rusersguide14enus.2134937042.pdf) (firmware upload process and separate transfer / flash stages).

## Operator browser evidence (2026-11-09)

- Request: `POST http://192.168.0.174/Forms/app_upload_1`.
- Response: `303 See Other`, `Location: http://192.168.0.174/app_upload.htm`, `Content-Length: 0`, `Server: Allegro-Software-RomPager/4.01`.
- Browser request `Content-Length: 1533873`. Custom BIN has exactly 1,533,558 bytes; 315-byte request overhead is consistent with multipart form packaging. The actual request `Content-Type` value was omitted from the pasted capture and still needs confirmation.
- Therefore the browser reached the native upload handler and received its redirect. The 303 by itself is *not* success, does not prove all body bytes were consumed, and does not prove any flash write started.
- Error on redirected page: `ERROR : HTTP transfert failure`.
- No `File format was not correct`, `File CRC was not correct`, or `Write firmware in FLASH failure` error was observed. This does not rule out an internal size/transport limit.
- No supported root-cause conclusion or corrected BIN yet. Further evidence: DevTools POST Timing/Content-Type and redirect destination response, omitting authorization credentials.
- **Security:** Operator's pasted Authorization header contained reversible Basic credentials. Avoid recording raw authorization headers in tickets or repository; rotate the NMC administrator password if that credential is still used.
