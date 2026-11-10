# H910P browser authentication

The device uses HTTP Basic authentication with a `WWW-Authenticate: Basic` response challenge. When Chromium receives a 401 from a same-origin request that opts into the browser's credential mode, it may present its own native username/password prompt. The custom login form was checking `/app_upload.htm` with `credentials:'same-origin'`, which allowed this behavior: a native dialog appeared over the custom login page and the JavaScript request eventually timed out.

v0.5.9 supplies the user's Basic Authorization header directly and uses `credentials:'omit'` for NMC API requests. As described in the Fetch Standard's HTTP-network-or-cache fetch algorithm, a 401 invokes the browser prompt only when `includeCredentials` is true. The form still treats HTTP 401/403 as rejected credentials and retains the card's native authorization boundary.

The NMC's HTTP interface currently lacks a tested nonchallenging session/authentication API. This change does not replace HTTP Basic with server sessions and does not encrypt the card's HTTP transport. Because the device's live behavior was not emulated during this build, all browser dialog tests are limited to code inspection and mocked transport; verify against a test H910P before qualifying the release.

Sources:
- https://fetch.spec.whatwg.org/
- https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/WWW-Authenticate
