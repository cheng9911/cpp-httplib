#pragma once

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace rpf {

using EventHandler = std::function<void(const std::string& event, const nlohmann::json& data)>;

struct Subscription {
    uint64_t id;
    EventHandler handler;
};

class EventBus {
public:
    // 订阅事件
    uint64_t subscribe(const std::string& event, EventHandler handler) {
        std::unique_lock lock(mutex_);
        uint64_t id = next_id_++;
        handlers_[event].push_back({id, std::move(handler)});
        return id;
    }

    // 取消订阅
    void unsubscribe(uint64_t subscription_id) {
        std::unique_lock lock(mutex_);
        for (auto& [event, subs] : handlers_) {
            subs.erase(
                std::remove_if(subs.begin(), subs.end(),
                    [subscription_id](const Subscription& s) { return s.id == subscription_id; }),
                subs.end()
            );
        }
    }

    // 发布事件
    void publish(const std::string& event, const nlohmann::json& data = {}) {
        std::shared_lock lock(mutex_);
        auto it = handlers_.find(event);
        if (it != handlers_.end()) {
            for (const auto& sub : it->second) {
                sub.handler(event, data);
            }
        }
    }

    void clear() {
        std::unique_lock lock(mutex_);
        handlers_.clear();
    }

private:
    std::map<std::string, std::vector<Subscription>> handlers_;
    std::atomic<uint64_t> next_id_{0};
    mutable std::shared_mutex mutex_;
};

} // namespace rpf
