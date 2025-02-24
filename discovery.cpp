#include "peerdiscovery.h"
#include <QDebug>

PeerDiscovery::PeerDiscovery(QObject *parent) : QObject(parent), udpSocket(new QUdpSocket(this)) {
    udpSocket->bind(12346, QUdpSocket::ShareAddress);
    connect(udpSocket, &QUdpSocket::readyRead, this, &PeerDiscovery::processPeerDiscoveryMessages);
}

void PeerDiscovery::discoverPeers() {
    QByteArray message = "P2Pal Peer Discovery Request";
    udpSocket->writeDatagram(message, QHostAddress::Broadcast, 12346);  // Broadcast discovery message
    qDebug() << "Broadcasting peer discovery request...";
}

void PeerDiscovery::processPeerDiscoveryMessages() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        udpSocket->readDatagram(data.data(), data.size(), &sender, &senderPort);

        qDebug() << "Discovered peer at" << sender.toString();
    }
}