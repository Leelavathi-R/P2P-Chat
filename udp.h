#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QRandomGenerator>


class UdpChat : public QObject {
    Q_OBJECT
public:
    explicit UdpChat(const QString &origin, QObject *parent = nullptr);
    
    void sendMessage(const QString &message);

signals:
    void messageReceived(const QString &message);

private slots:
    void processBroadCastMessages();
    void broadcastPresence();
    void processIncomingMessages();
    void resendMessage(int, quint16);
    void sendAck(int, quint16);
    void syncMessages();
    void sendSyncData(int, quint16, QString);

private:
    QUdpSocket udpSocketBroadcast;
    QUdpSocket udpSocketP2P;
    QString origin;
    quint16 localPort;  
    int sequenceNumber = 0;
    QMap<int, QString> sentMessages; // contains sent messages.
    QMap<QString, QPair<quint16, QVector<int>>> discoveredPeers;//contains discovered peers & message history
    QMap<QString, bool> isAckReceived;
    QMap<QString, int> vectorClock;
    QMap<QString, QMap<int, QString>> receivedMessages;
    
};