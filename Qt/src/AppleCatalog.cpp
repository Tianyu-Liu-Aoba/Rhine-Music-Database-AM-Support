#include "AppleCatalog.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>
#include <algorithm>

namespace {
// iTunes 公开查询不做突发请求：串行发送，间隔不小于 3.1 秒。
constexpr qint64 kInterval = 3100;
constexpr qint64 kCacheLifetime = 5 * 60 * 1000;
constexpr int kCacheLimit = 100;
constexpr int kTimeout = 20000;
constexpr int kMaximumLimit = 200;
}

AppleCatalog::AppleCatalog(QObject *parent) : QObject(parent) {
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &AppleCatalog::pump);
}

/** 目录查询只返回普通 JSON；调用方负责代数隔离，这里不保存业务状态。 */
void AppleCatalog::search(QString term, QString entity, int limit, Reply reply) {
    const auto text = term.trimmed();
    if (text.isEmpty()) {
        reply({}, QStringLiteral("请输入关键词"));
        return;
    }
    QUrl url(QStringLiteral("https://itunes.apple.com/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), text);
    query.addQueryItem(QStringLiteral("country"), m_storefront);
    query.addQueryItem(QStringLiteral("media"), QStringLiteral("music"));
    query.addQueryItem(QStringLiteral("entity"), entity);
    query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, kMaximumLimit)));
    url.setQuery(query);
    enqueue(url, std::move(reply));
}

void AppleCatalog::lookup(QStringList ids, QString entity, int limit, Reply reply) {
    QStringList values;
    for (const auto &id : ids) {
        const auto value = id.trimmed();
        if (!value.isEmpty() && !values.contains(value)) {
            values.append(value);
        }
        if (values.size() >= kMaximumLimit) {
            break;
        }
    }
    if (values.isEmpty()) {
        reply({}, QStringLiteral("缺少目录 ID"));
        return;
    }
    QUrl url(QStringLiteral("https://itunes.apple.com/lookup"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), values.join(QLatin1Char(',')));
    query.addQueryItem(QStringLiteral("country"), m_storefront);
    if (!entity.isEmpty()) {
        query.addQueryItem(QStringLiteral("entity"), entity);
        query.addQueryItem(QStringLiteral("limit"), QString::number(std::clamp(limit, 1, kMaximumLimit)));
    }
    url.setQuery(query);
    enqueue(url, std::move(reply));
}

/** 公开榜单与新品来自 Apple Marketing RSS：无需令牌，条目字段比 Search 接口少。 */
void AppleCatalog::charts(QString kind, int limit, Reply reply) {
    static const QRegularExpression shape(QStringLiteral("^(songs|albums)$"));
    if (!shape.match(kind).hasMatch()) {
        reply({}, QStringLiteral("Apple Music 没有该榜单类别"));
        return;
    }
    QUrl url(QStringLiteral("https://rss.applemarketingtools.com/api/v2/%1/music/most-played/%2/%3.json")
                 .arg(m_storefront)
                 .arg(std::clamp(limit, 1, 100))
                 .arg(kind));
    enqueue(url, std::move(reply));
}

void AppleCatalog::setStorefront(QString value) {
    auto normalized = value.trimmed().toLower();
    static const QRegularExpression pattern(QStringLiteral("^[a-z]{2}$"));
    if (!pattern.match(normalized).hasMatch()) {
        normalized = QStringLiteral("cn");
    }
    if (normalized != m_storefront) {
        m_storefront = normalized;
        reset();
    }
}

/** 切换音乐来源后旧结果不能再交付，也不该继续占用缓存。 */
void AppleCatalog::reset() {
    m_cache.clear();
    m_queue.clear();
    m_timer.stop();
}

void AppleCatalog::enqueue(QUrl url, Reply reply) {
    const auto key = url.toString();
    const auto hit = m_cache.find(key);
    if (hit != m_cache.end() && hit->expiry > QDateTime::currentMSecsSinceEpoch()) {
        const auto body = hit->body;
        QTimer::singleShot(0, this, [reply = std::move(reply), body]() { reply(body, {}); });
        return;
    }
    m_queue.enqueue({url, std::move(reply)});
    pump();
}

/** 串行发送：同一时刻只有一个在途请求，失败也不并发重试。 */
void AppleCatalog::pump() {
    if (m_busy || m_queue.isEmpty()) {
        return;
    }
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const auto wait = m_lastStart + kInterval - now;
    if (wait > 0) {
        m_timer.start(int(std::min<qint64>(wait, kInterval)));
        return;
    }
    auto job = m_queue.dequeue();
    m_busy = true;
    m_lastStart = now;
    QNetworkRequest request(job.url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("RhineMusicDatabase/1.0"));
    request.setTransferTimeout(kTimeout);
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, job = std::move(job)]() mutable { finish(reply, std::move(job)); });
}

void AppleCatalog::finish(QNetworkReply *reply, Job job) {
    reply->deleteLater();
    m_busy = false;
    const auto key = job.url.toString();
    auto body = QJsonObject{};
    QString error;
    if (reply->error() != QNetworkReply::NoError) {
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 403 || status == 429) {
            error = QStringLiteral("Apple Music 目录请求过于频繁，请稍后重试");
        } else {
            error = QStringLiteral("Apple Music 目录请求失败，请检查网络后重试");
        }
    } else {
        const auto payload = QJsonDocument::fromJson(reply->readAll()).object();
        // Search/Lookup 返回 results 数组，Marketing RSS 返回 feed 对象；两者都接受，形状由调用方校验。
        if (!payload.value("results").isArray() && !payload.value("feed").isObject()) {
            error = QStringLiteral("Apple Music 目录返回格式无效");
        } else {
            body = payload;
            if (m_cache.size() >= kCacheLimit) {
                m_cache.clear();
            }
            m_cache.insert(key, Entry{QDateTime::currentMSecsSinceEpoch() + kCacheLifetime, body});
        }
    }
    job.reply(body, error);
    pump();
}
