#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

/**
 * Apple 公开目录条目到 MusicData 的纯数据转换。
 * 不读取网络、不保存状态，便于离线回归直接比对。
 */
namespace AppleData {
QString identifier(const QJsonObject &raw, const char *key);
QString artwork(QString url, int size = 600);
QString kindOf(const QJsonObject &raw);
QJsonObject item(const QJsonObject &raw);
QJsonArray items(const QJsonArray &results);
// Marketing RSS 的条目字段更少（无专辑名、时长、试听地址），只用已知字段做映射。
QJsonObject rssItem(const QJsonObject &raw, const QString &kind);
QJsonArray rssItems(const QJsonArray &results, const QString &kind);
}
