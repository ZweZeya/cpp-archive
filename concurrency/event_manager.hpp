#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace my_std {

class event_manager {
private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<std::function<void()>>>> observers_;

public:

    void on(std::string event_id, std::function<void()> callback) {
        observers_[std::move(event_id)].push_back(std::make_shared<std::function<void()>>(std::move(callback)));
    }

    void emit(const std::string& event_id) {
        auto itr = observers_.find(event_id);
        if (itr == observers_.end()) return;

        auto snapshot = itr->second;
        for (auto& fn : snapshot) {
            (*fn)();
        }
    }
};

}
