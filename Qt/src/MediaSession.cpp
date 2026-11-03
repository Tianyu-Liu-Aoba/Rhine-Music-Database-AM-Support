#include "MediaSession.h"
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

#ifdef Q_OS_WIN
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
using winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager;

QString toText(const winrt::hstring &value) {
    return value.empty() ? QString() : QString::fromWCharArray(value.c_str(), int(value.size()));
}

// 优先镜像 Apple Music；同机其他播放器只在没有 Apple Music 会话时使用。
GlobalSystemMediaTransportControlsSession pickSession(const GlobalSystemMediaTransportControlsSessionManager &manager) {
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
        const auto size = static_cast<uint32_t>(stream.Size());
        if (size == 0 || size > 8 * 1024 * 1024) {
            return {};
        }
        winrt::Windows::Storage::Streams::DataReader reader{
            stream.as<winrt::Windows::Storage::Streams::IInputStream>()};
        reader.LoadAsync(size).get();
        std::vector<uint8_t> bytes(size);
        reader.ReadBytes(bytes);
        return QByteArray(reinterpret_cast<const char *>(bytes.data()), int(size));
    } catch (...) {
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
        if (changed) {
            QMetaObject::invokeMethod(owner, [owner] { emit owner->changed(); }, Qt::QueuedConnection);
        }
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
                        state = QJsonObject{
                            {"hasSession", true},
                            {"appID", toText(session.SourceAppUserModelId())},
                            {"title", toText(media.Title())},
                            {"artist", toText(media.Artist())},
                            {"album", toText(media.AlbumTitle())},
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
    m_impl->commands.push_back([action, value](GlobalSystemMediaTransportControlsSessionManager &manager) {
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
        } else if (action == QLatin1String("previous")) {
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
