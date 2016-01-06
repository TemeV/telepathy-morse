/*
    Copyright (C) 2014 Alexandr Akulich <akulichalexander@gmail.com>

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
    OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef MORSE_TEXTCHANNEL_HPP
#define MORSE_TEXTCHANNEL_HPP

#include <QPointer>

#include <TelepathyQt/BaseChannel>

#include <TelegramQt/CTelegramCore>

class QTimer;

class MorseTextChannel;

typedef Tp::SharedPtr<MorseTextChannel> MorseTextChannelPtr;

class MorseTextChannel : public Tp::BaseChannelTextType
{
    Q_OBJECT
public:
    static MorseTextChannelPtr create(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID);
    virtual ~MorseTextChannel();

    QString sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error);

    void messageAcknowledgedCallback(const QString &messageId);

public slots:
    void whenContactChatStateComposingChanged(const QString &phone, bool composing);
    void whenMessageReceived(const TelegramNamespace::Message &message, uint senderHandle);
    void whenMessageMediaDataReceived(const QString &contact, quint32 messageId, const QByteArray &data, const QString &mimeType,
                                      TelegramNamespace::MessageType type, quint32 offset, quint32 size);
    void updateChatParticipants(const Tp::UIntList &handles);

    void whenChatDetailsChanged(quint32 chatId, const Tp::UIntList &handles);

protected slots:
    void sentMessageDeliveryStatusChanged(const QString &phone, quint64 messageId, TelegramNamespace::MessageDeliveryStatus status);
    void reactivateLocalTyping();

protected:
    void setChatState(uint state, Tp::DBusError *error);
    void processReceivedMessage(const TelegramNamespace::Message &message, uint senderHandle);

private:
    MorseTextChannel(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID);

    struct PendingTelegramMessage{
        PendingTelegramMessage():
            message(TelegramNamespace::Message()),
            senderHandle(0)
        {
        }
        PendingTelegramMessage(TelegramNamespace::Message message, uint senderHandle):
            message(message),
            senderHandle(senderHandle)
        {
        }
        TelegramNamespace::Message message;
        uint senderHandle;
    };

    struct PendingMessageData{
        QByteArray data;
        QString mimeType;
    };

    QPointer<CTelegramCore> m_core;

    uint m_targetHandle;
    uint m_targetHandleType;
    uint m_selfHandle;
    QString m_targetID;
    QString m_selfID;

    Tp::BaseChannelTextTypePtr m_channelTextType;
    Tp::BaseChannelMessagesInterfacePtr m_messagesIface;
    Tp::BaseChannelChatStateInterfacePtr m_chatStateIface;
    Tp::BaseChannelGroupInterfacePtr m_groupIface;
    Tp::BaseChannelRoomInterfacePtr m_roomIface;
    Tp::BaseChannelRoomConfigInterfacePtr m_roomConfigIface;

    QTimer *m_localTypingTimer;

    QMap<quint32, PendingTelegramMessage> m_pendingTelegramMessages;
    QMap<quint32, PendingMessageData> m_pendingMessageData;
};

#endif // MORSE_TEXTCHANNEL_HPP
