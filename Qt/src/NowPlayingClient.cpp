#include "NowPlayingClient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {
constexpr int kPollMilliseconds = 1000;
const char *kBase = "http://127.0.0.1:9863";
}

NowPlayingClient::NowPlayingClient(QObject *parent) : QObject(parent) {
    m_timer.setInterval(kPollMilliseconds);
    connect(&m_timer, &QTimer::timeout, this, &NowPlayingClient::poll);
}

void NowPlayingClient::start() {
    if (!m_timer.isActive()) {
        m_timer.start();
        poll();
    }
}

void NowPlayingClient::stop() {
    m_timer.stop();
    if (!m_state.isEmpty()) {
        m_state = {};
        emit changed();
    }
}

bool NowPlayingClient::available() const {
    return !m_state.isEmpty();
}

QJsonObject NowPlayingClient::state() const {
    return m_state;
}

QString NowPlayingClient::coverKey() const {
    return m_coverKey;
}

QByteArray NowPlayingClient::cover() const {
    return m_cover;
}

QString NowPlayingClient::lyricText() const {
    return m_lyricText;
}

QString NowPlayingClient::translatedLyric() const {
    return m_translatedLyric;
}

void NowPlayingClient::poll() {
    QNetworkRequest request{QUrl(QStringLiteral("%1/api/query").arg(QString::fromLatin1(kBase)))};
    request.setTransferTimeout(1500);
    auto *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (!m_state.isEmpty()) {
                m_state = {};
                m_coverKey.clear();
                m_cover.clear();
                emit changed();
            }
            return;
        }
        const auto body = QJsonDocument::fromJson(reply->readAll()).object();
        const auto player = body.value("player").toObject();
        const auto track = body.value("track").toObject();
        const auto title = track.value("title").toString();
        QJsonObject next;
        if (player.value("hasSong").toBool() && !title.isEmpty()) {
            next = QJsonObject{
                {"hasSession", true},
                {"appID", QStringLiteral("now-playing")},
                {"title", title},
                {"artist", track.value("author").toString()},
                {"album", track.value("album").toString()},
                {"playing", !player.value("isPaused").toBool()},
                {"position", player.value("seekbarCurrentPosition").toDouble() * 1000.0},
                {"duration", track.value("duration").toDouble() * 1000.0},
                {"coverUrl", track.value("cover").toString()},
                {"url", track.value("url").toString()}};
        }
        const bool changed = next != m_state;
        if (!qEnvironmentVariableIsEmpty("RHINE_MEDIA_SESSION_LOG")) {
            qWarning("now-playing: song=%d title=%s album=%s cover=%d", next.isEmpty() ? 0 : 1,
                     qPrintable(next.value("title").toString()),
                     qPrintable(next.value("album").toString()),
                     next.value("coverUrl").toString().isEmpty() ? 0 : 1);
        }
        if (changed) {
            m_state = next;
            emit this->changed();
            if (next.isEmpty()) {
                m_coverKey.clear();
                m_cover.clear();
                m_coverUrl.clear();
            }
        }
        if (next.isEmpty()) {
            return;
        }
        const auto coverUrl = next.value("coverUrl").toString();
        if (!coverUrl.isEmpty() && coverUrl != m_coverUrl && coverUrl != m_coverPending) {
            fetchCover(coverUrl);
        }
        const auto artist = next.value("artist").toString();
        const auto key = title + QLatin1Char('|') + artist;
        if (!m_lyricPending && key != m_lyricKey) {
            m_lyricKey = key;
            fetchLyric(title, artist);
        }
    });
}

void NowPlayingClient::fetchCover(QString url) {
    m_coverPending = url;
    QNetworkRequest request{QUrl(url)};
    request.setTransferTimeout(5000);
    auto *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        m_coverPending.clear();
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }
        m_cover = reply->readAll();
        m_coverUrl = url;
        m_coverKey = QStringLiteral("now-playing:") + url;
        emit changed();
    });
}

void NowPlayingClient::fetchLyric(QString title, QString artist) {
    Q_UNUSED(title);
    Q_UNUSED(artist);
    m_lyricPending = true;
    m_lyricText.clear();
    m_translatedLyric.clear();
    QNetworkRequest request{QUrl(QStringLiteral("%1/api/lyric").arg(QString::fromLatin1(kBase)))};
    request.setTransferTimeout(4000);
    auto *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_lyricPending = false;
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }
        const auto body = QJsonDocument::fromJson(reply->readAll()).object();
        m_lyricText = body.value("lrc").toString();
        m_translatedLyric = body.value("translatedLyric").toString();
        if (!m_lyricText.isEmpty()) {
            emit changed();
        }
    });
}
