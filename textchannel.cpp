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

#include "textchannel.hpp"

#include <TelepathyQt/Constants>
#include <TelepathyQt/RequestableChannelClassSpec>
#include <TelepathyQt/RequestableChannelClassSpecList>
#include <TelepathyQt/Types>

#include <QLatin1String>
#include <QVariantMap>
#include <QDateTime>
#include <QTimer>

MorseTextChannel::MorseTextChannel(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID)
    : Tp::BaseChannelTextType(baseChannel),
      m_targetHandle(baseChannel->targetHandle()),
      m_targetHandleType(baseChannel->targetHandleType()),
      m_selfHandle(selfHandle),
      m_targetID(baseChannel->targetID()),
      m_selfID(selfID),
      m_localTypingTimer(0)
{
    m_core = core;

    QStringList supportedContentTypes = QStringList() << QLatin1String("text/plain");
    Tp::UIntList messageTypes = Tp::UIntList() << Tp::ChannelTextMessageTypeNormal << Tp::ChannelTextMessageTypeDeliveryReport;

    uint messagePartSupportFlags = 0;
    uint deliveryReportingSupport = Tp::DeliveryReportingSupportFlagReceiveSuccesses|Tp::DeliveryReportingSupportFlagReceiveRead;

    setMessageAcknowledgedCallback(Tp::memFun(this, &MorseTextChannel::messageAcknowledgedCallback));

    m_messagesIface = Tp::BaseChannelMessagesInterface::create(this,
                                                               supportedContentTypes,
                                                               messageTypes,
                                                               messagePartSupportFlags,
                                                               deliveryReportingSupport);

    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_messagesIface));
    m_messagesIface->setSendMessageCallback(Tp::memFun(this, &MorseTextChannel::sendMessageCallback));

    m_chatStateIface = Tp::BaseChannelChatStateInterface::create();
    m_chatStateIface->setSetChatStateCallback(Tp::memFun(this, &MorseTextChannel::setChatState));
    baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_chatStateIface));

#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
    if (m_targetHandleType == Tp::HandleTypeRoom) {
        Tp::ChannelGroupFlags groupFlags = Tp::ChannelGroupFlagProperties;

        // Permissions:
        groupFlags |= Tp::ChannelGroupFlagCanAdd;

        m_groupIface = Tp::BaseChannelGroupInterface::create(); //Tp::ChannelGroupFlagChannelSpecificHandles
        m_groupIface->setGroupFlags(groupFlags);
        m_groupIface->setSelfHandle(m_selfHandle);
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_groupIface));

        TelegramNamespace::GroupChat info;

        quint32 chatId = CTelegramCore::identifierToChatId(m_targetID);
        m_core->getChatInfo(&info, chatId);

        QDateTime creationTimestamp;
        if (info.date) {
            creationTimestamp.setTime_t(info.date);
        }

        m_roomIface = Tp::BaseChannelRoomInterface::create(/* roomName */ m_targetID,
                                                           /* server */ QString(),
                                                           /* creator */ QString(),
                                                           /* creatorHandle */ 0,
                                                           creationTimestamp);

        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomIface));

        m_roomConfigIface = Tp::BaseChannelRoomConfigInterface::create();
        baseChannel->plugInterface(Tp::AbstractChannelInterfacePtr::dynamicCast(m_roomConfigIface));
    }
#endif

    connect(m_core.data(), SIGNAL(contactTypingStatusChanged(QString,bool)), SLOT(whenContactChatStateComposingChanged(QString,bool)));
    connect(m_core.data(), SIGNAL(sentMessageStatusChanged(QString,quint64,TelegramNamespace::MessageDeliveryStatus)),
            SLOT(sentMessageDeliveryStatusChanged(QString,quint64,TelegramNamespace::MessageDeliveryStatus)));
}

MorseTextChannelPtr MorseTextChannel::create(CTelegramCore *core, Tp::BaseChannel *baseChannel, uint selfHandle, const QString &selfID)
{
    return MorseTextChannelPtr(new MorseTextChannel(core, baseChannel, selfHandle, selfID));
}

MorseTextChannel::~MorseTextChannel()
{
}

QString MorseTextChannel::sendMessageCallback(const Tp::MessagePartList &messageParts, uint flags, Tp::DBusError *error)
{
    QString content;
    foreach (const Tp::MessagePart &part, messageParts) {
        if (part.contains(QLatin1String("content-type"))
                && part.value(QLatin1String("content-type")).variant().toString() == QLatin1String("text/plain")
                && part.contains(QLatin1String("content"))) {
            content = part.value(QLatin1String("content")).variant().toString();
            break;
        }
    }

    return QString::number(m_core->sendMessage(m_targetID, content));
}

void MorseTextChannel::messageAcknowledgedCallback(const QString &messageId)
{
    m_core->setMessageRead(m_targetID, messageId.toUInt());
}

void MorseTextChannel::whenContactChatStateComposingChanged(const QString &phone, bool composing)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (phone != m_targetID) {
        return;
    }

    if (composing) {
        m_chatStateIface->chatStateChanged(m_targetHandle, Tp::ChannelChatStateComposing);
    } else {
        m_chatStateIface->chatStateChanged(m_targetHandle, Tp::ChannelChatStateActive);
    }
}

void MorseTextChannel::whenMessageReceived(const TelegramNamespace::Message &message, uint senderHandle)
{
    if (message.type != TelegramNamespace::MessageTypeText) {
        m_core->requestMessageMediaData(message.id);
        m_pendingTelegramMessages.insert(message.id, PendingTelegramMessage(message, senderHandle));
        qDebug() << "Media message pending";
        connect(m_core, SIGNAL(messageMediaDataReceived(QString,quint32,QByteArray,QString,TelegramNamespace::MessageType,quint32,quint32)),
                this, SLOT(whenMessageMediaDataReceived(QString,quint32,QByteArray,QString,TelegramNamespace::MessageType,quint32,quint32)));

   } else{
        processReceivedMessage(message, senderHandle);
    }
}

void MorseTextChannel::whenMessageMediaDataReceived(const QString &contact, quint32 messageId, const QByteArray &data, const QString &mimeType, TelegramNamespace::MessageType type, quint32 offset, quint32 size)
{
    qDebug() << Q_FUNC_INFO <<
                " contact: " << contact <<
                " messageId:" << messageId <<
//                " data:" << data <<
                " mimeType:" << mimeType <<
                " type:" << type <<
                " offset:" << offset <<
                " size:" << size;

    PendingMessageData pendingData = m_pendingMessageData.value(messageId, PendingMessageData());
    pendingData.data.append(data);
    pendingData.mimeType = mimeType;
    m_pendingMessageData.insert(messageId, pendingData);

    if (pendingData.data.size() == size)
    {
        PendingTelegramMessage pendingMessage = m_pendingTelegramMessages.value(messageId);
        processReceivedMessage(pendingMessage.message, pendingMessage.senderHandle);
        m_pendingMessageData.remove(messageId);
        m_pendingTelegramMessages.remove(messageId);

        if (m_pendingTelegramMessages.isEmpty())
        {
            disconnect(m_core, SIGNAL(messageMediaDataReceived(QString,quint32,QByteArray,QString,TelegramNamespace::MessageType,quint32,quint32)),
                       this, SLOT(whenMessageMediaDataReceived(QString,quint32,QByteArray,QString,TelegramNamespace::MessageType,quint32,quint32)));
        }
    }
}

void MorseTextChannel::processReceivedMessage(const TelegramNamespace::Message &message, uint senderHandle)
{
    QString contactID;
    if (m_targetHandleType == Tp::HandleTypeContact) {
        contactID = m_targetID;
    } else {
#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
        contactID = m_groupIface->memberIdentifiers().value(senderHandle);
#else
        return;
#endif
    }

    Tp::MessagePartList body;
    Tp::MessagePart text;
    text[QLatin1String("content-type")] = QDBusVariant(QLatin1String("text/plain"));
    text[QLatin1String("content")]      = QDBusVariant(message.text);
    body << text;

    if (message.type != TelegramNamespace::MessageTypeText)
    {
        PendingMessageData messageData = m_pendingMessageData.value(message.id);
        Tp::MessagePart data;
        data[QLatin1String("content-type")]    = QDBusVariant(messageData.mimeType);
        data[QLatin1String("content")]         = QDBusVariant(messageData.data);
        body << data;
    }

    Tp::MessagePartList partList;
    Tp::MessagePart header;

    const QString token = QString::number(message.id);
    header[QLatin1String("message-token")] = QDBusVariant(token);
    header[QLatin1String("message-type")]  = QDBusVariant(Tp::ChannelTextMessageTypeNormal);
    header[QLatin1String("message-sent")]  = QDBusVariant(message.timestamp);

    if (message.flags & TelegramNamespace::MessageFlagOut) {
        header[QLatin1String("message-sender")]    = QDBusVariant(m_selfHandle);
        header[QLatin1String("message-sender-id")] = QDBusVariant(m_selfID);
        partList << header << body;
        m_messagesIface->messageSent(partList, 0, token);
    } else {
        uint currentTimestamp = QDateTime::currentMSecsSinceEpoch() / 1000;

        header[QLatin1String("message-received")]  = QDBusVariant(currentTimestamp);
        header[QLatin1String("message-sender")]    = QDBusVariant(senderHandle);

        if (!contactID.isEmpty()) {
            header[QLatin1String("message-sender-id")] = QDBusVariant(contactID);
        }

        partList << header << body;
        addReceivedMessage(partList);
    }
}

void MorseTextChannel::updateChatParticipants(const Tp::UIntList &handles)
{
#if TP_QT_VERSION >= TP_QT_VERSION_CHECK(0, 9, 7)
    m_groupIface->setMembers(handles, /* details */ QVariantMap());
#endif
}

void MorseTextChannel::whenChatDetailsChanged(quint32 chatId, const Tp::UIntList &handles)
{
    qDebug() << Q_FUNC_INFO << chatId;
    const QString chatID = QString(QLatin1String("chat%1")).arg(chatId);

    if (m_targetID == chatID) {
        updateChatParticipants(handles);

        TelegramNamespace::GroupChat info;
        if (m_core->getChatInfo(&info, chatId)) {
            m_roomConfigIface->setTitle(info.title);
            m_roomConfigIface->setConfigurationRetrieved(true);
        }
    }
}

void MorseTextChannel::sentMessageDeliveryStatusChanged(const QString &phone, quint64 messageId, TelegramNamespace::MessageDeliveryStatus status)
{
    // We are connected to broadcast signal, so have to select only needed calls
    if (phone != m_targetID) {
        return;
    }

    Tp::DeliveryStatus statusFlag;

    switch (status) {
    case TelegramNamespace::MessageDeliveryStatusSent:
        statusFlag = Tp::DeliveryStatusAccepted;
        break;
    case TelegramNamespace::MessageDeliveryStatusRead:
        statusFlag = Tp::DeliveryStatusRead;
        break;
    default:
        return;
    }

    const QString token = QString::number(messageId);

    Tp::MessagePartList partList;

    Tp::MessagePart header;
    header[QLatin1String("message-sender")]    = QDBusVariant(m_targetHandle);
    header[QLatin1String("message-sender-id")] = QDBusVariant(m_targetID);
    header[QLatin1String("message-type")]      = QDBusVariant(Tp::ChannelTextMessageTypeDeliveryReport);
    header[QLatin1String("delivery-status")]   = QDBusVariant(statusFlag);
    header[QLatin1String("delivery-token")]    = QDBusVariant(token);
    partList << header;

    addReceivedMessage(partList);
}

void MorseTextChannel::reactivateLocalTyping()
{
    m_core->setTyping(m_targetID, TelegramNamespace::MessageActionTyping);
}

void MorseTextChannel::setChatState(uint state, Tp::DBusError *error)
{
    Q_UNUSED(error);

    if (!m_localTypingTimer) {
        m_localTypingTimer = new QTimer(this);
        m_localTypingTimer->setInterval(CTelegramCore::localTypingRecommendedRepeatInterval());
        connect(m_localTypingTimer, SIGNAL(timeout()), this, SLOT(reactivateLocalTyping()));
    }

    if (state == Tp::ChannelChatStateComposing) {
        m_core->setTyping(m_targetID, TelegramNamespace::MessageActionTyping);
        m_localTypingTimer->start();
    } else {
        m_core->setTyping(m_targetID, TelegramNamespace::MessageActionNone);
        m_localTypingTimer->stop();
    }
}
