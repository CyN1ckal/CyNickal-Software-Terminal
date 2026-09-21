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

namespace myapp {

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

void IngestWorker::enqueue(Job job)
{
    {
        const std::lock_guard<std::mutex> lock(mu_);
        jobs_.push_back(std::move(job));
        snap_.queued = static_cast<int>(jobs_.size());
        if (snap_.message.empty() && !snap_.running)
        {
            snap_.message = "queued";
        }
    }
    cv_.notify_one();
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
                snap_.running = !jobs_.empty();
                snap_.queued = static_cast<int>(jobs_.size());
                snap_.dirty = true;
                if (!snap_.running)
                {
                    snap_.message = "idle";
                }
            }
            catch (const std::exception& ex)
            {
                const std::lock_guard<std::mutex> lock(mu_);
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
    (void)ingestSymbol(store, get, job.symbol, job.from, job.to, on_day);
}

}  // namespace myapp
