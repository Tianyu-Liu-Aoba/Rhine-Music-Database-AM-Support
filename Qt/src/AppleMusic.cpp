#include "AppleMusic.h"
#include "AppleData.h"
#include <QLocale>
#include <QRegularExpression>
#include <algorithm>

namespace {
constexpr int kPageSize = 30;
constexpr int kSearchLimit = 100;
constexpr int kLookupLimit = 200;
constexpr int kChartLimit = 40;

QJsonValue argument(const QJsonArray &arguments, qsizetype index) {
    return index >= 0 && index < arguments.size() ? arguments.at(index) : QJsonValue();
}

QString entityOf(const QString &kind) {
    if (kind == "song") {
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
        done({}, QStringLiteral("Apple Music 不提供逐行歌词"));
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
    if (QStringList{"userPlaylists", "likedSongs", "likedAlbums", "librarySongs", "favoritePlaylists"}
            .contains(operation)) {
        done(page({}, 0,
                  QStringLiteral("Apple Music 公开目录不提供个人音乐与推荐，请在 Apple Music 应用中浏览")),
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
        done({}, QStringLiteral("缺少目录 ID"));
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
