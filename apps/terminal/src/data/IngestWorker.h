#pragma once

#include "market_data/Types.h"

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace myapp {

class IngestWorker
{
public:
    struct Job
    {
        std::string symbol;
        SessionDate from{};
        SessionDate to{};
    };

    struct Snapshot
    {
        bool running = false;
        bool dirty = false;
        int queued = 0;
        int days_done = 0;
        std::string symbol;
        SessionDate current_date{};
        std::string message;
        std::string error;
    };

    IngestWorker(std::filesystem::path db_path, std::filesystem::path secrets_path);
    ~IngestWorker();

    IngestWorker(const IngestWorker&) = delete;
    IngestWorker& operator=(const IngestWorker&) = delete;
    IngestWorker(IngestWorker&&) = delete;
    IngestWorker& operator=(IngestWorker&&) = delete;

    void enqueue(Job job);
    [[nodiscard]] Snapshot snapshot() const;
    void clearDirty();

private:
    void run();
    void runJob(class Store& store, class CurlClient& http, const Job& job);

    std::filesystem::path db_path_;
    std::filesystem::path secrets_path_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    Snapshot snap_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace myapp
