#include "mavlinkbase.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#ifndef __windows__
#include <unistd.h>
#endif

#include <QtNetwork>
#include <QThread>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFuture>

#include <openhd/mavlink.h>

#include "util.h"
#include "constants.h"

#include "openhd.h"
#include "powermicroservice.h"

#include "localmessage.h"

MavlinkBase::MavlinkBase(QObject *parent,  MavlinkType mavlink_type): QObject(parent), m_ground_available(false), m_mavlink_type(mavlink_type) {
    qDebug() << "MavlinkBase::MavlinkBase()";
}

void MavlinkBase::onStarted() {
    qDebug() << "MavlinkBase::onStarted(" << localPort << ")";

    switch (m_mavlink_type) {
        case MavlinkTypeUDP: {
            mavlinkSocket = new QUdpSocket(this);
            mavlinkSocket->bind(QHostAddress::Any, localPort);
            connect(mavlinkSocket, &QUdpSocket::readyRead, this, &MavlinkBase::processMavlinkUDPDatagrams);
            break;
        }
        case MavlinkTypeTCP: {
            mavlinkSocket = new QTcpSocket(this);
            connect(mavlinkSocket, &QUdpSocket::readyRead, this, &MavlinkBase::processMavlinkTCPData);
            connect(mavlinkSocket, &QUdpSocket::connected, this, &MavlinkBase::onTCPConnected);
            connect(mavlinkSocket, &QUdpSocket::disconnected, this, &MavlinkBase::onTCPDisconnected);

            tcpReconnectTimer = new QTimer(this);
            connect(tcpReconnectTimer, &QTimer::timeout, this, &MavlinkBase::reconnectTCP);
            tcpReconnectTimer->start(1000);
            break;
        }
    }

    emit setup();
}

void MavlinkBase::onTCPConnected() {

}

void MavlinkBase::onTCPDisconnected() {
    reconnectTCP();
}

void MavlinkBase::reconnectTCP() {
    if (((QTcpSocket*)mavlinkSocket)->state() == QAbstractSocket::ConnectedState) {
        ((QTcpSocket*)mavlinkSocket)->disconnectFromHost();
    }

    if (((QTcpSocket*)mavlinkSocket)->state() == QAbstractSocket::UnconnectedState) {
        ((QTcpSocket*)mavlinkSocket)->connectToHost(groundAddress, groundTCPPort);
    }
}

void MavlinkBase::setGroundIP(QString address) {
    bool reconnect = false;
    if (groundAddress != address) {
        reconnect = true;
    }

    groundAddress = address;

    if (reconnect) {
        switch (m_mavlink_type) {
            case MavlinkTypeTCP: {
                reconnectTCP();
                break;
            }
            default: {
                break;
            }
        }
    }
}


void MavlinkBase::set_loading(bool loading) {
    m_loading = loading;
    emit loadingChanged(m_loading);
}


void MavlinkBase::set_saving(bool saving) {
    m_saving = saving;
    emit savingChanged(m_saving);
}


void MavlinkBase::sendData(char* data, int len) {
    switch (m_mavlink_type) {
        case MavlinkTypeUDP: {
            ((QUdpSocket*)mavlinkSocket)->writeDatagram((char*)data, len, QHostAddress(groundAddress), groundUDPPort);
            break;
        }
        case MavlinkTypeTCP: {
            if (((QTcpSocket*)mavlinkSocket)->state() == QAbstractSocket::ConnectedState) {
                ((QTcpSocket*)mavlinkSocket)->write((char*)data, len);
            }
            break;
        }
    }
}

QVariantMap MavlinkBase::getAllParameters() {
    return m_allParameters;
}


void MavlinkBase::fetchParameters() {
    qDebug() << "MavlinkBase::fetchParameters()";
    mavlink_message_t msg;
    mavlink_msg_param_request_list_pack(255, MAV_COMP_ID_MISSIONPLANNER, &msg, targetSysID, targetCompID);

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    int len = mavlink_msg_to_send_buffer(buffer, &msg);

    sendData((char*)buffer, len);
}


bool MavlinkBase::isConnectionLost() {
    // we want to know if a heartbeat has been received (not -1, the default)
    // but not in the last 5 seconds.
    if (m_last_heartbeat > -1 && m_last_heartbeat < 5000) {
        return false;
    }
    return true;
}

void MavlinkBase::resetParamVars() {
    m_allParameters.clear();
    parameterCount = 0;
    parameterIndex = 0;
    initialConnectTimer = -1;
    // give the MavlinkStateGetParameters state a chance to receive a parameter
    // before timing out
    parameterLastReceivedTime = QDateTime::currentMSecsSinceEpoch();
}


void MavlinkBase::stateLoop() {
    qint64 current_timestamp = QDateTime::currentMSecsSinceEpoch();
    set_last_heartbeat(current_timestamp - last_heartbeat_timestamp);

    switch (state) {
        case MavlinkStateDisconnected: {
            set_loading(false);
            set_saving(false);
            if (m_ground_available) {
                state = MavlinkStateConnected;
            }
            break;
        }
        case MavlinkStateConnected: {
            if (initialConnectTimer == -1) {
                initialConnectTimer = QDateTime::currentMSecsSinceEpoch();
            } else if (current_timestamp - initialConnectTimer < 5000) {
                state = MavlinkStateGetParameters;
                resetParamVars();
                fetchParameters();
                //LocalMessage::instance()->showMessage("Connecting to drone", 0);
            }
            break;
        }
        case MavlinkStateGetParameters: {
            set_loading(true);
            set_saving(false);
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

            if (isConnectionLost()) {
                resetParamVars();
                m_ground_available = false;
                state = MavlinkStateDisconnected;
                //LocalMessage::instance()->showMessage("Connection to drone lost (E1)", 0);
            }

            if ((parameterCount != 0) && parameterIndex == (parameterCount - 1)) {
                emit allParametersChanged();
                state = MavlinkStateIdle;
            }

            if (currentTime - parameterLastReceivedTime > 7000) {
                resetParamVars();
                m_ground_available = false;
                state = MavlinkStateDisconnected;
                //LocalMessage::instance()->showMessage("Connection to drone lost (E2)", 0);
            }
            break;
        }
        case MavlinkStateIdle: {
            set_loading(false);

            if (isConnectionLost()) {
                resetParamVars();
                m_ground_available = false;
                state = MavlinkStateDisconnected;
                //LocalMessage::instance()->showMessage("Connection to drone lost (E3)", 0);
            }

            break;
        }
    }
}


void MavlinkBase::processMavlinkTCPData() {
    QByteArray data = mavlinkSocket->readAll();
    processData(data);
}


void MavlinkBase::processMavlinkUDPDatagrams() {
    QByteArray datagram;

    while ( ((QUdpSocket*)mavlinkSocket)->hasPendingDatagrams()) {
        m_ground_available = true;

        datagram.resize(int(((QUdpSocket*)mavlinkSocket)->pendingDatagramSize()));
        QHostAddress _groundAddress;
        quint16 groundPort;
         ((QUdpSocket*)mavlinkSocket)->readDatagram(datagram.data(), datagram.size(), &_groundAddress, &groundPort);
        groundUDPPort = groundPort;
        processData(datagram);
    }
}


void MavlinkBase::processData(QByteArray data) {
        typedef QByteArray::Iterator Iterator;
        mavlink_message_t msg;

        for (Iterator i = data.begin(); i != data.end(); i++) {
            char c = *i;

            uint8_t res = mavlink_parse_char(MAVLINK_COMM_0, (uint8_t)c, &msg, &r_mavlink_status);

            if (res) {
                emit processMavlinkMessage(msg);
            }
        }
}


void MavlinkBase::set_last_heartbeat(qint64 last_heartbeat) {
    m_last_heartbeat = last_heartbeat;
    emit last_heartbeat_changed(m_last_heartbeat);
}
