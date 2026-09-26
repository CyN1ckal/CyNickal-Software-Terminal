// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "backtest/BacktestWorker.h"

#include "market_data/Store.h"

#include <exception>
#include <mutex>
#include <optional>
#include <utility>

namespace terminal {

BacktestWorker::BacktestWorker(std::filesystem::path db_path)
    : db_path_(std::move(db_path)), thread_([this] { run(); })
{
}

BacktestWorker::~BacktestWorker()
{
    {
        const std::scoped_lock lock(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

std::uint64_t BacktestWorker::enqueue(BacktestRequest request)
{
    std::uint64_t serial = 0;
    {
        const std::scoped_lock lock(mu_);
        serial = next_serial_++;
        jobs_.push_back(Queued{.serial = serial, .request = std::move(request)});
    }
    cv_.notify_one();
    return serial;
}

BacktestWorker::Snapshot BacktestWorker::snapshot() const
{
    const std::scoped_lock lock(mu_);
    Snapshot snap;
    snap.running = running_;
    snap.queued = static_cast<int>(jobs_.size());
    snap.finished_serial = finished_serial_;
    return snap;
}

bool BacktestWorker::idle() const
{
    const std::scoped_lock lock(mu_);
    return !running_ && jobs_.empty();
}

std::optional<BacktestOutcome> BacktestWorker::outcome(std::uint64_t serial) const
{
    const std::scoped_lock lock(mu_);
    if (serial == 0 || serial != outcome_serial_)
    {
        return std::nullopt;
    }
    return outcome_;
}

void BacktestWorker::run()
{
    for (;;)
    {
        Queued job;
        {
            std::unique_lock lock(mu_);
            cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
            if (stop_)
            {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop_front();
            running_ = true;
        }
        BacktestOutcome result;
        try
        {
            Store writer(db_path_, StoreMode::Writer);
            result = runAndRecordBacktest(writer, job.request);
        }
        catch (const std::exception& ex)
        {
            result.ok = false;
            result.error = ex.what();
        }
        {
            const std::scoped_lock lock(mu_);
            running_ = false;
            finished_serial_ = job.serial;
            outcome_serial_ = job.serial;
            outcome_ = std::move(result);
        }
    }
}

}  // namespace terminal
