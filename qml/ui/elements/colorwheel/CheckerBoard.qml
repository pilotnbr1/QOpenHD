/*
 * Copyright Maxime Engel (https://github.com/MaximeEngel)
 *
 * License: GPL v3
 *
 */

import QtQuick 2.0

Grid {
    id: root
    property int cellSide: 5
    anchors.fill: parent
    rows: height/cellSide; columns: width/cellSide
    clip: true
    Repeater {
        model: root.columns*root.rows
        Rectangle {
            width: root.cellSide; height: root.cellSide
            color: (index%2 == 0) ? "gray" : "white"
        }
    }
}
