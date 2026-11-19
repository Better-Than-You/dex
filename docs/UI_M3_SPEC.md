# UI & Material Design 3 Specification (Hyprland)

## 1. Layer-Shell Window Configuration
Configure `LayerShellQt::Window::get(window)` on the root `QQuickWindow`:
- **Layer:** `LayerShellQt::Window::LayerOverlay`
- **Anchors:** `AnchorTop` (centered horizontally, `margins.top = config.ui.margin_top`)
- **Exclusive Zone:** `-1`
- **Keyboard Interactivity:**
  - `KeyboardInteractivityNone` during passive listening / status feedback (never steals focus).
  - Dynamically switch to `KeyboardInteractivityOnDemand` when `state == State.Confirming` so `Enter` confirms and `Esc` cancels.

### Hyprland layer rules

Dex's layer-shell window uses the scope `dex`. Hyprland composites blur and
shadows behind layer surfaces by default; for a rounded pill that produces a
rectangular blur/shadow halo outside the corners. Add this to
`~/.config/hypr/hyprland.conf` (or a sourced config) and reload Hyprland:

```ini
layerrule = noblur, dex
layerrule = noshadow, dex
layerrule = noanim, dex
```

The `dex` scope must match `layer->setScope("dex")` in `src/main.cpp`. The rules
remove only the compositor's outer rectangle; the Material 3 card keeps its own
surface colour and outline.

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
- **Listening:** Pill container (`radiusPill`), `"Listening…"` title, and a
  primary dot whose size/opacity follows `AppController.audioLevel` (plus a
  soft halo). Single-shot ASR produces no partial text, so the title stays
  `"Listening…"` while the user speaks. In stub mode the detail line explains
  how to inject `FINAL:<text>`.
- **Processing:** Same pill with `"Working…"` and a `BusyIndicator`; the
  finalized transcript appears as the detail line.
- **CommandExecuted:** Executed pill (`secondaryContainer` check icon, e.g.
  `Search • arch linux`) with a `"Done"` detail line.
- **MathResult:** Expression subtitle (`contentSurfaceVariant`) + large tabular
  result (`24px`, `Font.DemiBold`, `contentSurface`) + `"Copied"` badge
  (`tertiaryContainer`).
- **Confirming:** Rounded card with `?` icon (`primaryContainer`), command
  preview, M3 `Cancel Esc` outlined button and `Run Enter` filled `primary`
  button.
- **Error:** Error pill with the `errorContainer` icon well, status title and
  the provider error as detail.

> QML parses `on<Capital>` identifiers as signal handlers, so the M3
> `onSurface` / `onPrimary` / ... tokens are exposed in `Theme.qml` with a
> `content*` prefix (e.g. `onSurfaceVariant` -> `contentSurfaceVariant`).
