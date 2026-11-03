#include "AppleData.h"
#include "MusicData.h"
#include <QRegularExpression>

namespace {
// 公开目录把封面固定为 100x100；替换尺寸后缀即可取得同一张图的高清版本。
const QRegularExpression &artworkPattern() {
    static const QRegularExpression pattern(QStringLiteral("/(\\d+)x(\\d+)([a-z]{0,4})\\."));
    return pattern;
}

QString dateOf(const QJsonObject &raw) {
    const auto value = raw.value("releaseDate").toString();
    return value.size() >= 10 ? value.left(10) : QString();
}
}

namespace AppleData {
QString identifier(const QJsonObject &raw, const char *key) {
    const auto value = raw.value(QLatin1String(key));
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toInteger());
    }
    return {};
}

QString artwork(QString url, int size) {
    if (url.isEmpty()) {
        return {};
    }
    const auto replacement = QStringLiteral("/%1x%1\\3.").arg(QString::number(size));
    url.replace(artworkPattern(), replacement);
    return url;
}

QString kindOf(const QJsonObject &raw) {
    const auto wrapper = raw.value("wrapperType").toString();
    if (wrapper == "track" || raw.value("kind").toString() == "song") {
        return QStringLiteral("song");
    }
    if (wrapper == "collection") {
        return QStringLiteral("album");
    }
    if (wrapper == "artist") {
        return QStringLiteral("artist");
    }
    return {};
}

QJsonObject item(const QJsonObject &raw) {
    const auto kind = kindOf(raw);
    if (kind == "song") {
        const auto id = identifier(raw, "trackId");
        const auto title = raw.value("trackName").toString();
        if (id.isEmpty() || title.isEmpty()) {
            return {};
        }
        auto value =
            MusicData::item(kind, id, title, raw.value("artistName").toString(), QStringLiteral("apple"));
        const auto artistID = identifier(raw, "artistId");
        value["artists"] =
            QJsonArray{QJsonObject{{"id", artistID}, {"name", raw.value("artistName").toString()}}};
        value["album"] = raw.value("collectionName").toString();
        value["albumID"] = identifier(raw, "collectionId");
        value["duration"] = raw.value("trackTimeMillis").toDouble();
        value["cover"] = artwork(raw.value("artworkUrl100").toString());
        value["sourceURL"] = raw.value("trackViewUrl").toString();
        value["date"] = dateOf(raw);
        value["trackNumber"] = raw.value("trackNumber").toInt();
        value["genre"] = raw.value("primaryGenreName").toString();
        value["preview"] = raw.value("previewUrl").toString();
        value["appleType"] = QStringLiteral("song");
        return value;
    }
    if (kind == "album") {
        const auto id = identifier(raw, "collectionId");
        const auto title = raw.value("collectionName").toString();
        if (id.isEmpty() || title.isEmpty()) {
            return {};
        }
        auto value =
            MusicData::item(kind, id, title, raw.value("artistName").toString(), QStringLiteral("apple"));
        const auto artistID = identifier(raw, "artistId");
        value["artists"] =
            QJsonArray{QJsonObject{{"id", artistID}, {"name", raw.value("artistName").toString()}}};
        value["album"] = title;
        value["albumID"] = id;
        value["trackCount"] = raw.value("trackCount").toInt();
        value["cover"] = artwork(raw.value("artworkUrl100").toString());
        value["sourceURL"] = raw.value("collectionViewUrl").toString();
        value["date"] = dateOf(raw);
        value["genre"] = raw.value("primaryGenreName").toString();
        value["appleType"] = QStringLiteral("album");
        return value;
    }
    if (kind == "artist") {
        const auto id = identifier(raw, "artistId");
        const auto title = raw.value("artistName").toString();
        if (id.isEmpty() || title.isEmpty()) {
            return {};
        }
        auto value = MusicData::item(kind, id, title, title, QStringLiteral("apple"));
        value["artists"] = QJsonArray{QJsonObject{{"id", id}, {"name", title}}};
        value["sourceURL"] = raw.value("artistLinkUrl").toString();
        value["genre"] = raw.value("primaryGenreName").toString();
        value["appleType"] = QStringLiteral("artist");
        return value;
    }
    return {};
}

QJsonArray items(const QJsonArray &results) {
    QJsonArray values;
    for (const auto &entry : results) {
        const auto value = item(entry.toObject());
        if (!value.isEmpty()) {
            values.append(value);
        }
    }
    return values;
}
}
