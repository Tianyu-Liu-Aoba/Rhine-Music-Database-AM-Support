#include "AppleMusic.h"
#include "AppleData.h"
#include "MusicData.h"
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>
#include <algorithm>

namespace {
constexpr int kPageSize = 30;
constexpr int kSearchLimit = 100;
constexpr int kLookupLimit = 200;
constexpr int kChartLimit = 40;

QJsonValue argument(const QJsonArray &arguments, qsizetype index) {
    return index >= 0 && index < arguments.size() ? arguments.at(index) : QJsonValue();
}

/** 把 LRCLIB 的搜索结果转成面板歌词文档；没有可用歌词时返回空对象。 */
QJsonObject lyricsDocument(const QJsonArray &results) {
    QJsonObject chosen;
    for (const auto &value : results) {
        const auto entry = value.toObject();
        if (!entry.value("syncedLyrics").toString().isEmpty()) {
            chosen = entry;
            break;
        }
        if (chosen.isEmpty() && !entry.value("plainLyrics").toString().isEmpty()) {
            chosen = entry;
        }
    }
    const auto synced = chosen.value("syncedLyrics").toString();
    const auto plain = chosen.value("plainLyrics").toString();
    if (synced.isEmpty() && plain.isEmpty()) {
        return {};
    }
    QJsonArray lines;
    static const QRegularExpression stamp(
        QStringLiteral("\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"));
    for (const auto &raw : synced.split('\n')) {
        const auto match = stamp.match(raw);
        if (!match.hasMatch()) {
            continue;
        }
        const auto text = raw.mid(match.capturedEnd()).trimmed();
        if (text.isEmpty()) {
            continue;
        }
        auto fraction = 0;
        const auto digits = match.captured(3);
        if (!digits.isEmpty()) {
            fraction = digits.toInt();
            if (digits.size() == 1) {
                fraction *= 100;
            } else if (digits.size() == 2) {
                fraction *= 10;
            }
        }
        const auto startMs =
            (match.captured(1).toInt() * 60 + match.captured(2).toInt()) * 1000 + fraction;
        lines.append(QJsonObject{{"startMs", double(startMs)}, {"text", text}});
    }
    return QJsonObject{{"version", 1},
                       {"timing", lines.isEmpty() ? QStringLiteral("none") : QStringLiteral("line")},
                       {"lines", lines},
                       {"plainText", plain.isEmpty() ? synced : plain},
                       {"offsetMs", 0.0},
                       {"source", QStringLiteral("lrclib")}};
}

QString entityOf(const QString &kind) {    if (kind == "song") {
        return QStringLiteral("song");
    }
    if (kind == "album") {
        return QStringLiteral("album");
    }
    if (kind == "artist") {
        return QStringLiteral("musicArtist");
    }
    return {};
}

QJsonArray idsOf(const QJsonArray &values) {
    QJsonArray ids;
    for (const auto &value : values) {
        const auto id = value.toObject().value("id").toString();
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}
}

AppleMusic::AppleMusic(QObject *parent) : QObject(parent) {
    m_catalog.setStorefront(defaultStorefront());
}

QString AppleMusic::defaultStorefront() {
    const auto code = QLocale::territoryToCode(QLocale::system().territory()).toLower();
    static const QRegularExpression pattern(QStringLiteral("^[a-z]{2}$"));
    if (!pattern.match(code).hasMatch()) {
        return QStringLiteral("us");
    }
    // 公开 iTunes 接口的大陆店面只返回艺人，歌曲、专辑与按 ID 查询都是空结果。
    return code == "cn" ? QStringLiteral("tw") : code;
}

void AppleMusic::setStorefront(const QString &value) {
    m_catalog.setStorefront(value);
}

/** 切换来源或刷新商店后，旧请求的迟到结果不得再交付。 */
void AppleMusic::reset() {
    ++m_generation;
    m_catalog.reset();
}

void AppleMusic::request(const QString &operation, const QJsonArray &arguments, Reply reply) {
    if (!qEnvironmentVariableIsEmpty("RHINE_MEDIA_SESSION_LOG")) {
        qWarning("apple-op: %s args=%lld web=%d", qPrintable(operation), qlonglong(arguments.size()),
                 m_webLibrary ? 1 : 0);
    }
    const auto generation = m_generation;
    Done done = [this, generation, reply = std::move(reply)](QJsonValue value, QString error) {
        if (generation != m_generation) {
            return;
        }
        if (!error.isEmpty()) {
            reply(QJsonObject{{"code", 400}, {"message", error}}, {});
            return;
        }
        reply(QJsonObject{{"code", 200}, {"value", value}}, {});
    };
    if (operation == "search") {
        search(argument(arguments, 0).toString(), argument(arguments, 1).toString(QStringLiteral("song")),
               argument(arguments, 2).toInt(), std::move(done));
        return;
    }
    if (operation == "songs") {
        songs(argument(arguments, 0).toArray(), std::move(done));
        return;
    }
    if (operation == "detail") {
        detail(argument(arguments, 0).toObject(), std::move(done));
        return;
    }
    if (operation == "artistAlbums") {
        artistAlbums(argument(arguments, 0).toString(), argument(arguments, 1).toInt(), std::move(done));
        return;
    }
    if (operation == "trackPage") {
        trackPage(argument(arguments, 0).toObject(), argument(arguments, 1).toInt(), std::move(done));
        return;
    }
    if (operation == "playback" || operation == "preview") {
        playback(argument(arguments, 0).toObject(), std::move(done));
        return;
    }
    if (operation == "account") {
        done(QJsonArray{QStringLiteral("apple-local"), QStringLiteral("Apple Music")}, {});
        return;
    }
    if (operation == "capabilities") {
        done(capabilities(), {});
        return;
    }
    if (operation == "vip") {
        done(QJsonObject{{"vipLevel", QStringLiteral("unknown")}}, {});
        return;
    }
    if (operation == "subscriptionStatus") {
        done(QJsonObject{}, {});
        return;
    }
    if (operation == "suggest" || operation == "categories") {
        done(QJsonArray{}, {});
        return;
    }
    if (operation == "likedIDs") {
        done(QJsonArray{}, {});
        return;
    }
    if (operation == "wiki") {
        done(QStringLiteral("Apple Music 没有对应的百科条目，可在 Apple Music 应用中查看专辑与艺人介绍。"),
             {});
        return;
    }
    if (operation == "lyrics") {
        lyrics(argument(arguments, 0).toObject(), std::move(done));
        return;
    }
    if (operation == "mutate") {
        done({}, QStringLiteral("Apple Music 公开目录为只读，请在 Apple Music 应用中编辑资料库"));
        return;
    }
    if (operation == "discover") {
        discover(argument(arguments, 0).toString(), argument(arguments, 1).toInt(), std::move(done));
        return;
    }
    if (QStringList{"userPlaylists", "likedSongs", "likedAlbums", "librarySongs",
                    "favoritePlaylists"}
            .contains(operation)) {
        // 有网页会话桥时读真实资料库；没有就明确说明需要登录。
        if (m_webLibrary) {
            QString kind = QStringLiteral("songs");
            if (operation == "userPlaylists" || operation == "favoritePlaylists") {
                kind = QStringLiteral("playlists");
            } else if (operation == "likedAlbums") {
                kind = QStringLiteral("albums");
            }
            const auto offset = argument(arguments, 0).toInt();
            m_webLibrary(kind, QJsonObject{{"limit", 100}, {"offset", offset}},
                         [done, kind, offset](QJsonObject body, QString error) {
                             if (!error.isEmpty()) {
                                 done({}, error);
                                 return;
                             }
                             const auto result = body.value("result").toObject();
                             QJsonArray values;
                             for (const auto &value : result.value("items").toArray()) {
                                 const auto item = value.toObject();
                                 auto entry = MusicData::item(
                                     kind == "albums"       ? "album"
                                     : kind == "playlists"  ? "playlist"
                                                            : "song",
                                     item.value("id").toString(), item.value("name").toString(),
                                     item.value("artist").toString(), QStringLiteral("apple"));
                                 entry["album"] = item.value("album").toString();
                                 entry["duration"] = item.value("duration").toDouble();
                                 entry["cover"] = item.value("artwork").toString();
                                 entry["sourceURL"] = QStringLiteral("https://music.apple.com/");
                                 if (!entry.isEmpty()) {
                                     values.append(entry);
                                 }
                             }
                             done(page(values, offset), {});
                         });
            return;
        }
        done(page({}, 0,
                  QStringLiteral("登录 Apple Music 网页版后即可在此浏览资料库")),
             {});
        return;
    }
    done({}, QStringLiteral("Apple Music 不支持该接口：") + operation);
}

QJsonObject AppleMusic::page(const QJsonArray &values, int offset, const QString &notice) {
    const auto start = std::max(0, offset);
    QJsonArray items;
    for (auto index = qsizetype(start); index < values.size() && items.size() < kPageSize; ++index) {
        items.append(values.at(index));
    }
    QJsonObject result{{"items", items},
                       {"rawCount", items.size()},
                       {"total", values.size()},
                       {"more", start + items.size() < values.size()}};
    if (!notice.isEmpty()) {
        result["notice"] = notice;
    }
    return result;
}

/** 发现页用公开榜单填充：不需要账号，因此“每日推荐/猜你喜欢”统一落到热门歌曲。 */
void AppleMusic::discover(const QString &tab, int offset, Done done) {
    QString kind;
    if (tab == "albums") {
        kind = QStringLiteral("albums");
    } else if (tab.isEmpty() || tab == "charts" || tab == "daily" || tab == "guess") {
        kind = QStringLiteral("songs");
    } else {
        done(page({}, offset, QStringLiteral("Apple Music 公开目录只提供热门歌曲与热门专辑")), {});
        return;
    }
    m_catalog.charts(kind, kChartLimit, [done, offset, kind](QJsonObject body, QString error) {
        if (!error.isEmpty()) {
            done({}, error);
            return;
        }
        const auto results = body.value("feed").toObject().value("results").toArray();
        done(page(AppleData::rssItems(results,
                                      kind == "albums" ? QStringLiteral("album") : QStringLiteral("song")),
                  offset),
             {});
    });
}

/** Apple 没有开放歌词接口；逐行歌词取自公开的 LRCLIB，无需任何令牌。 */
void AppleMusic::setWebLibrary(WebLibrary provider) {
    m_webLibrary = std::move(provider);
}

void AppleMusic::setLyricProvider(LyricProvider provider) {
    m_lyricProvider = std::move(provider);
}

/** 把 LRC 与翻译 LRC 合成一份歌词文档；翻译按时间戳对齐到对应行。 */
QJsonObject lyricsDocumentFromLrc(const QString &lrc, const QString &translated) {
    static const QRegularExpression stamp(
        QStringLiteral("\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"));
    const auto parse = [](const QString &text) {
        QMap<qint64, QString> entries;
        for (const auto &raw : text.split('\n')) {
            const auto match = stamp.match(raw);
            if (!match.hasMatch()) {
                continue;
            }
            auto fraction = 0;
            const auto digits = match.captured(3);
            if (!digits.isEmpty()) {
                fraction = digits.toInt();
                if (digits.size() == 1) {
                    fraction *= 100;
                } else if (digits.size() == 2) {
                    fraction *= 10;
                }
            }
            const auto ms =
                (match.captured(1).toInt() * 60 + match.captured(2).toInt()) * 1000 + fraction;
            const auto line = raw.mid(match.capturedEnd()).trimmed();
            if (!line.isEmpty()) {
                entries.insert(ms, line);
            }
        }
        return entries;
    };
    const auto original = parse(lrc);
    if (original.isEmpty()) {
        return {};
    }
    const auto translation = parse(translated);
    QJsonArray lines;
    QStringList plain;
    for (auto it = original.begin(); it != original.end(); ++it) {
        const auto text = it.value();
        plain.append(text);
        QJsonObject line{{"startMs", double(it.key())}, {"text", text}};
        // 译文放独立字段：歌词页会以更小字号显示，不和原文挤在一行。
        if (translation.contains(it.key())) {
            line["translation"] = translation.value(it.key());
        }
        lines.append(line);
    }
    return QJsonObject{{"version", 1},
                       {"timing", QStringLiteral("line")},
                       {"lines", lines},
                       {"plainText", plain.join(QLatin1Char('\n'))},
                       {"offsetMs", 0.0},
                       {"source", QStringLiteral("now-playing")}};
}

void AppleMusic::lyrics(QJsonObject track, Done done) {
    // 本地服务聚合了多个歌词源并带翻译，优先使用。
    if (m_lyricProvider) {
        const auto extra = m_lyricProvider();
        const auto document =
            lyricsDocumentFromLrc(extra.value("lrc").toString(), extra.value("translated").toString());
        if (!qEnvironmentVariableIsEmpty("RHINE_MEDIA_SESSION_LOG")) {
            qWarning("apple-lyrics: provider lrc=%lld docLines=%lld",
                     qlonglong(extra.value("lrc").toString().size()),
                     qlonglong(document.value("lines").toArray().size()));
        }
        if (!document.isEmpty()) {
            done(document, {});
            return;
        }
    }
    const auto title = track.value("title").toString();
    auto artist = track.value("artist").toString();
    // 系统会话可能把「艺人 — 专辑」塞在艺人字段里，检索前先拆干净。
    const auto dash = artist.indexOf(QStringLiteral(" — "));
    if (dash > 0) {
        artist = artist.left(dash).trimmed();
    }
    // 「A & B / C」这类多人署名会让歌词库匹配失败，取第一位主艺人再查。
    static const QRegularExpression separators(QStringLiteral("\\s*(?:&|/|、|，|,|\\bfeat\\.?)\\s*"));
    const auto leaders = artist.split(separators, Qt::SkipEmptyParts);
    if (!leaders.isEmpty()) {
        artist = leaders.first().trimmed();
    }
    if (title.isEmpty()) {
        done({}, QStringLiteral("没有可查询的歌词信息"));
        return;
    }
    auto attempts = std::make_shared<int>(0);
    auto fetch = std::make_shared<std::function<void()>>();
    *fetch = [this, done, title, artist, attempts, fetch] {
        QUrl url(QStringLiteral("https://lrclib.net/api/search"));
        QUrlQuery query;
        // 第二次改用整串关键词，容忍艺人或标题的书写差异。
        if (*attempts == 0) {
            query.addQueryItem(QStringLiteral("track_name"), title);
            if (!artist.isEmpty()) {
                query.addQueryItem(QStringLiteral("artist_name"), artist);
            }
        } else {
            query.addQueryItem(QStringLiteral("q"), title + QStringLiteral(" ") + artist);
        }
        ++(*attempts);
        url.setQuery(query);
        QNetworkRequest request(url);
        // LRCLIB 要求可识别的应用标识（含来源网址），过短的 UA 会被判为滥用而返回 503。
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("RhineMusicDatabase/1.0 "
                                         "(https://github.com/Tianyu-Liu-Aoba/"
                                         "Rhine-Music-Database-AM-Support)"));
        request.setTransferTimeout(12000);
        auto *reply = m_lyrics.get(request);
        connect(reply, &QNetworkReply::finished, this, [done, reply, attempts, fetch] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                done({}, QStringLiteral("歌词服务不可用，请稍后重试"));
                return;
            }
            const auto results = QJsonDocument::fromJson(reply->readAll()).array();
            const auto document = lyricsDocument(results);
            if (!document.isEmpty()) {
                done(document, {});
                return;
            }
            if (*attempts < 2) {
                (*fetch)();
                return;
            }
            done({}, QStringLiteral("暂无歌词"));
        });
    };
    (*fetch)();
}

QJsonObject AppleMusic::capabilities() {
    return QJsonObject{{"catalog", true}, {"preview", true},    {"playback", false},
                       {"lyrics", false}, {"mutations", false}, {"library", false}};
}

void AppleMusic::search(QString term, QString kind, int offset, Done done) {
    const auto entity = entityOf(kind);
    if (entity.isEmpty()) {
        done({}, kind == "playlist" ? QStringLiteral("Apple Music 公开目录没有歌单，暂不支持歌单检索")
                                    : QStringLiteral("Apple Music 不支持此搜索类别"));
        return;
    }
    m_catalog.search(term, entity, kSearchLimit, [done, offset](QJsonObject body, QString error) {
        if (!error.isEmpty()) {
            done({}, error);
            return;
        }
        const auto values = AppleData::items(body.value("results").toArray());
        auto result = page(values, offset);
        if (values.size() >= kSearchLimit) {
            result["notice"] =
                QStringLiteral("公开目录最多返回 %1 条结果，可换用更精确的关键词").arg(kSearchLimit);
        }
        done(result, {});
    });
}

void AppleMusic::songs(const QJsonArray &ids, Done done) {
    QStringList values;
    for (const auto &id : ids) {
        const auto value = id.toString();
        if (!value.isEmpty()) {
            values.append(value);
        }
    }
    if (values.isEmpty()) {
        done(QJsonArray{}, {});
        return;
    }
    m_catalog.lookup(values, QStringLiteral("song"), kLookupLimit,
                     [done, values](QJsonObject body, QString error) {
                         if (!error.isEmpty()) {
                             done({}, error);
                             return;
                         }
                         const auto items = AppleData::items(body.value("results").toArray());
                         QHash<QString, QJsonObject> byID;
                         for (const auto &item : items) {
                             const auto value = item.toObject();
                             byID.insert(value.value("id").toString(), value);
                         }
                         QJsonArray ordered;
                         for (const auto &id : values) {
                             if (byID.contains(id)) {
                                 ordered.append(byID.value(id));
                             }
                         }
                         done(ordered, {});
                     });
}

/** 专辑与艺人详情用一次 Lookup 取得容器与其子项，避免串行多次请求。 */
void AppleMusic::detail(QJsonObject item, Done done) {
    const auto kind = item.value("kind").toString();
    const auto id = item.value("id").toString();
    if (id.isEmpty()) {
        // 榜单条目没有专辑 ID：当成"没有更多详情"返回，避免界面显示读取失败。
        auto value = item;
        value["description"] =
            value.value("description").toString(QStringLiteral("公开目录没有该条目的更多详情"));
        done(QJsonArray{value, QJsonArray{}, QJsonArray{}}, {});
        return;
    }
    if (kind == "song") {
        m_catalog.lookup({id}, QStringLiteral("song"), 1, [done, id](QJsonObject body, QString error) {
            if (!error.isEmpty()) {
                done({}, error);
                return;
            }
            const auto values = AppleData::items(body.value("results").toArray());
            if (values.isEmpty()) {
                done({}, QStringLiteral("Apple Music 目录中没有该曲目"));
                return;
            }
            done(QJsonArray{values.first(), values, QJsonArray{id}}, {});
        });
        return;
    }
    if (kind == "album") {
        m_catalog.lookup({id}, QStringLiteral("song"), kLookupLimit, [done](QJsonObject body, QString error) {
            if (!error.isEmpty()) {
                done({}, error);
                return;
            }
            QJsonObject album;
            QJsonArray songs;
            for (const auto &entry : body.value("results").toArray()) {
                const auto value = AppleData::item(entry.toObject());
                if (value.isEmpty()) {
                    continue;
                }
                if (album.isEmpty() && value.value("kind").toString() == "album") {
                    album = value;
                    continue;
                }
                if (value.value("kind").toString() == "song") {
                    songs.append(value);
                }
            }
            if (album.isEmpty()) {
                done({}, QStringLiteral("Apple Music 目录中没有该专辑"));
                return;
            }
            done(QJsonArray{album, songs, idsOf(songs)}, {});
        });
        return;
    }
    if (kind == "artist") {
        m_catalog.lookup({id}, QStringLiteral("album"), kLookupLimit,
                         [done](QJsonObject body, QString error) {
                             if (!error.isEmpty()) {
                                 done({}, error);
                                 return;
                             }
                             QJsonObject artist;
                             QJsonArray albums;
                             for (const auto &entry : body.value("results").toArray()) {
                                 const auto value = AppleData::item(entry.toObject());
                                 if (value.isEmpty()) {
                                     continue;
                                 }
                                 if (artist.isEmpty() && value.value("kind").toString() == "artist") {
                                     artist = value;
                                     continue;
                                 }
                                 if (value.value("kind").toString() == "album") {
                                     albums.append(value);
                                 }
                             }
                             if (artist.isEmpty()) {
                                 done({}, QStringLiteral("Apple Music 目录中没有该艺人"));
                                 return;
                             }
                             artist["trackCount"] = int(albums.size());
                             done(QJsonArray{artist, QJsonArray{}, QJsonArray{}}, {});
                         });
        return;
    }
    done({}, QStringLiteral("Apple Music 不支持该条目类型"));
}

void AppleMusic::artistAlbums(QString id, int offset, Done done) {
    if (id.isEmpty()) {
        done({}, QStringLiteral("缺少艺人 ID"));
        return;
    }
    m_catalog.lookup({id}, QStringLiteral("album"), kLookupLimit,
                     [done, offset](QJsonObject body, QString error) {
                         if (!error.isEmpty()) {
                             done({}, error);
                             return;
                         }
                         QJsonArray albums;
                         for (const auto &entry : body.value("results").toArray()) {
                             const auto value = AppleData::item(entry.toObject());
                             if (value.value("kind").toString() == "album") {
                                 albums.append(value);
                             }
                         }
                         done(page(albums, offset), {});
                     });
}

void AppleMusic::trackPage(QJsonObject item, int offset, Done done) {
    if (item.value("kind").toString() != "album") {
        done(page({}, offset), {});
        return;
    }
    const auto id = item.value("id").toString();
    if (id.isEmpty()) {
        done({}, QStringLiteral("缺少专辑 ID"));
        return;
    }
    m_catalog.lookup({id}, QStringLiteral("song"), kLookupLimit,
                     [done, offset](QJsonObject body, QString error) {
                         if (!error.isEmpty()) {
                             done({}, error);
                             return;
                         }
                         QJsonArray songs;
                         for (const auto &entry : body.value("results").toArray()) {
                             const auto value = AppleData::item(entry.toObject());
                             if (value.value("kind").toString() == "song") {
                                 songs.append(value);
                             }
                         }
                         done(page(songs, offset), {});
                     });
}

/** 公开目录只交付 30 秒试听片段；整曲播放需要在 Apple Music 应用内完成。 */
void AppleMusic::playback(QJsonObject track, Done done) {
    const auto id = track.value("id").toString();
    const auto preview = track.value("preview").toString();
    const auto deliver = [done](const QString &url) {
        done(QJsonObject{{"url", url},
                         {"type", QStringLiteral("m4a")},
                         {"preview", true},
                         {"source", QStringLiteral("apple")},
                         {"notice", QStringLiteral("Apple Music 仅提供 30 秒试听")}},
             {});
    };
    if (!preview.isEmpty()) {
        deliver(preview);
        return;
    }
    if (id.isEmpty()) {
        done({}, QStringLiteral("此曲目没有试听片段"));
        return;
    }
    m_catalog.lookup({id}, QStringLiteral("song"), 1, [done, deliver](QJsonObject body, QString error) {
        if (!error.isEmpty()) {
            done({}, error);
            return;
        }
        for (const auto &entry : body.value("results").toArray()) {
            const auto value = AppleData::item(entry.toObject());
            const auto url = value.value("preview").toString();
            if (!url.isEmpty()) {
                deliver(url);
                return;
            }
        }
        done({}, QStringLiteral("此曲目没有试听片段"));
    });
}
