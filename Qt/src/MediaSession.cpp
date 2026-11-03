#include "MediaSession.h"
#include "PlaybackRules.h"
#include <QDebug>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <chrono>
#include <deque>
#include <functional>
#include <thread>

#ifdef Q_OS_WIN
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

namespace {
constexpr int kPollMilliseconds = 500;

// Local diagnosis only: RHINE_MEDIA_SESSION_LOG=1 prints session changes to stderr.
bool verbose() {
    static const bool enabled = !qEnvironmentVariableIsEmpty("RHINE_MEDIA_SESSION_LOG");
    return enabled;
}

#ifdef Q_OS_WIN
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;

QString toText(const winrt::hstring &value) {
    return value.empty() ? QString() : QString::fromWCharArray(value.c_str(), int(value.size()));
}

// Prefer the Apple Music session; other players are only a fallback.
GlobalSystemMediaTransportControlsSession
pickSession(const GlobalSystemMediaTransportControlsSessionManager &manager) {
    GlobalSystemMediaTransportControlsSession fallback{nullptr};
    for (auto const &session : manager.GetSessions()) {
        const auto id = toText(session.SourceAppUserModelId());
        if (id.contains(QStringLiteral("AppleMusic"), Qt::CaseInsensitive)) {
            return session;
        }
        if (!fallback) {
            fallback = session;
        }
    }
    return fallback;
}

QByteArray readThumbnail(
    const winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionMediaProperties &media) {
    try {
        auto reference = media.Thumbnail();
        if (!reference) {
            return {};
        }
        auto stream = reference.OpenReadAsync().get();
        // 部分会话的 Size() 为 0 或不准确，按块读到 EOF，避免一次性读取拿到空数据。
        winrt::Windows::Storage::Streams::DataReader reader{
            stream.as<winrt::Windows::Storage::Streams::IInputStream>()};
        QByteArray bytes;
        constexpr uint32_t chunk = 64 * 1024;
        while (bytes.size() < 8 * 1024 * 1024) {
            const auto loaded = reader.LoadAsync(chunk).get();
            if (loaded == 0) {
                break;
            }
            auto buffer = reader.ReadBuffer(loaded);
            bytes.append(reinterpret_cast<const char *>(buffer.data()), int(buffer.Length()));
        }
        if (verbose()) {
            qWarning("media-session: thumbnail bytes=%d", int(bytes.size()));
        }
        return bytes;
    } catch (...) {
        if (verbose()) {
            qWarning("media-session: thumbnail read failed");
        }
        return {};
    }
}
#endif
}

struct MediaSession::Impl {
    mutable QMutex stateMutex;
    QJsonObject state;
    QByteArray cover;
    QString coverKey;
    QMutex commandMutex;
#ifdef Q_OS_WIN
    std::deque<std::function<void(GlobalSystemMediaTransportControlsSessionManager &)>> commands;
#endif
    std::atomic_bool running{false};
    std::thread worker;

    void publish(MediaSession *owner, QJsonObject next, QByteArray image, QString key) {
        bool changed = false;
        {
            QMutexLocker lock(&stateMutex);
            if (state != next) {
                state = next;
                changed = true;
            }
            if (image != cover) {
                cover = std::move(image);
                coverKey = std::move(key);
                changed = true;
            }
        }
        if (!changed) {
            return;
        }
        if (verbose()) {
            qWarning("media-session: hasSession=%d title=%s playing=%d pos=%.0f/%.0f",
                     next.value("hasSession").toBool() ? 1 : 0,
                     qPrintable(next.value("title").toString()),
                     next.value("playing").toBool() ? 1 : 0, next.value("position").toDouble(),
                     next.value("duration").toDouble());
        }
        QMetaObject::invokeMethod(owner, [owner] { emit owner->changed(); }, Qt::QueuedConnection);
    }
};

MediaSession::MediaSession(QObject *parent) : QObject(parent), m_impl(std::make_unique<Impl>()) {
}

MediaSession::~MediaSession() {
    stop();
}

void MediaSession::start() {
    if (m_impl->running.exchange(true)) {
        return;
    }
    if (verbose()) {
        qWarning("media-session: start requested");
    }
    m_impl->worker = std::thread([this] {
#ifdef Q_OS_WIN
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        } catch (...) {
            m_impl->running = false;
            return;
        }
        GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
        auto lastKey = QString();
        while (m_impl->running) {
            if (!manager) {
                try {
                    manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                } catch (...) {
                    manager = nullptr;
                }
            }
            if (manager) {
                std::deque<std::function<void(GlobalSystemMediaTransportControlsSessionManager &)>> pending;
                {
                    QMutexLocker lock(&m_impl->commandMutex);
                    pending.swap(m_impl->commands);
                }
                for (auto &command : pending) {
                    try {
                        command(manager);
                    } catch (...) {
                    }
                }
                try {
                    const auto session = pickSession(manager);
                    QJsonObject state;
                    QByteArray image;
                    QString key;
                    if (session) {
                        const auto media = session.TryGetMediaPropertiesAsync().get();
                        const auto info = session.GetPlaybackInfo();
                        const auto timeline = session.GetTimelineProperties();
                        // Apple Music 常把「艺人 — 专辑」一起塞进 Artist，拆分后歌词检索才准。
                        auto artistText = toText(media.Artist());
                        auto albumText = toText(media.AlbumTitle());
                        const auto dash = artistText.indexOf(QStringLiteral(" — "));
                        if (dash > 0) {
                            if (albumText.isEmpty()) {
                                albumText = artistText.mid(dash + 3).trimmed();
                            }
                            artistText = artistText.left(dash).trimmed();
                        }
                        state = QJsonObject{
                            {"hasSession", true},
                            {"appID", toText(session.SourceAppUserModelId())},
                            {"title", toText(media.Title())},
                            {"artist", artistText},
                            {"album", albumText},
                            {"playing", static_cast<int>(info.PlaybackStatus()) == 4},
                            {"position", double(timeline.Position().count() / 10000)},
                            {"duration", double(timeline.EndTime().count() / 10000)}};
                        const auto nextKey = state["title"].toString() + "|" + state["artist"].toString();
                        if (nextKey != lastKey) {
                            image = readThumbnail(media);
                            key = QStringLiteral("media-session:") + nextKey;
                            lastKey = nextKey;
                        }
                    }
                    m_impl->publish(this, state, image, key);
                } catch (...) {
                    m_impl->publish(this, QJsonObject{{"hasSession", false}}, {}, {});
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollMilliseconds));
        }
        try {
            winrt::uninit_apartment();
        } catch (...) {
        }
#else
        while (m_impl->running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollMilliseconds));
        }
#endif
    });
}

void MediaSession::stop() {
    if (!m_impl->running.exchange(false)) {
        return;
    }
    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }
}

QJsonObject MediaSession::snapshot() const {
    QMutexLocker lock(&m_impl->stateMutex);
    return m_impl->state;
}

QByteArray MediaSession::cover() const {
    QMutexLocker lock(&m_impl->stateMutex);
    return m_impl->cover;
}

QString MediaSession::coverKey() const {
    QMutexLocker lock(&m_impl->stateMutex);
    return m_impl->coverKey;
}

void MediaSession::control(const QString &action, double value) {
#ifdef Q_OS_WIN
    if (!m_impl->running) {
        return;
    }
    QMutexLocker lock(&m_impl->commandMutex);
    m_impl->commands.push_back(
        [action, value](GlobalSystemMediaTransportControlsSessionManager &manager) {
            const auto session = pickSession(manager);
            if (!session) {
                return;
            }
            if (action == QLatin1String("toggle")) {
                session.TryTogglePlayPauseAsync().get();
            } else if (action == QLatin1String("play")) {
                session.TryPlayAsync().get();
            } else if (action == QLatin1String("pause")) {
                session.TryPauseAsync().get();
            } else if (action == QLatin1String("next")) {
                session.TrySkipNextAsync().get();
            } else if (PlaybackRules::sessionAction(action) == QLatin1String("previous")) {
                session.TrySkipPreviousAsync().get();
            } else if (action == QLatin1String("seek")) {
                const auto ticks = static_cast<int64_t>(value * 10000.0);
                session.TryChangePlaybackPositionAsync(ticks).get();
            }
        });
#else
    Q_UNUSED(action);
    Q_UNUSED(value);
#endif
}
