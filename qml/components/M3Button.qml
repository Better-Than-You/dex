import QtQuick
import QtQuick.Layouts
import Dex

/// Material 3 action button. The Material Symbols glyph and the Rubik label
/// share one RowLayout so their vertical centres are derived from the same
/// geometry instead of font ascent/descent.
Item {
    id: root

    property string text: ""
    property string icon: ""
    property string variant: "filled" // filled | tonal | outlined | text
    property int iconSize: Theme.fontIcon
    property int labelSize: Theme.fontBody

    signal clicked()

    implicitWidth: contentRow.implicitWidth + Theme.spacingL * 2
    implicitHeight: contentRow.implicitHeight + Theme.spacingS * 2
    opacity: enabled ? 1.0 : 0.38

    function backgroundColor() {
        switch (variant) {
        case "filled":
            return Theme.primary
        case "tonal":
            return Theme.secondaryContainer
        default:
            return "transparent"
        }
    }

    function foregroundColor() {
        switch (variant) {
        case "filled":
            return Theme.contentPrimary
        case "tonal":
            return Theme.contentSecondaryContainer
        default:
            return Theme.contentSurfaceVariant
        }
    }

    Rectangle {
        id: background

        anchors.fill: parent
        radius: Theme.radiusButton
        color: root.backgroundColor()
        border.width: root.variant === "outlined" ? 1 : 0
        border.color: Theme.outlineVariant
    }

    Rectangle {
        anchors.fill: parent
        radius: background.radius
        color: root.foregroundColor()
        opacity: mouseArea.pressed ? 0.10 : mouseArea.containsMouse ? 0.08 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic }
        }
    }

    RowLayout {
        id: contentRow
        anchors.centerIn: parent
        spacing: Theme.spacingS

        Text {
            visible: root.icon.length > 0
            text: root.icon
            color: root.foregroundColor()
            font.family: Theme.fontIconFamily
            font.pixelSize: root.iconSize
            Layout.alignment: Qt.AlignVCenter
            verticalAlignment: Text.AlignVCenter
            lineHeightMode: Text.FixedHeight
            lineHeight: font.pixelSize
            renderType: Text.NativeRendering
        }

        Text {
            text: root.text
            color: root.foregroundColor()
            font.family: Theme.fontSans
            font.pixelSize: root.labelSize
            font.weight: Font.Medium
            Layout.alignment: Qt.AlignVCenter
            verticalAlignment: Text.AlignVCenter
            lineHeightMode: Text.FixedHeight
            lineHeight: font.pixelSize
            renderType: Text.NativeRendering
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
