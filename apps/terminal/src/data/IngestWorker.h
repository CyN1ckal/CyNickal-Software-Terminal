// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "market_data/Types.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace terminal {

class IngestWorker
{
public:
    struct Job
    {
        std::string symbol;
        SessionDate from{};
        SessionDate to{};
        int timeframe_s{kTimeframe1m};
        std::uint64_t serial{0};
    };

    // `accepted` is false when an identical job is already queued or running.
    // `serial` is that job either way, and Snapshot::finished_serial passes it when the job ends.
    struct EnqueueResult
    {
        bool accepted{false};
        std::uint64_t serial{0};
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
        std::uint64_t finished_serial{0};
        std::uint64_t error_serial{0};
    };

    IngestWorker(std::filesystem::path db_path, std::filesystem::path secrets_path);
    ~IngestWorker();

    IngestWorker(const IngestWorker&) = delete;
    IngestWorker& operator=(const IngestWorker&) = delete;
    IngestWorker(IngestWorker&&) = delete;
    IngestWorker& operator=(IngestWorker&&) = delete;

    EnqueueResult enqueue(Job job);
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
    Job running_job_{};
    bool running_valid_{false};
    std::uint64_t next_serial_{1};
    Snapshot snap_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace terminal
