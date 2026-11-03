#include "AppleQueue.h"
#include <QMetaObject>
#include <QMutex>
#include <QJsonObject>
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

// æ—¶é•¿å½¢å¦‚ "2 minutes, 58 seconds"ï¼›è¯­è¨€æ— å…³åœ°æŒ‰æ•°å­—é¡ºåºå–åˆ†ä¸Žç§’ã€‚
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

/** åœ¨çª—å£å­æ ‘é‡Œæ”¶é›†æ‰€æœ‰ ListItemï¼Œå¹¶å–å®ƒä»¬çš„æ–‡æœ¬å­å…ƒç´ ä½œä¸ºå­—æ®µã€‚ */
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
        // æ¡ç›®ä¸‹çš„ Text å­å…ƒç´ ç»™å‡ºå¹²å‡€å­—æ®µï¼šæ ‡é¢˜ã€è‰ºäºº â€” ä¸“è¾‘ã€æ—¶é•¿ã€‚
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
        QJsonObject item{{"index", result.size()}, {"label", label}};
        if (fields.size() >= 1) {
            item["title"] = fields.at(0);
        }
        if (fields.size() >= 2) {
            const auto rest = fields.at(1);
            const auto split = rest.indexOf(QStringLiteral(" â€” "));
            if (split > 0) {
                item["artist"] = rest.left(split).trimmed();
                item["album"] = rest.mid(split + 3).trimmed();
            } else {
                item["artist"] = rest;
            }
        }
        if (fields.size() >= 3) {
            item["duration"] = parseDuration(fields.at(2));
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
    // è¿·ä½ æ’­æ”¾å™¨åªæœ‰é˜Ÿåˆ—ï¼›æ‰¾ä¸åˆ°æ—¶é€€åˆ°ä¸»çª—å£ã€‚
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
    if (mini) {
        result = collectItems(automation, mini);
        mini->Release();
    }
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
    return result;
}
#endif
}

struct AppleQueue::Impl {
    mutable QMutex mutex;
    QJsonArray items;
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
#ifdef Q_OS_WIN
        if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
            m_impl->busy = false;
            return;
        }
        const auto items = readQueue();
        CoUninitialize();
#else
        const QJsonArray items;
#endif
        bool updated = false;
        {
            QMutexLocker lock(&m_impl->mutex);
            if (m_impl->items != items) {
                m_impl->items = items;
                updated = true;
            }
        }
        m_impl->busy = false;
        if (updated) {
            QMetaObject::invokeMethod(this, [this] { emit changed(); }, Qt::QueuedConnection);
        }
    }).detach();
}

QJsonArray AppleQueue::items() const {
    QMutexLocker lock(&m_impl->mutex);
    return m_impl->items;
}
