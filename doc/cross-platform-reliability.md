# Reliable macOS and Windows control

Input Leap is designed to keep the pointer, keyboard, clipboard, and file
transfers predictable when moving between computers with different display
scales and operating-system conventions.

## Recommended setup

1. Select **Control other computers** on the computer with the physical mouse
   and keyboard.
2. Open **Arrange Screens & Controls** and place the other computer on the
   correct edge.
3. Keep **Edge stability distance** at its default of 8 px. Increase it when a
   high-DPI display or trackpad causes accidental bounce-back at the boundary.
4. Open the remote computer's screen settings. Map Ctrl and Meta when you want
   macOS Command shortcuts to follow the same physical key on Windows. Enable
   **Reverse scrolling on this computer** only when the two computers use
   different natural-scrolling preferences.
5. Leave encryption enabled and use **Test Connection** before starting.

The handoff maps the pointer by its fractional position along the shared edge,
clamps it inside the destination display, and briefly rejects motion back
through the edge. This avoids oscillation and corner jumps across Retina,
scaled Windows, and mixed-orientation displays.

## Recovery behavior

When Wi-Fi changes or a computer wakes from sleep, the controlled computer
reconnects using bounded exponential backoff. Only one retry is scheduled at a
time. On disconnect, Input Leap releases every synthetic key and mouse button
so a failed connection cannot leave dragging or a modifier stuck.

The dashboard distinguishes connecting, reconnecting, connected, transferring,
disconnected, and actionable error states. On macOS, **Open Permissions** takes
you directly to the Accessibility settings required for input control.

## Troubleshooting

- Use **Test Connection** to distinguish an unreachable address from a TLS or
  pairing problem.
- On macOS, verify Accessibility and Input Monitoring access after replacing or
  moving the app.
- Use **Save Diagnostics** to create a report containing platform and connection
  state plus sanitized logs. Home-directory paths, IP addresses, and certificate
  fingerprints are removed.
- If switching feels too sensitive, increase **Edge stability distance** in
  **Arrange Screens & Controls**. If switching feels deliberate or slow, check
  the existing switch-delay and double-tap options.

## Release integrity

Release downloads include SHA-256 checksums. Official release automation can
code-sign the Windows installer and sign/notarize the macOS disk image when the
maintainer signing secrets described in [RELEASING.md](../RELEASING.md) are
configured.
