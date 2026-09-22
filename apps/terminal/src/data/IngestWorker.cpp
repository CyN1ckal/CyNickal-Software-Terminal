// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/IngestWorker.h"

#include "CurlClient.h"

#include "market_data/MboumIngest.h"
#include "market_data/Secrets.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] bool sameIngestJob(const IngestWorker::Job& a, const IngestWorker::Job& b)
{
    return a.symbol == b.symbol && a.from == b.from && a.to == b.to && a.timeframe_s == b.timeframe_s;
}

}  // namespace

IngestWorker::IngestWorker(std::filesystem::path db_path, std::filesystem::path secrets_path)
    : db_path_(std::move(db_path)), secrets_path_(std::move(secrets_path))
{
    thread_ = std::thread(&IngestWorker::run, this);
}

IngestWorker::~IngestWorker()
{
    {
        const std::lock_guard<std::mutex> lock(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
    {
        thread_.join();
    }
}

IngestWorker::EnqueueResult IngestWorker::enqueue(Job job)
{
    EnqueueResult out;
    {
        const std::lock_guard<std::mutex> lock(mu_);
        if (running_valid_ && sameIngestJob(running_job_, job))
        {
            out.serial = running_job_.serial;
            return out;
        }
        for (const Job& pending : jobs_)
        {
            if (sameIngestJob(pending, job))
            {
                out.serial = pending.serial;
                return out;
            }
        }
        job.serial = next_serial_;
        ++next_serial_;
        out.serial = job.serial;
        out.accepted = true;
        jobs_.push_back(std::move(job));
        snap_.queued = static_cast<int>(jobs_.size());
        if (!snap_.running)
        {
            snap_.message = "queued " + jobs_.back().symbol;
        }
    }
    cv_.notify_one();
    return out;
}

IngestWorker::Snapshot IngestWorker::snapshot() const
{
    const std::lock_guard<std::mutex> lock(mu_);
    return snap_;
}

void IngestWorker::clearDirty()
{
    const std::lock_guard<std::mutex> lock(mu_);
    snap_.dirty = false;
}

void IngestWorker::run()
{
    try
    {
        Store store(db_path_, StoreMode::Writer);
        std::unique_ptr<CurlClient> http;
        for (;;)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty())
                {
                    break;
                }
                if (jobs_.empty())
                {
                    continue;
                }
                job = std::move(jobs_.front());
                jobs_.pop_front();
                running_job_ = job;
                running_valid_ = true;
                snap_.running = true;
                snap_.queued = static_cast<int>(jobs_.size());
                snap_.symbol = job.symbol;
                snap_.current_date = job.from;
                snap_.days_done = 0;
                snap_.error.clear();
                snap_.message = "starting " + job.symbol;
            }
            try
            {
                if (!http)
                {
                    http = std::make_unique<CurlClient>(loadMboumApiKey(secrets_path_));
                }
                runJob(store, *http, job);
                const std::lock_guard<std::mutex> lock(mu_);
                running_valid_ = false;
                snap_.finished_serial = job.serial;
                snap_.running = !jobs_.empty();
                snap_.queued = static_cast<int>(jobs_.size());
                snap_.dirty = true;
                snap_.error.clear();
                if (!snap_.running)
                {
                    snap_.message = "idle";
                }
            }
            catch (const std::exception& ex)
            {
                const std::lock_guard<std::mutex> lock(mu_);
                running_valid_ = false;
                snap_.finished_serial = job.serial;
                snap_.error_serial = job.serial;
                snap_.running = !jobs_.empty();
                snap_.queued = static_cast<int>(jobs_.size());
                snap_.dirty = true;
                snap_.error = ex.what();
                snap_.message = "failed";
            }
        }
    }
    catch (const std::exception& ex)
    {
        const std::lock_guard<std::mutex> lock(mu_);
        snap_.error = ex.what();
        snap_.message = "store open failed";
        snap_.running = false;
    }
}

void IngestWorker::runJob(Store& store, CurlClient& http, const Job& job)
{
    bool first = true;
    auto get = [&](std::string_view url) {
        if (!first)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        }
        first = false;
        return http.getWithRetry(url);
    };
    auto on_day = [this, &job](const IngestDayResult& day) {
        const std::lock_guard<std::mutex> lock(mu_);
        snap_.current_date = day.session_date;
        ++snap_.days_done;
        snap_.dirty = true;
        snap_.message = job.symbol + " " + formatSessionDate(day.session_date) + " " +
                        std::string(toSql(day.status)) + " bars=" + std::to_string(day.bar_count);
    };
    if (job.timeframe_s == kTimeframe1d)
    {
        (void)ingestDailySymbol(store, get, job.symbol, job.from, job.to, on_day);
    }
    else
    {
        (void)ingestSymbol(store, get, job.symbol, job.from, job.to, on_day);
    }
}

}  // namespace terminal
