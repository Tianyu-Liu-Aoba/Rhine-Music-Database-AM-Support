#pragma once
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>

/**
 * Now Playing 服务的本地 HTTP 客户端（127.0.0.1:9863/api/query）。
 * 字段比 SMTC 更全（专辑、高清封面、进度），连不上时调用方退回 SMTC。
 */
class NowPlayingClient : public QObject {
    Q_OBJECT
  public:
    explicit NowPlayingClient(QObject *parent = nullptr);
    void start();
    void stop();
    bool available() const;
    QJsonObject state() const;
    // 封面异步下载：返回 key，图片字节通过 cover() 取。
    QString coverKey() const;
    QByteArray cover() const;
    QString lyricText() const;
    QString translatedLyric() const;

  signals:
    void changed();

  private:
    void poll();
    void fetchLyric(QString title, QString artist);
    void fetchCover(QString url);
    QNetworkAccessManager m_net;
    QTimer m_timer;
    QJsonObject m_state;
    QString m_coverKey;
    QByteArray m_cover;
    QString m_coverUrl;
    QString m_coverPending;
    QString m_lyricText;
    QString m_translatedLyric;
    QString m_lyricKey;
    bool m_lyricPending = false;
};
