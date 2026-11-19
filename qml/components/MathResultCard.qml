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
        spacing: Theme.spacingM

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingS

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: AppController.expressionText
                color: Theme.contentSurfaceVariant
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontLabel
                font.letterSpacing: 0.2
                verticalAlignment: Text.AlignVCenter
                lineHeightMode: Text.FixedHeight
                lineHeight: font.pixelSize
                renderType: Text.NativeRendering
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Rectangle {
                visible: AppController.resultCopied
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: copiedLabel.implicitWidth + Theme.spacingM
                implicitHeight: copiedLabel.implicitHeight + Theme.spacingXs
                radius: Theme.radiusChip
                color: Theme.tertiaryContainer

                Text {
                    id: copiedLabel
                    anchors.centerIn: parent
                    text: qsTr("Copied")
                    color: Theme.contentTertiaryContainer
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    verticalAlignment: Text.AlignVCenter
                    lineHeightMode: Text.FixedHeight
                    lineHeight: font.pixelSize
                    renderType: Text.NativeRendering
                }
            }
        }

        Text {
            id: mathResult
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: AppController.resultText
            color: Theme.contentSurface
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontLarge
            font.weight: Font.DemiBold
            font.letterSpacing: 0.3
            verticalAlignment: Text.AlignVCenter
            lineHeightMode: Text.FixedHeight
            lineHeight: font.pixelSize
            renderType: Text.NativeRendering
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.spacingS

            Item { Layout.fillWidth: true }

            M3Button {
                text: qsTr("Copy")
                icon: "content_copy"
                variant: "outlined"
                onClicked: AppController.copyResult()
            }

            M3Button {
                text: qsTr("Done")
                icon: "close"
                variant: "filled"
                onClicked: AppController.hideOverlay()
            }
        }
    }
}
