#include <QObject>
#include <QUdpSocket>

class PeerDiscovery : public QObject {
    Q_OBJECT

public:
    PeerDiscovery(QObject *parent = nullptr);
    void discoverPeers();

private slots:
    void processPeerDiscoveryMessages();

private:
    QUdpSocket *udpSocket;
};