#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

namespace engine_runtime {

template <typename Task>
class TaskQueue {
public:
    // Put a task into the queue and notify one waiting thread
    bool Push(Task t) {
        {
            std::unique_lock<std::mutex> lk{mt_};
            if (is_closed_) {
                return false;
            }
            tasks_.push(std::move(t));
        }

        consume_cv_.notify_one();
        return true;
    }

    // Take a task from the queue
    // Returns false if the queue is closed and empty
    bool Pop(Task& out) {
        std::unique_lock<std::mutex> lk{mt_};
        consume_cv_.wait(lk, [this] {
            return is_closed_ || !tasks_.empty();
        });

        if (tasks_.empty()) {
            return false;
        }

        out = std::move(tasks_.front());
        tasks_.pop();
        return true;
    }

    void Close() {
        {
            std::lock_guard<std::mutex> lk{mt_};
            is_closed_ = true;
        }
        consume_cv_.notify_all();
    }

private:
    std::queue<Task> tasks_;

    std::mutex mt_;
    std::condition_variable consume_cv_;
    bool is_closed_ = false;
};

} // namespace engine_runtime
