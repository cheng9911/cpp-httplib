#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>

namespace rpf {

class ServiceRegistry {
public:
    // 注册服务
    template<typename T>
    bool registerService(const std::string& name, std::shared_ptr<T> service) {
        std::unique_lock lock(mutex_);
        services_[name] = std::static_pointer_cast<void>(service);
        return true;
    }

    // 获取服务
    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second);
    }

    // 注销服务
    bool unregisterService(const std::string& name) {
        std::unique_lock lock(mutex_);
        return services_.erase(name) > 0;
    }

    // 查询
    bool hasService(const std::string& name) const {
        std::shared_lock lock(mutex_);
        return services_.count(name) > 0;
    }

    std::vector<std::string> listServices() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [name, _] : services_) {
            result.push_back(name);
        }
        return result;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        services_.clear();
    }

private:
    std::map<std::string, std::shared_ptr<void>> services_;
    mutable std::shared_mutex mutex_;
};

} // namespace rpf
