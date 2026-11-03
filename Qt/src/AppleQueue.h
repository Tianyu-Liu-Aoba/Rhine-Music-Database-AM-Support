#pragma once
#include <QJsonArray>
#include <QObject>
#include <memory>

/**
 * 通过 UI Automation 读取 Apple Music 的「Playing Next」队列。
 * 只读：不点击、不修改官方应用状态；跳转由调用方用上一首/下一首实现。
 * 非 Windows 平台为空实现。
 */
class AppleQueue : public QObject {
    Q_OBJECT
  public:
    explicit AppleQueue(QObject *parent = nullptr);
    ~AppleQueue() override;
    // 异步读取队列；同一时刻只保留一次在途刷新。
    void refresh();
    QJsonArray items() const;

  signals:
    void changed();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
