# Local 0.4.2 build when Dell CDN returns HTTP 403

The v0.4.2 source commit is `48f471f48995348fd4acfa7c757fec11feb81025`. Its builder needs the official Dell vendor ZIP at `originals/NMC_Firmware_01_18_0005_X2JYN.zip`. Dell's direct CDN URL returned **HTTP 403** on the operator's Linux machine on 2026-11-09, after cloning succeeded. This was a download failure, not a build failure. The vendor ZIP must not be redistributed through this public GitHub repository.

Use the original ZIP the owner previously downloaded, or obtain it through Dell's [official X2JYN page](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn) using a browser. Search for the ZIP under the local home directory, then verify against Dell's published SHA-256 before building:

```text
NMC_Firmware_01_18_0005_X2JYN.zip SHA256:
7190e0b41be5af5f0e26819ccc73a69e13748273ec5504685c1522b293183866

Expected custom H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin SHA256:
db0cede9c360b71a36dc68425970b8119005f170b3d9cfb7bda4eba2e7515757
```

An existing pinned checkout is located at `$HOME/h910p-build-CMM7SQ/repo` on the operator's machine. Place the verified ZIP inside that checkout's `originals/`, then run `python3 build_native.py`; verify the output digest before copying it to `~/Downloads/`.

Do not call `set -e` in the user's interactive shell; if using strict error handling, scope it to a subshell `( ... )` and capture status with `PIPESTATUS` so the terminal remains open.

**Verification:** Original ZIP digest confirmed both in the uploaded source and Dell's published driver listing; target BIN digest confirmed against the previously built v0.4.2 artifact. No new live hardware testing in this incident.
