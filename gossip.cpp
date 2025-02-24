#include "gossipprotocol.h"
#include <QVariantMap>
#include <QDebug>

GossipProtocol::GossipProtocol(QObject *parent) : QObject(parent) {}

void GossipProtocol::handleReceivedMessage(const QVariantMap &message) {
    QString origin = message["Origin"].toString();
    int seqNum = message["SequenceNumber"].toInt();

    if (messageHistory.value(origin) < seqNum) {
        messageHistory[origin] = seqNum;  // Update history with the latest sequence number
        qDebug() << "Received message from" << origin << "Sequence:" << seqNum;
    }
}

void GossipProtocol::compareMessageHistories() {
    // This is where peers compare their message histories
    // Anti-Entropy mechanism would trigger message sending if needed
    qDebug() << "Comparing message histories...";
}