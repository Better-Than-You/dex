pragma Singleton

import QtQuick

/// Material Design 3 dark tonal palette for Dex.
///
/// QML parses `on<Capital>` identifiers as signal handlers, so the M3
/// `onSurface` / `onPrimary` / ... content tokens are exposed with a
/// `content*` prefix (e.g. `onSurfaceVariant` -> `contentSurfaceVariant`).
QtObject {
    // Surfaces
    readonly property color surface: "#131419"
    readonly property color surfaceContainerLow: "#191A20"
    readonly property color surfaceContainer: "#1B1C22"
    readonly property color surfaceContainerHigh: "#1E1F25"
    readonly property color surfaceContainerHighest: "#282A31"

    // Content
    readonly property color contentSurface: "#E2E2E9"
    readonly property color contentSurfaceVariant: "#C4C6D0"
    readonly property color outline: "#8E9099"
    readonly property color outlineVariant: "#44474E"

    // Primary
    readonly property color primary: "#ADC6FF"
    readonly property color contentPrimary: "#102F60"
    readonly property color primaryContainer: "#2B4678"
    readonly property color contentPrimaryContainer: "#DBE2F9"

    // Secondary
    readonly property color secondaryContainer: "#3E4759"
    readonly property color contentSecondaryContainer: "#DBE2F9"

    // Tertiary
    readonly property color tertiaryContainer: "#573E5C"
    readonly property color contentTertiaryContainer: "#FAD8FD"

    // Error
    readonly property color error: "#FFB4AB"
    readonly property color contentError: "#690005"
    readonly property color errorContainer: "#93000A"
    readonly property color contentErrorContainer: "#FFDAD6"

    // Shape tokens
    readonly property int radiusPill: 28
    readonly property int radiusCard: 20
    readonly property int radiusChip: 12
    readonly property int radiusButton: 20

    // Spacing
    readonly property int spacingXs: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 12
    readonly property int spacingL: 16
    readonly property int spacingXl: 24

    // Type scale
    readonly property int fontLarge: 24
    readonly property int fontTitle: 16
    readonly property int fontBody: 14
    readonly property int fontLabel: 12

    // Motion
    readonly property int durationFast: 120
    readonly property int durationMedium: 180
    readonly property int durationSlow: 260
}
