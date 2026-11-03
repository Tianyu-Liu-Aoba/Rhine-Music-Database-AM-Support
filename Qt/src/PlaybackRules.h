#pragma once
#include <QJsonArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

/**
 * 播放相关的纯规则：不依赖 Qt 事件循环、网络或 WinRT，便于用例直接断言。
 * 生产代码与回归用例共用同一份实现——今晚两个反复出现的 bug（prev 路由、
 * 歌词候选只按艺人+时长放行）都在这两个函数里。
 */
namespace PlaybackRules {

// 播放条发出的动作名与系统会话动作名并不一致：prev 才是按钮，previous 是会话。
inline QString sessionAction(QString action) {
    if (action == QLatin1String("prev")) {
        return QStringLiteral("previous");
    }
    return action;
}

// 比较用：小写并去掉空白与标点，避免「(K)NoW_NAME」这类写法差异。
inline QString normalized(const QString &value) {
    static const QRegularExpression noise(QStringLiteral("[\\s\\p{P}]"));
    auto text = value.toLower();
    text.remove(noise);
    return text;
}

// 去掉括号里的版本后缀：(Shihoru ver.)、[Remix]、（Live）等。
inline QString withoutVersion(const QString &value) {
    static const QRegularExpression version(QStringLiteral("[\\(（\\[].*?[\\)）\\]]"));
    auto text = value;
    text.remove(version);
    return text.simplified();
}

/**
 * 歌词候选是否可信：必须「标题相符 且 艺人相符」。
 * 只按艺人 + 时长匹配会串到同一艺人的其它版本（Harvest 与 Harvest (Shihoru ver.)）。
 */
inline bool lyricMatches(const QString &title, const QString &artist, const QString &candidateTitle,
                         const QJsonArray &candidateArtists) {
    const auto wantedTitle = normalized(title);
    const auto wantedCore = normalized(withoutVersion(title));
    const auto candidateFull = normalized(candidateTitle);
    const auto candidateCore = normalized(withoutVersion(candidateTitle));
    if (wantedTitle.isEmpty() || candidateFull.isEmpty()) {
        return false;
    }
    const auto titleExact = candidateFull == wantedTitle;
    const auto titleSimilar = !candidateCore.isEmpty() && !wantedCore.isEmpty() &&
                              (candidateCore.contains(wantedCore) ||
                               wantedCore.contains(candidateCore));
    if (!titleExact && !titleSimilar) {
        return false;
    }
    const auto wantedArtist = normalized(artist);
    if (wantedArtist.isEmpty()) {
        return false;
    }
    for (const auto &entry : candidateArtists) {
        const auto name = normalized(entry.toObject().value("name").toString());
        if (!name.isEmpty() && (name.contains(wantedArtist) || wantedArtist.contains(name))) {
            return true;
        }
    }
    // 艺人可以是纯字符串数组（部分接口），宽松处理一次。
    for (const auto &entry : candidateArtists) {
        const auto name = normalized(entry.toString());
        if (!name.isEmpty() && (name.contains(wantedArtist) || wantedArtist.contains(name))) {
            return true;
        }
    }
    return false;
}

} // namespace PlaybackRules
