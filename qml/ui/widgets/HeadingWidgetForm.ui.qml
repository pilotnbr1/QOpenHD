import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.13
import QtGraphicalEffects 1.13
import Qt.labs.settings 1.0

import OpenHD 1.0

BaseWidget {
    id: headingWidget
    width: 30
    height: 20
    defaultYOffset: 50

    widgetIdentifier: "heading_widget"

    defaultHCenter: true
    defaultVCenter: false

    hasWidgetDetail: true
    widgetDetailComponent: Column {
        Item {
            width: parent.width
            height: 24
            Text {
                text: "Toggle Inav"
                color: "white"
                font.bold: true
                anchors.left: parent.left
            }
            Switch {
                width: 32
                height: parent.height
                anchors.rightMargin: 12
                anchors.right: parent.right
                // @disable-check M222
                Component.onCompleted: checked = settings.value("inav_heading",
                                                                true)
                // @disable-check M222
                onCheckedChanged: settings.setValue("inav_heading", checked)
            }
        }
    }

    Item {
        id: widgetInner
        height: 15
        anchors.fill: parent
        //so that fpv sits aligned in horizon must add margin
        width: 30

        Text {
            id: hdg_text
            color: "white"
            text: qsTr(OpenHD.hdg)
            bottomPadding: 3
            leftPadding: 6
            horizontalAlignment: Text.AlignHCenter
        }
        antialiasing: true

        Text {
            id: hdg_glyph
            y: 0
            width: 30
            height: 18
            color: "#ffffff"
            text: "\ufdd8"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            font.family: "Font Awesome 5 Free-Solid-900.otf"
            font.pixelSize: 14
        }
    }
}
