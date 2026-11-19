import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dex

/// Pill used for every non-card state: listening, processing, executed, error.
Item {
    id: root

    property string mode: "listening" // listening | processing | executed | error
    property string title: qsTr("Listening…")
    property string detail: ""
    property real level: 0.0

    implicitWidth: row.implicitWidth + Theme.spacingM * 2
    implicitHeight: row.implicitHeight + Theme.spacingM * 2

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingM

        Item {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.centerIn: parent
                width: 36
                height: 36
                radius: 18
                color: root.mode === "executed"
                    ? Theme.secondaryContainer
                    : root.mode === "error"
                        ? Theme.errorContainer
                        : Theme.surfaceContainerHighest

                Rectangle {
                    id: halo
                    anchors.centerIn: parent
                    width: 26 + root.level * 22
                    height: width
                    radius: width / 2
                    color: Theme.primary
                    opacity: root.mode === "listening" ? 0.08 + root.level * 0.30 : 0
                    visible: root.mode === "listening"

                    Behavior on width {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic }
                    }
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic }
                    }
                }

                Rectangle {
                    id: dot
                    anchors.centerIn: parent
                    width: root.mode === "error"
                        ? 4
                        : 8 + Math.round(root.level * 12)
                    height: root.mode === "error"
                        ? 14
                        : 8 + Math.round(root.level * 12)
                    radius: root.mode === "error" ? 2 : width / 2
                    color: root.mode === "error" ? Theme.error : Theme.primary
                    visible: root.mode === "listening" || root.mode === "error"

                    Behavior on width {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic }
                    }
                    Behavior on height {
                        NumberAnimation { duration: Theme.durationFast; easing.type: Easing.OutCubic }
                    }

                    SequentialAnimation on opacity {
                        running: root.mode === "listening"
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.35; duration: 700; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    running: root.mode === "processing"
                    visible: running
                }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    visible: root.mode === "executed"
                    color: Theme.contentSecondaryContainer
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: root.title
                color: Theme.contentSurface
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter
                renderType: Text.NativeRendering
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.contentSurfaceVariant
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontBody
                verticalAlignment: Text.AlignVCenter
                renderType: Text.NativeRendering
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }
}
