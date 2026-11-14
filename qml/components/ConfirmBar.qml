import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dex

/// Confirmation gate: preview + Cancel [Esc] / Run [Enter].
Item {
    id: root

    implicitWidth: column.implicitWidth + Theme.spacingXl
    implicitHeight: column.implicitHeight + Theme.spacingXl

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingM

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingM

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: 16
                color: Theme.primaryContainer

                Text {
                    anchors.centerIn: parent
                    text: "?"
                    color: Theme.contentPrimaryContainer
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Run %1?").arg(AppController.statusTitle)
                    color: Theme.contentSurface
                    font.pixelSize: Theme.fontTitle
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    Layout.fillWidth: true
                    visible: AppController.statusDetail.length > 0
                    text: AppController.statusDetail
                    color: Theme.contentSurfaceVariant
                    font.pixelSize: Theme.fontLabel
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingS

            Item { Layout.fillWidth: true }

            Button {
                id: cancelButton
                implicitWidth: 118
                implicitHeight: 38
                onClicked: AppController.cancelAction()

                contentItem: Text {
                    text: qsTr("Cancel  Esc")
                    color: Theme.contentSurfaceVariant
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusButton
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.outlineVariant
                }
            }

            Button {
                id: runButton
                implicitWidth: 118
                implicitHeight: 38
                onClicked: AppController.confirmAction()

                contentItem: Text {
                    text: qsTr("Run  Enter")
                    color: Theme.contentPrimary
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusButton
                    color: runButton.down ? Qt.darker(Theme.primary, 1.15) : Theme.primary
                }
            }
        }
    }
}
