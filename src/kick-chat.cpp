#include "kick-chat.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QAbstractSocket>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWebSocket>

#include <cctype>
#include <cstring>
#include <utility>

namespace BitrateSwitch {

static const char *kKickWsUrl =
    "wss://ws-us2.pusher.com/app/32cbd69e4b950bf97679?protocol=7&client=js&version=7.6.0&flash=false";

struct KickUiCmd {
    KickChatClient::CommandCallback cb;
    ChatMessage msg;
};

struct KickUiRaid {
    KickChatClient::RaidCallback cb;
    std::string targetSlug;
    std::string displayName;
};

KickChatClient::KickChatClient() = default;

KickChatClient::~KickChatClient()
{
    disconnect();
}

void KickChatClient::setConfig(const ChatConfig &cfg)
{
    config_ = cfg;
}

void KickChatClient::setCommandCallback(CommandCallback cb)
{
    cmdCb_ = std::move(cb);
}

void KickChatClient::setRaidCallback(RaidCallback cb)
{
    raidCb_ = std::move(cb);
}

static void queueChatCommand(KickChatClient::CommandCallback cb, ChatMessage msg)
{
    auto *p = new KickUiCmd{std::move(cb), std::move(msg)};
    obs_queue_task(
        OBS_TASK_UI,
        [](void *vp) {
            auto *pack = static_cast<KickUiCmd *>(vp);
            if (pack->cb)
                pack->cb(pack->msg);
            delete pack;
        },
        p,
        false);
}

static void queueRaid(KickChatClient::RaidCallback cb, std::string slug, std::string display)
{
    auto *p = new KickUiRaid{std::move(cb), std::move(slug), std::move(display)};
    obs_queue_task(
        OBS_TASK_UI,
        [](void *vp) {
            auto *pack = static_cast<KickUiRaid *>(vp);
            if (pack->cb)
                pack->cb(pack->targetSlug, pack->displayName);
            delete pack;
        },
        p,
        false);
}

static bool kickCanUseCommands(const ChatConfig &cfg, const QString &senderSlug,
                               const QJsonArray &badges)
{
    QString ch = QString::fromStdString(cfg.channel);
    if (senderSlug.compare(ch, Qt::CaseInsensitive) == 0)
        return true;
    for (const QJsonValue &v : badges) {
        if (!v.isObject())
            continue;
        QString k = v.toObject().value(QLatin1String("type")).toString();
        if (k == QLatin1String("moderator") || k == QLatin1String("broadcaster"))
            return true;
    }
    QString sl = senderSlug.toLower();
    for (const auto &a : cfg.admins) {
        if (sl == QString::fromStdString(a).toLower())
            return true;
    }
    return false;
}

bool KickChatClient::parsePusherEvent(const std::string &utf8, std::string *eventName,
                                      std::string *dataJson)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(utf8), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    QJsonObject o = doc.object();
    QString ev = o.value(QLatin1String("event")).toString();
    if (ev.isEmpty())
        return false;
    QJsonValue dataVal = o.value(QLatin1String("data"));
    if (dataVal.isString())
        *dataJson = dataVal.toString().toStdString();
    else if (dataVal.isObject())
        *dataJson = QString(QJsonDocument(dataVal.toObject()).toJson(QJsonDocument::Compact))
                        .toStdString();
    else
        return false;
    *eventName = ev.toStdString();
    return true;
}

void KickChatClient::handleChatJson(const std::string &dataJson)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(dataJson), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    QJsonObject root = doc.object();
    if (root.value(QLatin1String("type")).toString() != QLatin1String("message"))
        return;
    qint64 roomId = root.value(QLatin1String("chatroom_id")).toVariant().toLongLong();
    if (roomId <= 0)
        return;
    if (static_cast<uint64_t>(roomId) != config_.kickChatroomId)
        return;
    QString content = root.value(QLatin1String("content")).toString();
    QJsonObject sender = root.value(QLatin1String("sender")).toObject();
    QString slug = sender.value(QLatin1String("slug")).toString();
    QJsonArray badges = sender.value(QLatin1String("identity")).toObject()
                            .value(QLatin1String("badges"))
                            .toArray();
    if (!kickCanUseCommands(config_, slug, badges))
        return;
    if (content.isEmpty() || !content.startsWith(QLatin1Char('!')))
        return;

    ChatMessage msg;
    msg.username = slug.toStdString();
    msg.message = content.toStdString();
    std::string args;
    msg.command = ChatClient::parseCommandForConfig(config_, msg.message, args);
    msg.args = std::move(args);

    if (msg.command == ChatCommand::None)
        return;
    queueChatCommand(cmdCb_, std::move(msg));
}

void KickChatClient::handleHostRaidJson(const std::string &dataJson)
{
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(dataJson), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return;
    QJsonObject root = doc.object();
    QString channelSlug = root.value(QLatin1String("channel")).toObject()
                              .value(QLatin1String("slug"))
                              .toString();
    QJsonObject hosted = root.value(QLatin1String("hosted")).toObject();
    QString hostedSlug = hosted.value(QLatin1String("slug")).toString();
    QString hostedUser = hosted.value(QLatin1String("username")).toString();
    if (hostedSlug.isEmpty())
        return;
    QString cfgCh = QString::fromStdString(config_.channel);
    if (channelSlug.compare(cfgCh, Qt::CaseInsensitive) != 0)
        return;

    queueRaid(raidCb_, hostedSlug.toStdString(),
              hostedUser.isEmpty() ? hostedSlug.toStdString() : hostedUser.toStdString());
}

void KickChatClient::dispatchText(const std::string &utf8)
{
    std::string ev, data;
    if (!parsePusherEvent(utf8, &ev, &data))
        return;
    if (ev == "App\\Events\\ChatMessageEvent")
        handleChatJson(data);
    else if (ev == "App\\Events\\ChatMoveToSupportedChannelEvent")
        handleHostRaidJson(data);
}

void KickChatClient::workerMain()
{
    QEventLoop loop;
    QWebSocket ws;
    QString subscribeTpl = QStringLiteral("{\"event\":\"pusher:subscribe\",\"data\":{\"auth\":\"\","
                                          "\"channel\":\"%1\"}}");

    QObject::connect(&ws, &QWebSocket::connected, [&]() {
        QString ch1 = QStringLiteral("channel.%1").arg(config_.kickChannelId);
        QString ch2 = QStringLiteral("chatrooms.%1.v2").arg(config_.kickChatroomId);
        ws.sendTextMessage(subscribeTpl.arg(ch1));
        ws.sendTextMessage(subscribeTpl.arg(ch2));
        connected_ = true;
        blog(LOG_INFO, "[BitrateSceneSwitch] Kick: subscribed");
    });

    QObject::connect(&ws, &QWebSocket::textMessageReceived, [&](const QString &q) {
        dispatchText(q.toUtf8().toStdString());
    });

    QObject::connect(&ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
                     [&](QAbstractSocket::SocketError) {
                         blog(LOG_WARNING, "[BitrateSceneSwitch] Kick: socket error");
                     });

    QObject::connect(&ws, &QWebSocket::disconnected, &loop, &QEventLoop::quit);

    ws.open(QUrl(QString::fromUtf8(kKickWsUrl)));

    QTimer pingTimer;
    pingTimer.setInterval(120000);
    QObject::connect(&pingTimer, &QTimer::timeout, [&]() {
        if (running_)
            ws.sendTextMessage(QStringLiteral("{\"event\":\"pusher:ping\",\"data\":{}}"));
    });
    pingTimer.start();

    while (running_) {
        loop.processEvents(QEventLoop::AllEvents, 50);
    }

    pingTimer.stop();
    ws.close();
    connected_ = false;
}

bool KickChatClient::connect()
{
    if (running_)
        return true;
    if (config_.kickChannelId == 0 || config_.kickChatroomId == 0) {
        blog(LOG_WARNING, "[BitrateSceneSwitch] Kick: set channel id and chatroom id");
        return false;
    }

    running_ = true;
    worker_ = std::thread([this]() { workerMain(); });
    return true;
}

void KickChatClient::disconnect()
{
    running_ = false;
    if (worker_.joinable())
        worker_.join();
}

bool KickChatClient::isConnected() const
{
    return connected_;
}

void KickChatClient::sendMessage(const std::string &)
{
}

} // namespace BitrateSwitch
