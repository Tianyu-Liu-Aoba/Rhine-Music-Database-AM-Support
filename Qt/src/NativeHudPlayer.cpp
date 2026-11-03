#include "NativeHud.h"
#include "MusicData.h"
#include "PlayerBar.h"
#include "PlaybackQueueCard.h"
#include <QDebug>
#include <algorithm>

/** 面板没有在播放、也没有可显示曲目时，播放条交给 Apple Music 的系统会话。 */
bool NativeHud::mirroringSystemSession() const {
    const auto verbose = !qEnvironmentVariableIsEmpty("RHINE_MEDIA_SESSION_LOG");
    const auto trace = [&](const char *why, bool value) {
        if (verbose) {
            qWarning("mirror: %s=%d", why, value ? 1 : 0);
        }
        return value;
    };
    if (!trace("apple", m_music->isApple())) {
        return false;
    }
    const auto &player = m_music->player;
    const auto session = m_host->mediaSessionState();
    const auto playing = player.state["playing"].toBool();
    const auto hasTitle = !session.value("title").toString().isEmpty();
    if (verbose) {
        qWarning("mirror: playing=%d currentTitle=%s sessionTitle=%s", playing ? 1 : 0,
                 qPrintable(player.current.value("title").toString()),
                 qPrintable(session.value("title").toString()));
    }
    if (playing) {
        return false;
    }
    if (player.current.value("title").toString().isEmpty()) {
        return trace("session", session.value("hasSession").toBool() && hasTitle);
    }
    return false;
}

/** 适配播放业务快照；状态栏组件不感知账号、歌词页面或播放器生命周期。 */
void NativeHud::playerBar(double x, double y, double width) {
    const auto &player = m_music->player;
    PlayerBar::State state;
    state.title = player.current.value("title").toString();
    state.artist = player.current.value("artist").toString();
    state.album = player.current.value("album").toString();
    state.cover = player.coverImage;
    state.status = player.status;
    state.mode = player.mode;
    state.positionMs = player.state["position"].toDouble();
    state.durationMs = player.state["duration"].toDouble();
    state.volume = player.state["volume"].toDouble();
    state.playing = player.state["playing"].toBool();
    state.loading = player.loading;
    state.failed = player.failed;
    state.hasTrack = !player.current.isEmpty();
    const auto liked = m_music->songLiked(player.current);
    state.liked = liked.value_or(false);
    state.likeKnown = liked.has_value();
    state.canLike = state.hasTrack && (m_music->isSiren() || !m_music->uid.isEmpty()) &&
                    !m_music->writeLoading && m_music->canMutate("/like") &&
                    player.current.value("provider").toString(m_music->provider) == m_music->provider;
    state.seekable = player.state["seekable"].toBool();
    // Apple Music 模式下由系统媒体会话接管播放条，可直接遥控官方应用。
    if (mirroringSystemSession()) {
        const auto session = m_host->mediaSessionState();
        state.title = session.value("title").toString();
        state.artist = session.value("artist").toString();
        state.album = session.value("album").toString();
        state.cover = m_host->mediaSessionCoverKey();
        state.status = session.value("playing").toBool() ? QStringLiteral("Apple Music 正在播放")
                                                         : QStringLiteral("Apple Music 已暂停");
        state.positionMs = session.value("position").toDouble();
        state.durationMs = session.value("duration").toDouble();
        state.playing = session.value("playing").toBool();
        state.loading = false;
        state.failed = false;
        state.hasTrack = true;
        state.seekable = true;
        state.liked = false;
        state.likeKnown = false;
        state.canLike = false;
    }
    state.applicationVolume = PlatformFeatures::ApplicationVolume;
    const auto drawing = PlayerBar::render(state, {x, y, width, PlayerBar::RowHeight}, dark, *m_glyphs);
    for (const auto &command : drawing.commands) {
        commands.append(command);
    }
    for (const auto &target : drawing.hits) {
        hits.append(target);
    }
    playerBounds = drawing.inputBounds;
    playerSeekRect = drawing.seek;
    playerVolumeRect = drawing.volume;
}

/** 队列面板 */
void NativeHud::playerPanel(double x, double y, double width) {
    const auto &player = m_music->player;
    // Apple Music 模式下队列由官方应用维护，这里展示 UIA 读到的真实队列。
    const auto appleTracks = m_music->isApple() ? m_host->appleQueue() : QJsonArray{};
    const auto trackCount = appleTracks.isEmpty() ? player.queue.size() : appleTracks.size();
    queueOffset = std::clamp(queueOffset, 0, PlaybackQueueCard::maximumOffset(trackCount));
    PlaybackQueueCard::Model model;
    model.bounds = {x, y, width, 836};
    model.tracks = appleTracks.isEmpty() ? player.queue.items() : appleTracks;
    model.current = appleTracks.isEmpty() ? player.current : QJsonObject{};
    if (!appleTracks.isEmpty()) {
        model.currentIndex = -1;
    }
    model.cover = player.coverImage;
    model.mode = player.mode;
    model.status = player.status;
    model.currentIndex = player.queue.indexOf(player.current);
    model.offset = queueOffset;
    model.playing = player.state["playing"].toBool();
    model.dark = dark;
    PlaybackQueueCard::Canvas canvas;
    canvas.fill = [this](double left, double top, double w, double h, QJsonArray color) {
        fill(left, top, w, h, color);
    };
    canvas.text = [this](QString value, double left, double top, double size, QString weight,
                         QJsonArray color) { text(value, left, top, size, weight, 0, color); };
    canvas.fit = [this](QString value, double size, double w, QString weight) {
        return fit(value, size, w, weight);
    };
    canvas.hit = [this](double left, double top, double w, double h, QString action, QString label) {
        hit(left, top, w, h, action, label);
    };
    canvas.image = [this](QString key, double left, double top, double w, double h) {
        image(key, left, top, w, h);
    };
    PlaybackQueueCard::paint(model, canvas);
}
