import QtQuick
import QtQuick.Layouts
import Dex

/// Expression subtitle + large tabular result + "Copied" badge.
Item {
    id: root

    implicitWidth: column.implicitWidth + Theme.spacingXl
    implicitHeight: column.implicitHeight + Theme.spacingXl

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Theme.spacingM
        spacing: Theme.spacingXs

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingS

            Text {
                Layout.fillWidth: true
                text: AppController.expressionText
                color: Theme.contentSurfaceVariant
                font.pixelSize: Theme.fontLabel
                font.letterSpacing: 0.2
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Rectangle {
                visible: AppController.resultCopied
                implicitWidth: copiedLabel.implicitWidth + Theme.spacingM
                implicitHeight: 22
                radius: Theme.radiusChip
                color: Theme.tertiaryContainer

                Text {
                    id: copiedLabel
                    anchors.centerIn: parent
                    text: qsTr("Copied")
                    color: Theme.contentTertiaryContainer
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: AppController.resultText
            color: Theme.contentSurface
            font.pixelSize: Theme.fontLarge
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
