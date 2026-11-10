# H910P v0.5.2 engineering qualification (2026-11-09)

**Change scope:** targeted correction to authentication and login backdrop; no changes to OEM executable instructions, UPS event handlers, firmware POST handler, or native device authentication. Base Dell firmware 01.18.0005.

## Reproduced defect

In 0.5.1 the browser sign-in code sent `fetch('/app_upload.htm',{credentials:'omit',headers:{Authorization:basic}})`. The Fetch `omit` mode forbids inclusion of HTTP authentication credentials, so the code could wrongly deny a valid user at the OEM protected endpoint. The offline HTML demo also had a separate flaw: it only allowed `admin/admin`, which is not the device login.

The login page stylesheet also used a two-tone linear-gradient with a 49-pixel contrasting stripe at the bottom, visually suggesting the background stopped short. The root page now uses the same solid background as a viewport-filling sign-in panel.

## Changes

- Use `credentials:'same-origin'` on the Dell authenticated GET; keep the explicitly constructed HTTP Basic authorization value and downstream same-origin NMC requests.
- Use a `div` for the nested sign-in surface (the outer static page already contains `main#app`).
- Full viewport background: `100vh` fallback + `100dvh`, consistent background on the document root and sign-in region, no stripe.
- Test the actual packaged entry and compressor, both browser suites, credential-mode failure/success, geometry, and semantic markup.
- Separate reproducible offline demo builder with a prominent simulation indicator. Any nonempty demo credentials work, without a network connection; it is **not** a bypass in production code.

## Artifact

- File: `H910P_Custom_UI_v0.5.2.bin`
- Size: **1,515,865 bytes** (factory **1,517,029 bytes**, saving 1,164 bytes)
- SHA-256: `e45b92abbd02555818332a0b4638fe8cfe440dd1d25a9727bd6c68271904167a`
- Base image firmware version: 01.18.0005
- Source: public GitHub `dev` branch; Dell proprietary firmware inputs excluded.

## Verification and limitations

- Firmware size guard, CRC, decompression round-trip, resource allowlist and callback descriptor invariants: **passed**.
- 18 Python firmware-format regressions: **passed**.
- 4 optimal compressor regressions and 4 native-entry tests: **passed**.
- 15-route browser suite, invalid login / valid login simulation, full-height backdrop, responsive fallback: **passed with simulated device responses**.
- Configuration/recipient modal and typed power-confirmation suite: **passed with simulated device responses**.
- Offline preview in headless Chromium: **passed at 1440x900 and 390x844 with non-default demo credentials**.
- No test against a real H910P was available. **Do not claim that login on real hardware works until the user confirms.**
- No new GitHub UI-only updater or native Monita/Discord/ntfy transport added. This is a deliberately targeted correction.
