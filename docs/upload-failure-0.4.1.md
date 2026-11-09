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
