#include <QObject>
#include <QMap>
#include <QString>

class GossipProtocol : public QObject {
    Q_OBJECT

public:
    GossipProtocol(QObject *parent = nullptr);
    void handleReceivedMessage(const QVariantMap &message);
    void compareMessageHistories();

private:
    QMap<QString, int> messageHistory;  // Message history for each origin
};
