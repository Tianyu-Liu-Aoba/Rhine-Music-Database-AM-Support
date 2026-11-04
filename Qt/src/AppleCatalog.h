#pragma once
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <functional>

class QNetworkReply;

/**
 * 公开目录边界：只使用 iTunes Search / Lookup 的匿名查询。
 * 不携带账号、令牌或 Cookie，串行限速并缓存短时效结果。
 */
class AppleCatalog : public QObject {
    Q_OBJECT
  public:
    using Reply = std::function<void(QJsonObject, QString)>;
    explicit AppleCatalog(QObject *parent = nullptr);
    void search(QString term, QString entity, int limit, Reply reply);
    void lookup(QStringList ids, QString entity, int limit, Reply reply);
    // 公开榜单与新品：Apple Marketing RSS，无令牌；条目字段比 Search 接口少。
    void charts(QString kind, int limit, Reply reply);
    QString storefront() const {
        return m_storefront;
    }
    void setStorefront(QString value);
    void reset();

  private:
    struct Entry {
        qint64 expiry = 0;
        QJsonObject body;
    };
    struct Job {
        QUrl url;
        Reply reply;
    };
    void enqueue(QUrl url, Reply reply);
    void pump();
    void finish(QNetworkReply *reply, Job job);
    QNetworkAccessManager m_network;
    QQueue<Job> m_queue;
    QHash<QString, Entry> m_cache;
    QTimer m_timer;
    qint64 m_lastStart = 0;
    bool m_busy = false;
    QString m_storefront = QStringLiteral("cn");
};
