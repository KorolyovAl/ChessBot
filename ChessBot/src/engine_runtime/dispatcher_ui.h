#pragma once
#include <QObject>
#include <QMetaObject>
#include <QThread>

#include <functional>

namespace engine_runtime {

class UiDispatcher {
public:
    UiDispatcher(QObject& obj) : anchor_(&obj) {}

    void Post(std::function<void()> fn) {
        if (anchor_ == nullptr) {
            return;
        }

        if (anchor_->thread() == QThread::currentThread()) {
            fn();
        }

        QMetaObject::invokeMethod(anchor_, [fn = std::move(fn)]() mutable {
                                    fn();
                                }, Qt::ConnectionType::QueuedConnection);
    }

private:
    QObject* anchor_;
};

} // namespace engine_runtime
