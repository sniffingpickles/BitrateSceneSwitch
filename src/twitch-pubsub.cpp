#include "twitch-pubsub.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace BitrateSwitch {

static const char *kPubSubUrl = "wss://pubsub-edge.twitch.tv";

struct RaidPack {
	TwitchPubSubClient::RaidCallback cb;
	std::string login;
	std::string display;
};

static void queueRaidCallback(TwitchPubSubClient::RaidCallback cb,
			       std::string login, std::string display)
{
	auto *p = new RaidPack{std::move(cb), std::move(login),
			       std::move(display)};
	obs_queue_task(
		OBS_TASK_UI,
		[](void *vp) {
			auto *pack = static_cast<RaidPack *>(vp);
			if (pack->cb)
				pack->cb(pack->login, pack->display);
			delete pack;
		},
		p, false);
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
	ws_.disconnect();
	if (worker_.joinable())
		worker_.join();
	connected_ = false;
}

bool TwitchPubSubClient::isConnected() const
{
	return connected_;
}

void TwitchPubSubClient::flushListen()
{
	std::vector<std::string> copy;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		copy = topics_;
		resendListen_ = false;
	}
	if (copy.empty())
		return;

	QJsonArray qtopics;
	for (const auto &t : copy)
		qtopics.append(QString::fromStdString(t));

	QJsonObject data;
	data["topics"] = qtopics;
	data["auth_token"] = QJsonValue(QJsonValue::Null);

	QJsonObject root;
	root["type"] = QStringLiteral("LISTEN");
	root["nonce"] = QString::number(++nonce_);
	root["data"] = data;

	std::string json =
		QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString();
	ws_.send(json);
}

void TwitchPubSubClient::workerMain()
{
	if (!ws_.connect(kPubSubUrl)) {
		blog(LOG_WARNING,
		     "[BitrateSceneSwitch] PubSub: failed to connect");
		connected_ = false;
		return;
	}

	connected_ = true;
	flushListen();

	auto lastPing = std::chrono::steady_clock::now();

	while (running_) {
		bool flush = false;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			flush = resendListen_;
		}
		if (flush && ws_.isConnected())
			flushListen();

		std::string raw;
		auto result = ws_.recv(raw);

		if (result == WsClient::RecvResult::Timeout) {
			auto elapsed =
				std::chrono::duration_cast<
					std::chrono::seconds>(
					std::chrono::steady_clock::now() -
					lastPing)
					.count();
			if (elapsed >= 280) {
				QJsonObject ping;
				ping["type"] = QStringLiteral("PING");
				ping["nonce"] = QString::number(++nonce_);
				std::string pj =
					QJsonDocument(ping)
						.toJson(QJsonDocument::Compact)
						.toStdString();
				ws_.send(pj);
				lastPing = std::chrono::steady_clock::now();
			}
			continue;
		}

		if (result != WsClient::RecvResult::Message)
			break;

		RaidCallback cbCopy;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			cbCopy = raidCb_;
		}

		QJsonParseError err{};
		QJsonDocument doc =
			QJsonDocument::fromJson(QByteArray::fromStdString(raw),
						&err);
		if (err.error != QJsonParseError::NoError || !doc.isObject())
			continue;
		QJsonObject o = doc.object();
		if (o.value(QLatin1String("type")).toString() !=
		    QLatin1String("MESSAGE"))
			continue;
		QJsonObject data = o.value(QLatin1String("data")).toObject();
		QString innerStr =
			data.value(QLatin1String("message")).toString();
		if (innerStr.isEmpty())
			continue;

		QJsonDocument innerDoc =
			QJsonDocument::fromJson(innerStr.toUtf8(), &err);
		if (err.error != QJsonParseError::NoError ||
		    !innerDoc.isObject())
			continue;
		QJsonObject innerObj = innerDoc.object();
		if (innerObj.value(QLatin1String("type")).toString() !=
		    QLatin1String("raid_go_v2"))
			continue;
		QJsonObject raid =
			innerObj.value(QLatin1String("raid")).toObject();
		QString targetLogin =
			raid.value(QLatin1String("target_login")).toString();
		QString display =
			raid.value(QLatin1String("target_display_name"))
				.toString();
		if (targetLogin.isEmpty())
			continue;

		auto now = std::chrono::steady_clock::now();
		if (haveLastRaidEmit_ &&
		    std::chrono::duration_cast<std::chrono::seconds>(
			    now - lastRaidEmit_)
				    .count() < 10)
			continue;
		haveLastRaidEmit_ = true;
		lastRaidEmit_ = now;

		if (cbCopy)
			queueRaidCallback(std::move(cbCopy),
					  targetLogin.toStdString(),
					  display.toStdString());
	}

	ws_.disconnect();
	connected_ = false;
}

} // namespace BitrateSwitch
