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
- `Listening`: Pill container (`radiusPill`), primary mic indicator, live streaming `partialText` (`onSurfaceVariant`).
- `CommandExecuted`: Action chip (`secondaryContainer`, e.g. `Search • arch linux`) + checkmark icon.
- `MathResult`: Expression (`onSurfaceVariant`) + 24px `Font.DemiBold` result (`onSurface`) + `"Copied"` badge (`tertiaryContainer`).
- `Confirming`: Action preview + M3 `Cancel [Esc]` outlined button and `Run [Enter]` filled `primary` button.
