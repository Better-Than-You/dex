# UI & Material Design 3 Specification (Hyprland)

## 1. Layer-Shell Window Configuration
Configure `LayerShellQt::Window::get(window)` on the root `QQuickWindow`:
- **Layer:** `LayerShellQt::Window::LayerOverlay`
- **Anchors:** `AnchorTop` (centered horizontally, `margins.top = config.ui.margin_top`)
- **Exclusive Zone:** `-1`
- **Keyboard Interactivity:**
  - `KeyboardInteractivityNone` during passive listening / status feedback (never steals focus).
  - Dynamically switch to `KeyboardInteractivityOnDemand` when `state == State.Confirming` so `Enter` confirms and `Esc` cancels.

## 2. Material Design 3 Tokens (`qml/Theme.qml`)
`pragma Singleton` `QtObject` with dark M3 tonal palette (compatible with Caelestia dark surfaces):
- `surfaceContainerHigh`: `"#1E1F25"`
- `surfaceContainerHighest`: `"#282A31"`
- `onSurface`: `"#E2E2E9"`
- `onSurfaceVariant`: `"#C4C6D0"`
- `primary`: `"#ADC6FF"`
- `onPrimary`: `"#102F60"`
- `secondaryContainer`: `"#3E4759"`
- `onSecondaryContainer`: `"#DBE2F9"`
- `tertiaryContainer`: `"#573E5C"`
- `onTertiaryContainer`: `"#FAD8FD"`
- `error`: `"#FFB4AB"`
- `outlineVariant`: `"#44474E"`
- Shape tokens: `radiusPill: 28`, `radiusCard: 20`, `radiusChip: 12`

## 3. QML Visual States
- **Listening:** Pill container (`radiusPill`), primary dot whose size/opacity follows `AppController.audioLevel` (plus a soft halo), live streaming `partialText` (`contentSurfaceVariant`). In stub mode the detail line explains how to inject `FINAL:<text>`.
- **CommandExecuted:** Action chip (`secondaryContainer`, e.g., `Search • arch linux`) + checkmark icon.
- **MathResult:** Expression subtitle (`contentSurfaceVariant`) + large tabular result (`24px`, `Font.DemiBold`, `contentSurface`) + `"Copied"` badge (`tertiaryContainer`).
- **Confirming:** Command preview + M3 `Cancel [Esc]` outlined button and `Run [Enter]` filled `primary` button.

> QML parses `on<Capital>` identifiers as signal handlers, so the M3
> `onSurface` / `onPrimary` / ... tokens are exposed in `Theme.qml` with a
> `content*` prefix (e.g. `onSurfaceVariant` -> `contentSurfaceVariant`).
