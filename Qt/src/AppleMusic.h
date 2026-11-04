#pragma once
#include "AppleCatalog.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <functional>

/**
 * Apple Music 门面：公开目录检索与 30 秒试听。
 * 只交付普通 Qt JSON，不持有账号、凭据或界面状态；写操作一律明确拒绝。
 */
class AppleMusic : public QObject {
    Q_OBJECT
  public:
    using Reply = std::function<void(QJsonObject body, QString error)>;
    explicit AppleMusic(QObject *parent = nullptr);
    void request(const QString &operation, const QJsonArray &arguments, Reply reply);
    void setStorefront(const QString &value);
    // 本地 Now Playing 服务提供的歌词（含翻译）；为空时退回 LRCLIB。
    using LyricProvider = std::function<QJsonObject()>;
    void setLyricProvider(LyricProvider provider);
    // 跟随系统区域的目录地区；中国大陆店面在公开接口下没有歌曲与专辑，改用繁体中文店面。
    static QString defaultStorefront();
    void reset();

  private:
    using Done = std::function<void(QJsonValue value, QString error)>;
    void search(QString term, QString kind, int offset, Done done);
    void songs(const QJsonArray &ids, Done done);
    void detail(QJsonObject item, Done done);
    void artistAlbums(QString id, int offset, Done done);
    void trackPage(QJsonObject item, int offset, Done done);
    void playback(QJsonObject track, Done done);
    void discover(const QString &tab, int offset, Done done);
    void lyrics(QJsonObject track, Done done);
    static QJsonObject page(const QJsonArray &values, int offset, const QString &notice = {});
    static QJsonObject capabilities();
    quint64 m_generation = 0;
    AppleCatalog m_catalog;
    LyricProvider m_lyricProvider;
    // 歌词走公开的 LRCLIB，与目录请求分开管理，避免共用限速队列。
    QNetworkAccessManager m_lyrics;
};
