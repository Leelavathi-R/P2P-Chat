#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
# include "./udp.h"

// Basic GUI Implementation
class Chatbox : public QWidget {

private:
    QTextEdit *logArea;
    QTextEdit *inputText;
    QPushButton *sendButton;
    UdpChat udpChat;

public:
    Chatbox(const QString &origin, QWidget *parent = nullptr) : QWidget(parent), udpChat(origin) {
        
        QVBoxLayout *layout = new QVBoxLayout(this); // Chat Layout

        logArea = new QTextEdit(this); // 1. Chat Log Area
        logArea->setReadOnly(true);  // Prevent editing chat log
        
        inputText = new QTextEdit(this); // 2. Text Input Area
        inputText->setPlaceholderText("Type a message..."); 
        
        inputText->setWordWrapMode(QTextOption::WordWrap); // 3. Word wrap 
        
        inputText->setFocus();  // 4. Auto-focus on input field

        layout->addWidget(logArea);
        layout->addWidget(inputText);

        setLayout(layout);
        setWindowTitle("P2Pal Chatbox - " + origin);
        resize(400, 300);  // Set window size

        connect(&udpChat, &UdpChat::messageReceived, this, &Chatbox::onMessageReceived);
        connect(inputText, &QTextEdit::textChanged, this, [this]() {
            if (inputText->toPlainText().endsWith("\n")) {
                QString message = inputText->toPlainText().trimmed();
                inputText->clear();
                udpChat.sendMessage(message);
            }
        });

    }
private slots:
    void onMessageReceived(const QString &message) {
        logArea->append("Peer: " + message);
    }

};

int main(int argc, char *argv[]) {
    QApplication app_window(argc, argv);
    QString origin = "Peer" + QString::number(QRandomGenerator::global()->bounded(100));
    Chatbox chat(origin);
    chat.show();
    return app_window.exec();
}