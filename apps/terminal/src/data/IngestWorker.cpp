// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/IngestWorker.h"

#include "CurlClient.h"
#include "OpenFigiSession.h"

#include "market_data/MboumIngest.h"
#include "market_data/Secrets.h"
#include "market_data/Store.h"
#include "market_data/Time.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace terminal {
namespace {

constexpr std::size_t kIngestFailureHistory = 64;

[[nodiscard]] bool sameIngestJob(const IngestWorker::Job& a, const IngestWorker::Job& b)
{
    return a.symbol == b.symbol && a.from == b.from && a.to == b.to &&
           a.timeframe_s == b.timeframe_s && a.splits_only == b.splits_only &&
           a.statements == b.statements && a.statement == b.statement &&
           a.statement_timeframe == b.statement_timeframe && a.options == b.options &&
           a.option_expiration == b.option_expiration;
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
        const std::scoped_lock<std::mutex> lock(mu_);
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
        const std::scoped_lock<std::mutex> lock(mu_);
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
    const std::scoped_lock<std::mutex> lock(mu_);
    return snap_;
}

IngestWorker::SerialFailure IngestWorker::failureForSerial(std::uint64_t serial) const
{
    const std::scoped_lock<std::mutex> lock(mu_);
    const auto found = std::ranges::find_if(failures_, [serial](const FailedSerial& row) {
        return row.serial == serial;
    });
    SerialFailure out;
    if (found == failures_.end())
    {
        return out;
    }
    out.failed = true;
    out.message = found->message;
    return out;
}

void IngestWorker::clearDirty()
{
    const std::scoped_lock<std::mutex> lock(mu_);
    snap_.dirty = false;
}

void IngestWorker::run()
{
    try
    {
        Store store(db_path_, StoreMode::Writer);
        std::unique_ptr<CurlClient> http;
        std::unique_ptr<OpenFigiSession> figi;
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
                if (!figi)
                {
                    figi = std::make_unique<OpenFigiSession>(secrets_path_);
                }
                std::string notice = runJob(store, *http, figi->client(), job);
                const std::scoped_lock<std::mutex> lock(mu_);
                running_valid_ = false;
                snap_.finished_serial = job.serial;
                snap_.running = !jobs_.empty();
                snap_.queued = static_cast<int>(jobs_.size());
                snap_.dirty = true;
                snap_.error.clear();
                if (!notice.empty())
                {
                    snap_.message = std::move(notice);
                }
                else if (!snap_.running)
                {
                    snap_.message = "idle";
                }
            }
            catch (const std::exception& ex)
            {
                std::string message = ex.what();
                const std::scoped_lock<std::mutex> lock(mu_);
                running_valid_ = false;
                failures_.push_back(FailedSerial{.serial=job.serial, .message=message});
                if (failures_.size() > kIngestFailureHistory)
                {
                    failures_.pop_front();
                }
                snap_.finished_serial = job.serial;
                snap_.error_serial = job.serial;
                snap_.running = !jobs_.empty();
                snap_.queued = static_cast<int>(jobs_.size());
                snap_.dirty = true;
                snap_.error = std::move(message);
                snap_.message = "failed";
            }
        }
    }
    catch (const std::exception& ex)
    {
        const std::scoped_lock<std::mutex> lock(mu_);
        snap_.error = ex.what();
        snap_.message = "store open failed";
        snap_.running = false;
    }
}

std::string IngestWorker::runJob(Store& store, CurlClient& http, OpenFigiClient& figi, const Job& job)
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
        const std::scoped_lock<std::mutex> lock(mu_);
        snap_.current_date = day.session_date;
        ++snap_.days_done;
        snap_.dirty = true;
        snap_.message = job.symbol + " " + formatSessionDate(day.session_date) + " " +
                        std::string(toSql(day.status)) + " bars=" + std::to_string(day.bar_count);
    };
    if (job.options)
    {
        const std::string when =
            job.option_expiration == 0 ? std::string("chain") : formatSessionDate(job.option_expiration);
        {
            const std::scoped_lock<std::mutex> lock(mu_);
            snap_.message = job.symbol + " options " + when;
        }
        return ingestOptions(store, get, figi, job.symbol, job.option_expiration).identity_notice;
    }
    if (job.statements)
    {
        {
            const std::scoped_lock<std::mutex> lock(mu_);
            snap_.message = job.symbol + " " + std::string(toSql(job.statement)) + " " +
                            std::string(toSql(job.statement_timeframe));
        }
        return ingestStatement(store, get, figi, job.symbol, job.statement, job.statement_timeframe).identity_notice;
    }
    if (job.splits_only)
    {
        return ingestSplits(store, get, figi, job.symbol).identity_notice;
    }
    if (job.timeframe_s == kTimeframe1d)
    {
        return ingestDailySymbol(store, get, figi, job.symbol, job.from, job.to, on_day).identity_notice;
    }
    return ingestSymbol(store, get, figi, job.symbol, job.from, job.to, on_day).identity_notice;
}

}  // namespace terminal
