import QtQuick
import Dex

Window {
    id: root

    visible: AppController.overlayVisible
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    width: AppController.windowWidth
    height: card.height + Theme.spacingXl * 2

    FocusScope {
        id: focusScope
        anchors.fill: parent
        focus: AppController.state === AppController.Confirming

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                AppController.confirmAction()
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                AppController.cancelAction()
                event.accepted = true
            }
        }

        Rectangle {
            id: card
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Theme.spacingL
            width: Math.min(root.width - Theme.spacingXl * 2, 560)
            height: loader.height + Theme.spacingXs * 2
            radius: AppController.state === AppController.Confirming
                ? Theme.radiusCard
                : Theme.radiusPill
            color: Theme.surfaceContainerHigh
            border.width: 1
            border.color: Theme.outlineVariant

            Behavior on height {
                NumberAnimation { duration: Theme.durationMedium; easing.type: Easing.OutCubic }
            }
            Behavior on radius {
                NumberAnimation { duration: Theme.durationMedium; easing.type: Easing.OutCubic }
            }

            Loader {
                id: loader
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Theme.spacingXs
                width: parent.width - Theme.spacingXs * 2
                // Height comes from the loaded item so the card can size to
                // it. Filling the card here would create a height binding
                // loop (card -> loader -> item -> card).
                sourceComponent: {
                    switch (AppController.state) {
                    case AppController.MathResult:
                        return mathComponent
                    case AppController.Confirming:
                        return confirmComponent
                    case AppController.CommandExecuted:
                        return executedComponent
                    case AppController.Error:
                        return errorComponent
                    default:
                        return listeningComponent
                    }
                }
            }
        }
    }

    Component {
        id: listeningComponent
        StatusPill {
            mode: AppController.state === AppController.Processing ? "processing" : "listening"
            level: AppController.audioLevel
            title: {
                if (AppController.state === AppController.Processing)
                    return qsTr("Working…")
                return AppController.partialText.length > 0
                    ? AppController.partialText
                    : qsTr("Listening…")
            }
            detail: AppController.state === AppController.Processing
                ? AppController.finalText
                : AppController.providerName === "stub"
                    ? qsTr("Stub input — send FINAL:<text> over IPC")
                    : qsTr("Say a command or a calculation")
        }
    }

    Component {
        id: executedComponent
        StatusPill {
            mode: "executed"
            title: AppController.statusDetail.length > 0
                ? AppController.statusTitle + " • " + AppController.statusDetail
                : AppController.statusTitle
            detail: qsTr("Done")
        }
    }

    Component {
        id: errorComponent
        StatusPill {
            mode: "error"
            title: AppController.statusTitle.length > 0
                ? AppController.statusTitle
                : qsTr("Something went wrong")
            detail: AppController.errorText
        }
    }

    Component {
        id: mathComponent
        MathResultCard {}
    }

    Component {
        id: confirmComponent
        ConfirmBar {}
    }
}
