# Dex Material 3 & Hyprland Layer-Shell Specification

## 1. LayerShellQt Configuration (`main.cpp`)
Attach `LayerShellQt::Window::get(window)` to the root `QQuickWindow`:
- Layer: `LayerShellQt::Window::LayerOverlay`
- Anchors: `LayerShellQt::Window::AnchorTop` (horizontally centered, `margins.top = config.ui.margin_top`)
- Exclusive Zone: `-1`
- Keyboard Interactivity:
  - `KeyboardInteractivityNone` when passive/listening (never steals focus).
  - Switch dynamically to `KeyboardInteractivityOnDemand` when `state == AppController::Confirming` so `Enter` confirms and `Esc` cancels.

## 2. M3 Dark Tonal Tokens (`qml/Theme.qml` Singleton)
> QML reserves `on<Capital>` names for signal handlers. Theme exposes the M3
> content tokens with a `content*` prefix: `onSurface` -> `contentSurface`,
> `onSurfaceVariant` -> `contentSurfaceVariant`, `onPrimary` -> `contentPrimary`,
> etc. Values are unchanged.

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
- Radii: `radiusPill: 28`, `radiusCard: 20`, `radiusChip: 12`

## 3. UI States
- `Listening`: Pill container (`radiusPill`), primary dot whose size/opacity follows `AppController.audioLevel` (plus a soft halo/pulse), title `Listening…`. Single-shot Parakeet emits no partials, so no streaming text appears; stub mode explains how to inject `FINAL:<text>` in the detail line.
- `Processing`: Same pill with `Working…` and a `BusyIndicator`; the finalized transcript is the detail line.
- `CommandExecuted`: Executed pill (`secondaryContainer` check icon, e.g. `Search • arch linux`) with a `Done` detail line.
- `MathResult`: Expression subtitle (`contentSurfaceVariant`) + 24px `Font.DemiBold` result (`contentSurface`) + `"Copied"` badge (`tertiaryContainer`).
- `Confirming`: Rounded card with `?` icon (`primaryContainer`), action preview, and M3 `Cancel Esc` outlined / `Run Enter` filled `primary` buttons.
- `Error`: Error pill (`errorContainer` icon well) with status title and provider error detail.
