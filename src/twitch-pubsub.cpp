#include "twitch-pubsub.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QAbstractSocket>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <utility>

namespace BitrateSwitch {

static const char *kPubSubUrl = "wss://pubsub-edge.twitch.tv";

struct RaidPack {
    TwitchPubSubClient::RaidCallback cb;
    std::string login;
    std::string display;
};

static void queueRaidCallback(TwitchPubSubClient::RaidCallback cb, std::string login,
                              std::string display)
{
    auto *p = new RaidPack{std::move(cb), std::move(login), std::move(display)};
    obs_queue_task(
        OBS_TASK_UI,
        [](void *vp) {
            auto *pack = static_cast<RaidPack *>(vp);
            if (pack->cb)
                pack->cb(pack->login, pack->display);
            delete pack;
        },
        p,
        false);
}

TwitchPubSubClient::TwitchPubSubClient() = default;

TwitchPubSubClient::~TwitchPubSubClient()
{
    stop();
}

void TwitchPubSubClient::setRaidCallback(RaidCallback cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    raidCb_ = std::move(cb);
}

void TwitchPubSubClient::subscribeRaid(const std::string &broadcasterUserId)
{
    if (broadcasterUserId.empty())
        return;
    std::string topic = "raid." + broadcasterUserId;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &t : topics_) {
        if (t == topic)
            return;
    }
    topics_.push_back(std::move(topic));
    resendListen_ = true;
}

void TwitchPubSubClient::start()
{
    if (running_.exchange(true))
        return;
    worker_ = std::thread([this]() { workerMain(); });
}

void TwitchPubSubClient::stop()
{
    running_ = false;
    if (worker_.joinable())
        worker_.join();
    connected_ = false;
}

bool TwitchPubSubClient::isConnected() const
{
    return connected_;
}

void TwitchPubSubClient::flushListen(QWebSocket &ws)
{
    std::vector<std::string> copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        copy = topics_;
        resendListen_ = false;
    }
    if (copy.empty())
        return;

    QStringList qtopics;
    for (const auto &t : copy)
        qtopics.append(QString::fromStdString(t));

    QJsonObject data;
    data["topics"] = QJsonArray::fromStringList(qtopics);
    data["auth_token"] = QJsonValue(QJsonValue::Null);

    QJsonObject root;
    root["type"] = QStringLiteral("LISTEN");
    root["nonce"] = QString::number(++nonce_);
    root["data"] = data;

    ws.sendTextMessage(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

void TwitchPubSubClient::workerMain()
{
    QEventLoop loop;
    QWebSocket ws;
    QTimer pingTimer;
    pingTimer.setInterval(290000);
    pingTimer.setSingleShot(false);

    QObject::connect(&pingTimer, &QTimer::timeout, [&]() {
        if (!running_)
            return;
        QJsonObject ping;
        ping["type"] = QStringLiteral("PING");
        ping["nonce"] = QString::number(++nonce_);
        ws.sendTextMessage(QString::fromUtf8(QJsonDocument(ping).toJson(QJsonDocument::Compact)));
    });

    QObject::connect(&ws, &QWebSocket::connected, [&]() {
        connected_ = true;
        flushListen(ws);
        pingTimer.start();
    });

    QObject::connect(&ws, &QWebSocket::textMessageReceived, [&](const QString &q) {
        RaidCallback cbCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cbCopy = raidCb_;
        }

        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(q.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            return;
        QJsonObject o = doc.object();
        if (o.value(QLatin1String("type")).toString() != QLatin1String("MESSAGE"))
            return;
        QJsonObject data = o.value(QLatin1String("data")).toObject();
        QString innerStr = data.value(QLatin1String("message")).toString();
        if (innerStr.isEmpty())
            return;

        QJsonDocument innerDoc = QJsonDocument::fromJson(innerStr.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !innerDoc.isObject())
            return;
        QJsonObject innerObj = innerDoc.object();
        if (innerObj.value(QLatin1String("type")).toString() != QLatin1String("raid_go_v2"))
            return;
        QJsonObject raid = innerObj.value(QLatin1String("raid")).toObject();
        QString targetLogin = raid.value(QLatin1String("target_login")).toString();
        QString display = raid.value(QLatin1String("target_display_name")).toString();
        if (targetLogin.isEmpty())
            return;

        auto now = std::chrono::steady_clock::now();
        if (haveLastRaidEmit_ &&
            std::chrono::duration_cast<std::chrono::seconds>(now - lastRaidEmit_).count() < 10)
            return;
        haveLastRaidEmit_ = true;
        lastRaidEmit_ = now;

        if (cbCopy)
            queueRaidCallback(std::move(cbCopy), targetLogin.toStdString(), display.toStdString());
    });

    QObject::connect(&ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
                     [&](QAbstractSocket::SocketError) {
                         blog(LOG_WARNING, "[BitrateSceneSwitch] PubSub: socket error");
                     });

    QObject::connect(&ws, &QWebSocket::disconnected, [&]() {
        connected_ = false;
        pingTimer.stop();
    });

    ws.open(QUrl(QString::fromUtf8(kPubSubUrl)));

    while (running_) {
        loop.processEvents(QEventLoop::AllEvents, 50);
        bool flush = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            flush = resendListen_;
        }
        if (flush && ws.state() == QAbstractSocket::ConnectedState)
            flushListen(ws);
    }

    pingTimer.stop();
    ws.close();
}

} // namespace BitrateSwitch
