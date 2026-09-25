// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/OptionChainSource.h"

#include "chart/CChartLoad.h"
#include "data/IngestWorker.h"
#include "ui/Theme.h"

#include "market_data/Store.h"
#include "market_data/Time.h"

#include "imgui.h"

#include <cstdio>
#include <ctime>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace terminal {
namespace {

[[nodiscard]] bool isBusyError(std::string_view what) noexcept
{
    return what.find("busy") != std::string_view::npos || what.find("locked") != std::string_view::npos;
}

[[nodiscard]] std::string formatFetched(UnixSeconds ts)
{
    std::tm parts{};
    if (!tryUtcTm(static_cast<std::time_t>(ts), parts))
    {
        return {};
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d UTC", parts.tm_year + 1900, parts.tm_mon + 1,
                  parts.tm_mday, parts.tm_hour, parts.tm_min);
    return buf;
}

[[nodiscard]] std::string expiryLabel(const OptionExpiry& expiry)
{
    return formatSessionDate(expiry.expiration) + " " + std::string(toSql(expiry.expiration_type));
}

}  // namespace

void OptionChainSource::restore(std::string_view symbol, std::string_view figi, SessionDate expiration,
                                std::string_view expiration_type)
{
    active_symbol_ = normalizeChartSymbol(symbol);
    active_figi_ = std::string(figi);
    std::snprintf(symbol_input_, sizeof(symbol_input_), "%s", active_symbol_.c_str());
    expiration_ = 0;
    has_expiration_ = false;
    expiration_type_ = OptionExpirationType::Weekly;
    if (expiration != 0 && (expiration_type == "weekly" || expiration_type == "monthly"))
    {
        expiration_ = expiration;
        expiration_type_ = optionExpirationTypeFromSql(expiration_type);
        has_expiration_ = true;
    }
    expiries_.clear();
    quotes_.clear();
    underlying_.reset();
    loaded_key_.clear();
    failed_key_.clear();
    inflight_key_.clear();
    inflight_serial_ = 0;
    inflight_ = false;
    have_slice_ = false;
    busy_ = false;
    blocked_ = false;
    fetch_now_ = false;
    needs_reload_ = true;
    error_.clear();
    status_ = active_symbol_.empty() ? "enter a symbol" : "not fetched";
}

const std::string& OptionChainSource::symbol() const noexcept
{
    return active_symbol_;
}

const std::string& OptionChainSource::figi() const noexcept
{
    return active_figi_;
}

bool OptionChainSource::hasExpiration() const noexcept
{
    return has_expiration_;
}

SessionDate OptionChainSource::expiration() const noexcept
{
    return expiration_;
}

OptionExpirationType OptionChainSource::expirationType() const noexcept
{
    return expiration_type_;
}

std::span<const OptionQuote> OptionChainSource::quotes() const noexcept
{
    return quotes_;
}

const std::optional<OptionUnderlying>& OptionChainSource::underlying() const noexcept
{
    return underlying_;
}

const std::string& OptionChainSource::status() const noexcept
{
    return status_;
}

bool OptionChainSource::failed() const noexcept
{
    return !error_.empty();
}

bool OptionChainSource::fetching() const
{
    return inflight_ && inflight_key_ == viewKey();
}

std::string OptionChainSource::expirationLabel() const
{
    if (!has_expiration_)
    {
        return {};
    }
    return formatSessionDate(expiration_) + " " + std::string(toSql(expiration_type_));
}

std::string OptionChainSource::viewKey() const
{
    std::string key = active_symbol_;
    key.push_back('|');
    if (!has_expiration_)
    {
        key += "none";
        return key;
    }
    key += formatSessionDate(expiration_);
    key.push_back('|');
    key += toSql(expiration_type_);
    return key;
}

const OptionExpiry* OptionChainSource::selectedExpiry() const
{
    if (!has_expiration_)
    {
        return nullptr;
    }
    for (const OptionExpiry& expiry : expiries_)
    {
        if (expiry.expiration == expiration_ && expiry.expiration_type == expiration_type_)
        {
            return &expiry;
        }
    }
    return nullptr;
}

void OptionChainSource::requestFetch(IngestWorker* ingest, bool force)
{
    if (ingest == nullptr || active_symbol_.empty() || blocked_)
    {
        return;
    }
    const std::string key = viewKey();
    if (!force && (busy_ || have_slice_ || key == failed_key_ || (inflight_ && inflight_key_ == key)))
    {
        return;
    }
    IngestWorker::Job job;
    job.symbol = active_symbol_;
    job.options = true;
    job.option_expiration = has_expiration_ ? expiration_ : 0;
    const IngestWorker::EnqueueResult result = ingest->enqueue(std::move(job));
    inflight_ = true;
    inflight_serial_ = result.serial;
    inflight_key_ = key;
    if (force)
    {
        failed_key_.clear();
        error_.clear();
    }
    status_ = std::string("fetching ") + active_symbol_;
    if (has_expiration_)
    {
        status_ += ' ';
        status_ += formatSessionDate(expiration_);
    }
}

void OptionChainSource::refresh(Store* store, IngestWorker* ingest)
{
    bool just_finished = false;
    if (ingest != nullptr && inflight_)
    {
        const IngestWorker::Snapshot snap = ingest->snapshot();
        if (snap.finished_serial >= inflight_serial_ && inflight_serial_ != 0)
        {
            const IngestWorker::SerialFailure failure = ingest->failureForSerial(inflight_serial_);
            inflight_ = false;
            if (failure.failed)
            {
                failed_key_ = inflight_key_;
                error_ = failure.message;
                status_ = failure.message;
            }
            else if (inflight_key_ == viewKey())
            {
                error_.clear();
                needs_reload_ = true;
                just_finished = true;
            }
        }
    }

    if (!needs_reload_ && viewKey() == loaded_key_)
    {
        if (fetch_now_)
        {
            requestFetch(ingest, true);
            fetch_now_ = false;
        }
        return;
    }

    needs_reload_ = false;
    busy_ = false;
    blocked_ = false;
    if (store == nullptr)
    {
        quotes_.clear();
        expiries_.clear();
        underlying_.reset();
        have_slice_ = false;
        loaded_key_ = viewKey();
        fetch_now_ = false;
        status_ = "market data is unavailable";
        return;
    }
    if (active_symbol_.empty())
    {
        quotes_.clear();
        expiries_.clear();
        underlying_.reset();
        have_slice_ = false;
        loaded_key_ = viewKey();
        error_.clear();
        status_ = "enter a symbol";
        fetch_now_ = false;
        return;
    }

    try
    {
        std::optional<Instrument> found = resolveChartInstrument(*store, active_figi_, active_symbol_);
        if (!found.has_value() && active_figi_.empty() && active_symbol_.front() != '$')
        {
            // The chain endpoint answers SPX with the $SPX index.
            found = store->resolveSymbol("$" + active_symbol_);
        }
        if (found.has_value() && found->figi.has_value())
        {
            active_figi_ = *found->figi;
        }
        if (found.has_value() && found->listing_open && found->symbol != active_symbol_)
        {
            // Renamed since the book was saved, or SPX found as $SPX: show the stored ticker.
            active_symbol_ = found->symbol;
            std::snprintf(symbol_input_, sizeof(symbol_input_), "%s", active_symbol_.c_str());
        }
        if (!found.has_value())
        {
            quotes_.clear();
            expiries_.clear();
            underlying_.reset();
            have_slice_ = false;
            loaded_key_ = viewKey();
        }
        else
        {
            const InstrumentId id = found->id;
            expiries_ = store->queryOptionExpiries(id);
            underlying_ = store->findOptionUnderlying(id);
            if (!has_expiration_)
            {
                const OptionExpiry* best = nullptr;
                UnixSeconds best_fetched = 0;
                OptionExpirationType best_type = OptionExpirationType::Weekly;
                bool have_best = false;
                for (const OptionExpiry& expiry : expiries_)
                {
                    if (!expiry.fetched_at.has_value())
                    {
                        continue;
                    }
                    const UnixSeconds fetched = *expiry.fetched_at;
                    const bool newer = !have_best || fetched > best_fetched;
                    const bool monthly_tie = have_best && fetched == best_fetched &&
                                             expiry.expiration_type == OptionExpirationType::Monthly &&
                                             best_type != OptionExpirationType::Monthly;
                    if (newer || monthly_tie)
                    {
                        best = &expiry;
                        best_fetched = fetched;
                        best_type = expiry.expiration_type;
                        have_best = true;
                    }
                }
                if (best != nullptr)
                {
                    expiration_ = best->expiration;
                    expiration_type_ = best->expiration_type;
                    has_expiration_ = true;
                }
            }
            const OptionExpiry* selected = selectedExpiry();
            have_slice_ = selected != nullptr && selected->fetched_at.has_value();
            quotes_.clear();
            if (have_slice_)
            {
                quotes_ = store->queryOptionQuotes(id, expiration_, expiration_type_);
            }
            loaded_key_ = viewKey();
            error_.clear();
            if (!have_slice_)
            {
                status_ = "not fetched";
            }
            else
            {
                status_ = active_symbol_ + "  " + formatSessionDate(expiration_) + " " +
                          std::string(toSql(expiration_type_)) + "  " + std::to_string(quotes_.size()) +
                          " contracts";
                if (selected->fetched_at.has_value())
                {
                    status_ += "  ";
                    status_ += formatFetched(*selected->fetched_at);
                }
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (isBusyError(ex.what()))
        {
            busy_ = true;
            if (loaded_key_ != viewKey())
            {
                quotes_.clear();
                have_slice_ = false;
            }
            needs_reload_ = true;
            status_ = "database busy";
            return;
        }
        quotes_.clear();
        expiries_.clear();
        underlying_.reset();
        have_slice_ = false;
        loaded_key_ = viewKey();
        error_ = ex.what();
        status_ = error_;
        fetch_now_ = false;
        return;
    }

    if (just_finished && !have_slice_)
    {
        failed_key_ = viewKey();
        status_ = expiries_.empty() ? "no option chain" : "no contracts for that expiration";
    }
    if (fetch_now_)
    {
        requestFetch(ingest, true);
        fetch_now_ = false;
    }
    else if (!have_slice_ && !just_finished && !blocked_ && !busy_ && error_.empty())
    {
        requestFetch(ingest, false);
    }
}

void OptionChainSource::applyLinkedSymbol(std::string_view symbol)
{
    const std::string normalized = normalizeChartSymbol(symbol);
    if (normalized == active_symbol_)
    {
        return;
    }
    active_figi_.clear();
    has_expiration_ = false;
    expiration_ = 0;
    expiries_.clear();
    quotes_.clear();
    underlying_.reset();
    have_slice_ = false;
    active_symbol_ = normalized;
    std::snprintf(symbol_input_, sizeof(symbol_input_), "%s", active_symbol_.c_str());
    failed_key_.clear();
    error_.clear();
    fetch_now_ = true;
    needs_reload_ = true;
}

bool OptionChainSource::drawPicker(IngestWorker* ingest)
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::kField);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Theme::kBg3);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Theme::kBg3);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("SYMBOL");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(96.0f);
    const bool symbol_go =
        ImGui::InputText("##opt_symbol", symbol_input_, sizeof(symbol_input_),
                         ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::TextUnformatted("EXPIRATION");
    ImGui::SameLine();
    const OptionExpiry* selected = selectedExpiry();
    const std::string current_label = selected != nullptr ? expiryLabel(*selected) : expirationLabel();
    const char* current = current_label.empty() ? "select" : current_label.c_str();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("##opt_expiration", current))
    {
        for (const OptionExpiry& expiry : expiries_)
        {
            const std::string label = expiryLabel(expiry);
            const bool chosen = has_expiration_ && expiry.expiration == expiration_ &&
                                expiry.expiration_type == expiration_type_;
            if (ImGui::Selectable(label.c_str(), chosen) && !chosen)
            {
                expiration_ = expiry.expiration;
                expiration_type_ = expiry.expiration_type;
                has_expiration_ = true;
                error_.clear();
                failed_key_.clear();
                needs_reload_ = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    ImGui::BeginDisabled(ingest == nullptr);
    const bool clicked = ImGui::Button("GO");
    ImGui::EndDisabled();
    ImGui::PopStyleColor(4);

    if (!symbol_go && !clicked)
    {
        return false;
    }
    const std::string next = normalizeChartSymbol(symbol_input_);
    const bool switched = next != active_symbol_;
    if (switched)
    {
        active_figi_.clear();
        has_expiration_ = false;
        expiration_ = 0;
        expiries_.clear();
        quotes_.clear();
        underlying_.reset();
        have_slice_ = false;
    }
    active_symbol_ = next;
    std::snprintf(symbol_input_, sizeof(symbol_input_), "%s", active_symbol_.c_str());
    failed_key_.clear();
    error_.clear();
    fetch_now_ = true;
    needs_reload_ = true;
    return switched;
}

}  // namespace terminal
