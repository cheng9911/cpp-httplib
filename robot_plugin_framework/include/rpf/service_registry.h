#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <typeindex>

namespace rpf {

class ServiceRegistry {
public:
    // 注册服务
    template<typename T>
    bool registerService(const std::string& name, std::shared_ptr<T> service) {
        if (name.empty()) {
            return false;
        }
        std::unique_lock lock(mutex_);
        services_[name] = std::static_pointer_cast<void>(service);
        type_ids_.insert_or_assign(name, std::type_index(typeid(T)));
        return true;
    }

    // 获取服务（带类型安全检查）
    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = services_.find(name);
        if (it == services_.end()) {
            return nullptr;
        }
        // 类型检查：确保注册时的类型与请求的类型一致
        auto type_it = type_ids_.find(name);
        if (type_it != type_ids_.end() && type_it->second != std::type_index(typeid(T))) {
            return nullptr;  // 类型不匹配
        }
        return std::static_pointer_cast<T>(it->second);
    }

    // 注销服务
    bool unregisterService(const std::string& name) {
        std::unique_lock lock(mutex_);
        type_ids_.erase(name);
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
        type_ids_.clear();
    }

private:
    std::map<std::string, std::shared_ptr<void>> services_;
    std::map<std::string, std::type_index> type_ids_;
    mutable std::shared_mutex mutex_;
};

} // namespace rpf
