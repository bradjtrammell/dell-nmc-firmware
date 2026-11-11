# OpenH910P v0.6 visual identity

**Personality:** measured, utilitarian, confident. **Reference:** an early-1970s electrical field instrument and printed maintenance ledger.

- **Mode:** fixed light; no automatic dark mode.
- **Tokens:** paper `#f4f0e5`, rail `#eae5d7`, panel `#fffdf8`, spruce `#163b31`, ink `#23352c`, oxide `#a3462e`, amber `#c08c37`, muted `#626f65`, fine-line `#d5d0c1`, focus `#a3462e`.
- **Type:** Iowan Old Style/Baskerville/Georgia for numerical readouts and headings; Aptos/Segoe UI/system for body and controls; SFMono/Consolas/Liberation Mono for firmware and diagnostics.
- **Layout:** unboxed instrument-ledger content with a permanent narrow left rail on desktop, horizontal scrollable index navigation on mobile, editorial system heading and a single deep-spruce measurement field.
- **Navigation:** five in-page semantic sections; a visible active section and bypass link.
- **Shape/depth:** square cuts, hairline rules and no shadows, pill badges or decorative gradients.
- **Imagery:** original abstract logo only, no OEM Dell mark or stock illustration.
- **Motion:** 180ms color changes, 200ms meter fill; reduced-motion preference overrides all animation.
- **Signature:** oversized serif battery measurement on a forest-green operational plate beside four power indicators.
- **Safety:** visible permanent simulator warning, disabled control actions and zero implication that data comes from an attached UPS.

The interface runs entirely on locally served HTML/CSS/JS and the native JSON API. `fetch()` never calls Dell XML or CGI paths. WCAG color contrast, semantic tables and keyboard focus are design requirements; automated a11y validation remains to be run with a browser test harness.
