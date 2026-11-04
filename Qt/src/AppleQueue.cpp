#include "AppleQueue.h"
#include <QDebug>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <oleauto.h>
#include <uiautomation.h>
#endif

namespace {
#ifdef Q_OS_WIN
// U+2014 EM DASH: written as an escape so file encoding can never corrupt it.
const QString kSeparator = QString(QChar(0x2014));
// Carries the reference clock out of readQueue (it runs on a worker thread).
double g_referencePositionMs = -1;
QList<double> g_referenceTimes;

QString toText(BSTR value) {
    if (!value) {
        return {};
    }
    const QString text = QString::fromWCharArray(value, int(SysStringLen(value)));
    SysFreeString(value);
    return text;
}

CONTROLTYPEID controlType(IUIAutomationElement *element) {
    CONTROLTYPEID type = 0;
    if (element) {
        element->get_CurrentControlType(&type);
    }
    return type;
}

BSTR currentName(IUIAutomationElement *element) {
    BSTR name = nullptr;
    if (element) {
        element->get_CurrentName(&name);
    }
    return name;
}

// Duration text looks like "2 minutes, 58 seconds"; read numbers language-agnostically.
double parseDuration(const QString &text) {
    static const QRegularExpression digits(QStringLiteral("(\\d+)"));
    auto match = digits.globalMatch(text);
    QList<int> numbers;
    while (match.hasNext() && numbers.size() < 2) {
        numbers.append(match.next().captured(1).toInt());
    }
    if (numbers.size() < 2) {
        return 0;
    }
    return double(numbers.at(0) * 60 + numbers.at(1)) * 1000.0;
}

bool verbose() {
    static const bool enabled = !qEnvironmentVariableIsEmpty("RHINE_MEDIA_SESSION_LOG");
    return enabled;
}

QJsonArray collectItems(IUIAutomation *automation, IUIAutomationElement *window) {
    QJsonArray result;
    IUIAutomationCondition *trueCondition = nullptr;
    automation->CreateTrueCondition(&trueCondition);
    IUIAutomationElementArray *all = nullptr;
    window->FindAll(TreeScope_Descendants, trueCondition, &all);
    int count = 0;
    if (all) {
        all->get_Length(&count);
    }
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement *element = nullptr;
        if (FAILED(all->GetElement(i, &element)) || !element) {
            continue;
        }
        if (controlType(element) != UIA_ListItemControlTypeId) {
            element->Release();
            continue;
        }
        auto label = toText(currentName(element));
        QStringList fields;
        IUIAutomationElementArray *texts = nullptr;
        element->FindAll(TreeScope_Descendants, trueCondition, &texts);
        int textCount = 0;
        if (texts) {
            texts->get_Length(&textCount);
        }
        for (int k = 0; k < textCount && fields.size() < 3; ++k) {
            IUIAutomationElement *text = nullptr;
            if (FAILED(texts->GetElement(k, &text)) || !text) {
                continue;
            }
            if (controlType(text) == UIA_TextControlTypeId) {
                auto value = toText(currentName(text));
                if (!value.isEmpty()) {
                    fields.append(value);
                }
            }
            text->Release();
        }
        if (texts) {
            texts->Release();
        }
        element->Release();
        if (label.isEmpty()) {
            continue;
        }
        // 时长可能是一个独立 Text，也可能只出现在整条 label 的末尾（主窗口列表）。
        auto duration = fields.size() >= 3 ? parseDuration(fields.at(2)) : 0.0;
        if (duration <= 0) {
            static const QRegularExpression tail(
                QStringLiteral("(\\d{1,3})\\s*[:：]\\s*(\\d{2})\\s*$"));
            auto match = tail.match(label);
            if (match.hasMatch()) {
                duration = (match.captured(1).toLongLong() * 60 + match.captured(2).toLongLong()) * 1000.0;
            }
        }
        if (duration <= 0) {
            // 迷你播放器的队列行没有时长文本，仍要保留：没有时长不等于不是曲目。
            duration = -1;
        }
        QJsonObject item{{"index", result.size()}, {"label", label}};
        if (duration > 0) {
            item["duration"] = duration;
        }
        if (fields.size() >= 1) {
            item["title"] = fields.at(0);
        }
        if (fields.size() >= 2) {
            const auto rest = fields.at(1);
            const auto split = rest.indexOf(kSeparator);
            if (split > 0) {
                item["artist"] = rest.left(split).trimmed();
                item["album"] = rest.mid(split + 1).trimmed();
            } else {
                item["artist"] = rest;
            }
        }
        result.append(item);
    }
    if (all) {
        all->Release();
    }
    trueCondition->Release();
    return result;
}

QJsonArray readQueue() {
    QJsonArray result;
    IUIAutomation *automation = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(IUIAutomation), reinterpret_cast<void **>(&automation)))) {
        return result;
    }
    IUIAutomationElement *root = nullptr;
    automation->GetRootElement(&root);
    IUIAutomationCondition *trueCondition = nullptr;
    automation->CreateTrueCondition(&trueCondition);
    IUIAutomationElementArray *windows = nullptr;
    root->FindAll(TreeScope_Children, trueCondition, &windows);
    int count = 0;
    if (windows) {
        windows->get_Length(&count);
    }
    IUIAutomationElement *mini = nullptr;
    IUIAutomationElement *main = nullptr;
    for (int i = 0; i < count; ++i) {
        IUIAutomationElement *window = nullptr;
        if (FAILED(windows->GetElement(i, &window)) || !window) {
            continue;
        }
        const auto title = toText(currentName(window));
        if (!mini && title.contains(QStringLiteral("Mini Player"))) {
            mini = window;
            continue;
        }
        if (!main && title == QStringLiteral("Apple Music")) {
            main = window;
            continue;
        }
        window->Release();
    }
    double positionMs = -1;
    if (mini) {
        // The mini player shows the elapsed time as "M:SS"; use it as the reference clock.
        IUIAutomationElementArray *texts = nullptr;
        mini->FindAll(TreeScope_Descendants, trueCondition, &texts);
        int textCount = 0;
        if (texts) {
            texts->get_Length(&textCount);
        }
        static const QRegularExpression timePattern(QStringLiteral("^\\d{1,3}:\\d{2}$"));
        QList<double> times;
        for (int i = 0; i < textCount; ++i) {
            IUIAutomationElement *element = nullptr;
            if (FAILED(texts->GetElement(i, &element)) || !element) {
                continue;
            }
            if (controlType(element) == UIA_TextControlTypeId) {
                const auto text = toText(currentName(element));
                if (timePattern.match(text).hasMatch()) {
                    const auto parts = text.split(QLatin1Char(':'));
                    if (parts.size() == 2) {
                        // 队列行里也有时长文本，这里全部收集，由调用方挑最接近的。
                        times.append(
                            (parts.at(0).toLongLong() * 60 + parts.at(1).toLongLong()) * 1000.0);
                    }
                }
            }
            element->Release();
        }
        positionMs = times.isEmpty() ? -1 : times.first();
        g_referenceTimes = times;
        if (texts) {
            texts->Release();
        }
        result = collectItems(automation, mini);
        mini->Release();
    }
    if (verbose()) {
        qWarning("queue: reference position=%lld", qlonglong(positionMs));
    }
    g_referencePositionMs = positionMs;
    // The mini player is the clean source; the main window is only a fallback and
    // its list is filtered down to entries that carry a duration.
    if (result.isEmpty() && main) {
        result = collectItems(automation, main);
    }
    if (main) {
        main->Release();
    }
    if (windows) {
        windows->Release();
    }
    trueCondition->Release();
    root->Release();
    automation->Release();
    if (verbose()) {
        qWarning("queue: items=%lld", qlonglong(result.size()));
    }
    return result;
}
#endif
}

struct AppleQueue::Impl {
    mutable QMutex mutex;
    QJsonArray items;
    double positionMs = -1;
    QList<double> times;
    std::atomic_bool busy{false};
};

AppleQueue::AppleQueue(QObject *parent) : QObject(parent), m_impl(std::make_unique<Impl>()) {
}

AppleQueue::~AppleQueue() = default;

void AppleQueue::refresh() {
    if (m_impl->busy.exchange(true)) {
        return;
    }
    std::thread([this] {
        // 任何异常都必须复位 busy，否则队列会永久卡在"刷新中"。
        struct Reset {
            std::atomic_bool &flag;
            ~Reset() { flag = false; }
        } reset{m_impl->busy};
#ifdef Q_OS_WIN
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            return;
        }
        QJsonArray items;
        double reference = -1;
        QList<double> times;
        try {
            items = readQueue();
            reference = g_referencePositionMs;
            times = g_referenceTimes;
        } catch (...) {
        }
        CoUninitialize();
#else
        const QJsonArray items;
        const double reference = -1;
        const QList<double> times;
#endif
        bool updated = false;
        {
            QMutexLocker lock(&m_impl->mutex);
            if (m_impl->items != items) {
                m_impl->items = items;
                updated = true;
            }
            m_impl->positionMs = reference;
            m_impl->times = times;
        }
        if (updated) {
            QMetaObject::invokeMethod(this, [this] { emit changed(); }, Qt::QueuedConnection);
        }
    }).detach();
}

QJsonArray AppleQueue::items() const {
    QMutexLocker lock(&m_impl->mutex);
    return m_impl->items;
}

void AppleQueue::clear() {
    QMutexLocker lock(&m_impl->mutex);
    m_impl->items = {};
    m_impl->times.clear();
    m_impl->positionMs = -1;
}

double AppleQueue::positionMs(double expected) const {    QMutexLocker lock(&m_impl->mutex);
    if (m_impl->times.isEmpty()) {
        return m_impl->positionMs;
    }
    if (expected < 0) {
        return m_impl->times.first();
    }
    // 迷你播放器里既有进度也有队列时长，取最接近系统会话位置的那个。
    auto best = m_impl->times.first();
    for (const auto value : m_impl->times) {
        if (std::abs(value - expected) < std::abs(best - expected)) {
            best = value;
        }
    }
    return best;
}
