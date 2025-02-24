#include "udp.h"
#include <QDebug>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>


UdpChat::UdpChat(const QString &origin, QObject *parent) : QObject(parent),origin(origin) {
    
    localPort = QRandomGenerator::global()->bounded(4000, 6000);
    
    udpSocketP2P.bind(QHostAddress::AnyIPv4, localPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(&udpSocketP2P, &QUdpSocket::readyRead, this, &UdpChat::processIncomingMessages);
    
    udpSocketBroadcast.bind(QHostAddress::Any, 9999, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(&udpSocketBroadcast, &QUdpSocket::readyRead, this, &UdpChat::processBroadCastMessages);
    broadcastPresence();

    QTimer *syncTimer = new QTimer(this);
    connect(syncTimer, &QTimer::timeout, this, &UdpChat::syncMessages);
    syncTimer->start(5000); 
}

void UdpChat::broadcastPresence() {
    QVariantMap data;
    data["Origin"] = origin;
    data["Port"] = localPort;
    data["ChatText"] = "Hello from "+origin;
    QByteArray message = QJsonDocument::fromVariant(data).toJson();

    udpSocketBroadcast.writeDatagram(message, QHostAddress::Broadcast, 9999);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, message]() {
        udpSocketBroadcast.writeDatagram(message, QHostAddress::Broadcast, 9999);  // 9999 is a common port for peer discovery
    });
    timer->start(5000);  // Broadcast every 5 seconds
}

void UdpChat::sendMessage(const QString &message) {

    if (discoveredPeers.isEmpty()) {
        qDebug() << "No peers available to send the message.";
        return;
    }

    QVariantMap data;
    data["ChatText"] = message;
    data["Origin"] = origin;
    data["sequenceNumber"] = ++sequenceNumber;
    data["Type"] = "msg";

    QByteArray buffer = QJsonDocument::fromVariant(data).toJson();
    sentMessages[sequenceNumber] = message;
    for (auto it = discoveredPeers.begin(); it != discoveredPeers.end(); ++it) {
        if(it.key() != origin){
            udpSocketP2P.writeDatagram(buffer, QHostAddress::AnyIPv4, it.value().first);
            qDebug() << "Sent message to peer:" << it.key() << "on port" << it.value().first;
            discoveredPeers[it.key()].second.append(sequenceNumber);
            if (isAckReceived.contains(it.key())) {
                isAckReceived[it.key()] = false;  // Update existing key
            } else {
                isAckReceived.insert(it.key(), false);  // Create new key
            }
            QTimer *resendTimer = new QTimer(this);
            resendTimer->setSingleShot(true);  // timer runs only once
            quint16 peerPort = it.value().first;
            QString peerKey = it.key();  // Capture the key (peer identifier)

            // Capture peerKey and peerPort in the lambda
            connect(resendTimer, &QTimer::timeout, this, [this, peerPort, peerKey, resendTimer]() {
                if (!isAckReceived[peerKey]) {
                    qDebug() << "Resending message with seqNum:" << this->sequenceNumber;
                    resendMessage(this->sequenceNumber, peerPort);
                }
                resendTimer->deleteLater();  // Clean up the timer after it times out
            });
            resendTimer->start(2000); 
        } 
    }
}

void UdpChat::resendMessage(int seqNum, quint16 peerPort) {
        QVariantMap data;
        data["ChatText"] = sentMessages[seqNum];
        data["Origin"] = origin;
        data["sequenceNumber"] = seqNum;
        data["Type"] = "msg";

        QByteArray buffer = QJsonDocument::fromVariant(data).toJson();
        udpSocketP2P.writeDatagram(buffer, QHostAddress::AnyIPv4, peerPort);
        qDebug() << "Resent message with seqNum:" << seqNum << "to port" << peerPort;
}

void UdpChat::processBroadCastMessages() {
    while (udpSocketBroadcast.hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(udpSocketBroadcast.pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udpSocketBroadcast.readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);

        QJsonDocument doc = QJsonDocument::fromJson(buffer);
        QVariantMap data = doc.toVariant().toMap();

        QString peerOrigin = data["Origin"].toString();
        quint16 peerPort = data["Port"].toUInt();
        QString message = data["ChatText"].toString();

        if (!discoveredPeers.contains(peerOrigin)) {  
            discoveredPeers[peerOrigin] = qMakePair(peerPort, QVector<int>()); ; 
            emit messageReceived(message);
        } 
        if ((!vectorClock.contains(peerOrigin)) && (peerOrigin != origin)) {
            vectorClock[peerOrigin] = 0;
        }
        if ((!receivedMessages.contains(peerOrigin)) && (peerOrigin != origin)) {
            receivedMessages[peerOrigin] = QMap<int, QString>();
        }
    }
}

void UdpChat::processIncomingMessages() {
    while (udpSocketP2P.hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(udpSocketP2P.pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udpSocketP2P.readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);

        QJsonDocument doc = QJsonDocument::fromJson(buffer);
        QVariantMap data = doc.toVariant().toMap();
        QString peerOrigin = data["Origin"].toString();
        quint16 peerPort = data["Port"].toUInt();
        QString message = data["ChatText"].toString();
        
        if (data["Type"] == "ack") {
            isAckReceived[peerOrigin] = true;
            emit messageReceived("ACK from "+peerOrigin);
        }
        else if(data["Type"] == "msg"){
            int sequenceNumber = data["sequenceNumber"].toInt();
            receivedMessages[peerOrigin][sequenceNumber] = message;
            vectorClock[peerOrigin] = qMax(vectorClock[peerOrigin], sequenceNumber);
            data.remove("Type");
            data.remove("Port");
            QByteArray modifiedBuffer = QJsonDocument::fromVariant(data).toJson();
            sendAck(data["sequenceNumber"].toInt(), senderPort);
            emit messageReceived(QString::fromUtf8(modifiedBuffer));
        }
        else if(data["Type"] == "sync"){
            qDebug()<<"vector-clock(peer):"<<vectorClock;
            QVariantMap receivedVectorClock = data["VectorClock"].toMap();
            for (auto it = receivedVectorClock.begin(); it != receivedVectorClock.end(); ++it) {
                int receivedSeqNum = it.value().toInt();
                if (vectorClock.contains(it.key())) {
                    int localSeqNum = vectorClock[it.key()];
                    qDebug()<<"receivedSeqNum:"<<receivedSeqNum<<"localSeqNum"<<localSeqNum;
                    if(receivedSeqNum < localSeqNum){
                        qDebug()<<peerOrigin<<"has missing messages"<<it.key()<<localSeqNum;
                        sendSyncData(receivedSeqNum, peerPort, it.key());
                    }
                    else{
                        qDebug()<<peerOrigin<<" doesnt have any missing messages: ";
                    }
                }
            }
            if (receivedVectorClock.isEmpty()){
                qDebug()<<peerOrigin<<"has missing messages";
                    for(auto peeritr = receivedMessages.begin(); peeritr !=  receivedMessages.end(); peeritr++){
                        sendSyncData(0, peerPort, peeritr->first());
                    }
            }
        }
    }
}

void UdpChat::sendSyncData(int receivedSeqNum, quint16 peerPort, QString peerName){
    qDebug()<<receivedSeqNum<<peerName;
    for(auto msgitr = receivedMessages[peerName].begin(); msgitr !=  receivedMessages[peerName].end(); msgitr++){
        QVariantMap syncData;
       qDebug()<< msgitr.key()<<msgitr.value();
        if(msgitr.key() > receivedSeqNum){
            qDebug()<<"Inside if";
            syncData["ChatText"] = msgitr.value();
            syncData["Origin"] = peerName;
            syncData["sequenceNumber"] = msgitr.key();
            syncData["Type"] = "msg";
            QByteArray buffer = QJsonDocument::fromVariant(syncData).toJson();
            udpSocketP2P.writeDatagram(buffer, QHostAddress::AnyIPv4, peerPort);
            qDebug() << "Sync msgs:"<< "to port" << peerPort;
        }
    }
}
void UdpChat::sendAck(int seqNum, quint16 receiverPort) {
    QVariantMap ackData;
    ackData["Type"] = "ack";
    ackData["sequenceNumber"] = seqNum;
    ackData["Origin"] = origin;

    QByteArray buffer = QJsonDocument::fromVariant(ackData).toJson();
    udpSocketP2P.writeDatagram(buffer, QHostAddress::AnyIPv4, receiverPort);
}

void UdpChat::syncMessages() {
    qDebug() << "Performing Anti-Entropy Sync";
    qDebug()<< receivedMessages;
    for (auto it = discoveredPeers.begin(); it != discoveredPeers.end(); ++it) {
        if(it.key() != origin){
            QString peer = it.key();
            quint16 peerPort = it.value().first;

            QVariantMap request;
            request["Type"] = "sync";
            request["Origin"] = origin;
            request["Port"] = discoveredPeers[origin].first;
            QVariantMap seqMap;
            for (auto seqIt = vectorClock.begin(); seqIt != vectorClock.end(); ++seqIt) {
                seqMap[seqIt.key()] = seqIt.value();
            }
            request["VectorClock"] = seqMap;
            QByteArray buffer = QJsonDocument::fromVariant(request).toJson();
            qDebug()<<"VectorClock-sent:"<<buffer;
            udpSocketP2P.writeDatagram(buffer, QHostAddress::Broadcast, peerPort);
        }
    }
}

