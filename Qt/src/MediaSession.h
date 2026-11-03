#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <memory>

/**
 * Windows 系统媒体会话（SMTC）镜像与传输控制。
 * WinRT 需要 MTA，因此工作线程独立初始化公寓；快照由对象线程同步读取。
 * 非 Windows 平台编译为空实现，调用方无需分支。
 */
class MediaSession : public QObject {
    Q_OBJECT
  public:
    explicit MediaSession(QObject *parent = nullptr);
    ~MediaSession() override;
    void start();
    void stop();
    QJsonObject snapshot() const;
    QByteArray cover() const;
    QString coverKey() const;
    // action: toggle / play / pause / next / previous / seek（value 为毫秒）
    void control(const QString &action, double value = 0);

  signals:
    void changed();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
