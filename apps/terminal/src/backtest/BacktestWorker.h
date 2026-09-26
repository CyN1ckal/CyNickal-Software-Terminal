// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "backtest/BacktestJob.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

namespace terminal {

// Runs backtests off the GUI thread, one at a time, on its own writer connection.
// The GUI keeps its reader; the WAL lets it see a run as soon as the writer commits.
class BacktestWorker
{
public:
    struct Snapshot
    {
        bool running{false};
        int queued{0};
        std::uint64_t finished_serial{0};
    };

    explicit BacktestWorker(std::filesystem::path db_path);
    ~BacktestWorker();

    BacktestWorker(const BacktestWorker&) = delete;
    BacktestWorker& operator=(const BacktestWorker&) = delete;
    BacktestWorker(BacktestWorker&&) = delete;
    BacktestWorker& operator=(BacktestWorker&&) = delete;

    // The serial Snapshot::finished_serial reaches when this request is done.
    std::uint64_t enqueue(BacktestRequest request);
    [[nodiscard]] Snapshot snapshot() const;
    // Nothing running and nothing queued: destroying the worker now does not wait.
    [[nodiscard]] bool idle() const;
    // The outcome of `serial` once it has finished. Only the newest outcome is kept.
    [[nodiscard]] std::optional<BacktestOutcome> outcome(std::uint64_t serial) const;

private:
    void run();

    struct Queued
    {
        std::uint64_t serial{0};
        BacktestRequest request;
    };

    std::filesystem::path db_path_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Queued> jobs_;
    bool running_{false};
    bool stop_{false};
    std::uint64_t next_serial_{1};
    std::uint64_t finished_serial_{0};
    std::uint64_t outcome_serial_{0};
    BacktestOutcome outcome_;
    std::thread thread_;
};

}  // namespace terminal
