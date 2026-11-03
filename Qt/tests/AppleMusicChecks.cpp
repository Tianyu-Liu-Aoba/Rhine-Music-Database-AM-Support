#include "AppleData.h"
#include "AppleMusic.h"
#include "MusicData.h"
#include "PlaybackRules.h"
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

/**
 * Apple Music 公开目录的离线回归。
 * 只覆盖条目映射、分页边界与只读接口的明确拒绝；真实目录请求与试听不进入常驻测试。
 */
namespace {
QJsonObject songResult() {
    return QJsonObject{{"wrapperType", "track"},
                       {"kind", "song"},
                       {"trackId", 1890100604},
                       {"trackName", "至冬 (Tale Version)"},
                       {"artistId", 1648683265},
                       {"artistName", "HOYO-MiX"},
                       {"collectionId", 1890100599},
                       {"collectionName", "原神-至冬"},
                       {"trackTimeMillis", 210000},
                       {"artworkUrl100", "https://is1-ssl.mzstatic.com/image/thumb/Music/abc/100x100bb.jpg"},
                       {"trackViewUrl", "https://music.apple.com/gb/album/id1890100604"},
                       {"previewUrl", "https://audio-ssl.itunes.apple.com/itunes-assets/x.m4a"},
                       {"releaseDate", "2026-10-21T07:00:00Z"},
                       {"trackNumber", 1},
                       {"primaryGenreName", "Soundtrack"}};
}

QJsonObject albumResult() {
    return QJsonObject{{"wrapperType", "collection"},
                       {"collectionType", "Album"},
                       {"collectionId", 1890100599},
                       {"collectionName", "原神-至冬"},
                       {"artistId", 1648683265},
                       {"artistName", "HOYO-MiX"},
                       {"trackCount", 4},
                       {"artworkUrl100", "https://is1-ssl.mzstatic.com/image/thumb/Music/abc/100x100bb.jpg"},
                       {"collectionViewUrl", "https://music.apple.com/gb/album/id1890100599"},
                       {"releaseDate", "2026-10-21T07:00:00Z"},
                       {"primaryGenreName", "Soundtrack"}};
}

QJsonObject artistResult() {
    return QJsonObject{{"wrapperType", "artist"},
                       {"artistType", "Artist"},
                       {"artistId", 1648683265},
                       {"artistName", "HOYO-MiX"},
                       {"artistLinkUrl", "https://music.apple.com/gb/artist/1648683265"},
                       {"primaryGenreName", "Soundtrack"}};
}
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    int testCount = 0;
    bool passed = true;
    const auto check = [&](bool value, const char *name) {
        ++testCount;
        if (!value) {
            qCritical() << "Apple Music test failed:" << name;
            passed = false;
        }
    };

    const auto song = AppleData::item(songResult());

    // 播放条发出 prev，系统会话只认 previous：两者必须映射一致。
    check(PlaybackRules::sessionAction(QStringLiteral("prev")) == QStringLiteral("previous"),
          "player:prev maps to the session previous action");
    check(PlaybackRules::sessionAction(QStringLiteral("next")) == QStringLiteral("next"),
          "next passes through unchanged");
    check(PlaybackRules::sessionAction(QStringLiteral("toggle")) == QStringLiteral("toggle"),
          "toggle passes through unchanged");

    // 歌词候选必须标题 + 艺人同时相符；只按艺人会串到同一艺人的其它曲目。
    const auto artists = [](const QString &name) {
        return QJsonArray{QJsonObject{{"name", name}}};
    };
    check(PlaybackRules::lyricMatches(QStringLiteral("Harvest (Shihoru ver.)"),
                                      QStringLiteral("(K)NoW_NAME"), QStringLiteral("Harvest"),
                                      artists(QStringLiteral("(K)NoW_NAME"))),
          "version suffix still matches the plain title");
    check(!PlaybackRules::lyricMatches(QStringLiteral("Ghost"), QStringLiteral("tayori"),
                                       QStringLiteral("Into You"),
                                       artists(QStringLiteral("tayori"))),
          "a different title by the same artist is rejected");
    check(!PlaybackRules::lyricMatches(QStringLiteral("Harvest"), QStringLiteral("A"),
                                       QStringLiteral("Harvest"),
                                       artists(QStringLiteral("B"))),
          "a matching title by another artist is rejected");
    check(song["id"] == "1890100604" && song["kind"] == "song" && song["provider"] == "apple",
          "song keeps a decimal id and the apple provider");
    check(song["key"] == "apple:song:1890100604", "song key is provider-scoped");
    check(song["album"] == "原神-至冬" && song["albumID"] == "1890100599", "song carries album identity");
    check(song["duration"].toDouble() == 210000, "duration stays in milliseconds");
    check(song["cover"].toString().contains("/600x600bb.jpg"), "cover is upscaled from the 100px original");
    check(song["date"] == "2026-10-21", "release date is trimmed to a day");
    check(song["preview"].toString().startsWith("https://audio-ssl"), "preview url is preserved");
    check(song["artists"].toArray().size() == 1, "artist entry is filled");
    check(MusicData::time(song) == "03:30", "duration renders as mm:ss");

    const auto album = AppleData::item(albumResult());
    check(album["kind"] == "album" && album["trackCount"].toInt() == 4, "album keeps its track count");
    const auto artist = AppleData::item(artistResult());
    check(artist["kind"] == "artist" && artist["id"] == "1648683265", "artist maps by artist id");

    check(AppleData::item(QJsonObject{{"wrapperType", "musicVideo"}}).isEmpty(),
          "unsupported wrappers are skipped");
    check(AppleData::item(QJsonObject{{"wrapperType", "track"}}).isEmpty(), "missing trackId is rejected");
    check(AppleData::items(QJsonArray{artistResult(), songResult(), QJsonObject{}}).size() == 2,
          "items() drops unmappable entries");

    // 目录地区跟随系统区域；公开接口下的 cn 店面没有歌曲与专辑，不能被当作默认值。
    const auto storefront = AppleMusic::defaultStorefront();
    static const QRegularExpression pattern(QStringLiteral("^[a-z]{2}$"));
    check(pattern.match(storefront).hasMatch(), "default storefront is a two letter region");
    check(storefront != "cn", "mainland storefront is avoided for catalog queries");

    AppleMusic music;
    const auto call = [&](const QString &operation, const QJsonArray &arguments) {
        QJsonObject boxed;
        music.request(operation, arguments, [&boxed](QJsonObject body, QString) { boxed = body; });
        return boxed;
    };
    const auto capabilities = call("capabilities", {}).value("value").toObject();
    check(capabilities["preview"].toBool() && !capabilities["mutations"].toBool(),
          "capabilities expose preview only");

    const auto account = call("account", {}).value("value").toArray();
    check(account.size() == 2 && account.at(0) == "apple-local", "account is a fixed local marker");

    const auto lyrics = call("lyrics", QJsonArray{QJsonObject{}});
    check(lyrics.value("code").toInt() == 400 && !lyrics.value("message").toString().isEmpty(),
          "lyrics are refused with a message");

    const auto mutate = call("mutate", QJsonArray{QStringLiteral("/like"), QJsonObject{}});
    check(mutate.value("code").toInt() == 400, "writes are refused");

    const auto personal =
        call("userPlaylists", QJsonArray{QStringLiteral("apple-local"), 0}).value("value").toObject();
    check(personal.value("items").toArray().isEmpty() && !personal.value("notice").toString().isEmpty(),
          "personal music returns an explicit notice");

    const auto playlist = call("search", QJsonArray{QStringLiteral("x"), QStringLiteral("playlist"), 0});
    check(playlist.value("code").toInt() == 400, "playlist search is refused");

    if (!passed) {
        return 1;
    }
    qInfo() << "Apple Music offline regression passed:" << testCount << "checks";
    return 0;
}
