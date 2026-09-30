#pragma once
#include <functional>
#include <vector>
namespace SKSE {
    namespace log {
        template<class... Args> void error(const char*, Args&&...) {}
        template<class... Args> void info(const char*, Args&&...) {}
    }
    struct TaskInterface {
        std::vector<std::function<void()>> tasks;
        void AddTask(std::function<void()> task) { tasks.push_back(std::move(task)); }
        void run() { auto batch = std::move(tasks); tasks.clear(); for (const auto& task : batch) task(); }
    };
    inline TaskInterface taskInterface;
    inline auto GetTaskInterface() { return &taskInterface; }
}
