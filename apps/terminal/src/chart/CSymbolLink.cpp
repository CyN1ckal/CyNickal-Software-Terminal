// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CSymbolLink.h"

#include "chart/CChartLoad.h"

#include <algorithm>
#include <utility>

namespace terminal {
namespace {

class HopGuard
{
public:
    explicit HopGuard(int& hop) noexcept : hop_(&hop)
    {
        ++(*hop_);
    }
    ~HopGuard()
    {
        --(*hop_);
    }
    HopGuard(const HopGuard&) = delete;
    HopGuard& operator=(const HopGuard&) = delete;
    HopGuard(HopGuard&&) = delete;
    HopGuard& operator=(HopGuard&&) = delete;

private:
    int* hop_;
};

// Saves the in-flight delivery so a nested rewrite can see it, including an empty ticker.
class InboundGuard
{
public:
    InboundGuard(bool& active, SymbolLinkGroup& group, std::string& symbol, SymbolLinkGroup next_group,
                 std::string next_symbol)
        : active_(active), group_(group), symbol_(symbol), saved_active_(active), saved_group_(group),
          saved_symbol_(symbol)
    {
        active_ = true;
        group_ = next_group;
        symbol_ = std::move(next_symbol);
    }
    ~InboundGuard()
    {
        active_ = saved_active_;
        group_ = saved_group_;
        symbol_ = std::move(saved_symbol_);
    }
    InboundGuard(const InboundGuard&) = delete;
    InboundGuard& operator=(const InboundGuard&) = delete;
    InboundGuard(InboundGuard&&) = delete;
    InboundGuard& operator=(InboundGuard&&) = delete;

private:
    bool& active_;
    SymbolLinkGroup& group_;
    std::string& symbol_;
    bool saved_active_;
    SymbolLinkGroup saved_group_;
    std::string saved_symbol_;
};

}  // namespace

CSymbolLink::Binding::~Binding()
{
    detach();
}

void CSymbolLink::Binding::attach(CSymbolLink& link, std::string_view window_id, ApplySymbol apply, void* context)
{
    detach();
    if (window_id.empty() || apply == nullptr)
    {
        return;
    }
    if (Slot* existing = link.find(window_id); existing != nullptr && existing->binding != nullptr)
    {
        existing->binding->detach();
    }
    Slot slot{
        .binding = this,
        .window_id = std::string(window_id),
        .apply = apply,
        .context = context,
        .group = SymbolLinkGroup::None,
    };
    link.slots_.push_back(std::move(slot));
    link_ = &link;
    window_id_ = link.slots_.back().window_id;
}

void CSymbolLink::Binding::detach() noexcept
{
    if (link_ == nullptr)
    {
        return;
    }
    CSymbolLink* const link = link_;
    link_ = nullptr;
    window_id_.clear();
    const auto found = std::ranges::find_if(link->slots_, [this](const Slot& slot) { return slot.binding == this; });
    if (found == link->slots_.end())
    {
        return;
    }
    if (found + 1 != link->slots_.end())
    {
        *found = std::move(link->slots_.back());
    }
    link->slots_.pop_back();
}

bool CSymbolLink::Binding::attached() const noexcept
{
    return link_ != nullptr;
}

std::string_view CSymbolLink::Binding::windowId() const noexcept
{
    return window_id_;
}

SymbolLinkGroup CSymbolLink::Binding::group() const noexcept
{
    if (link_ == nullptr)
    {
        return SymbolLinkGroup::None;
    }
    const Slot* slot = link_->find(window_id_);
    if (slot == nullptr)
    {
        return SymbolLinkGroup::None;
    }
    return slot->group;
}

void CSymbolLink::Binding::setGroup(SymbolLinkGroup group) noexcept
{
    if (link_ == nullptr)
    {
        return;
    }
    Slot* slot = link_->find(window_id_);
    if (slot == nullptr || slot->group == group)
    {
        return;
    }
    slot->group = group;
}

void CSymbolLink::Binding::publish(std::string_view symbol)
{
    if (link_ == nullptr)
    {
        return;
    }
    link_->publish(window_id_, symbol);
}

CSymbolLink::~CSymbolLink()
{
    for (Slot& slot : slots_)
    {
        if (slot.binding != nullptr)
        {
            slot.binding->link_ = nullptr;
            slot.binding->window_id_.clear();
            slot.binding = nullptr;
        }
    }
    slots_.clear();
}

int CSymbolLink::droppedHops() const noexcept
{
    return dropped_hops_;
}

void CSymbolLink::publish(std::string_view from, std::string_view raw)
{
    const std::string symbol = normalizeChartSymbol(raw);
    const Slot* slot = std::as_const(*this).find(from);
    if (slot == nullptr || slot->group == SymbolLinkGroup::None)
    {
        return;
    }
    if (inflight_active_ && inflight_group_ == slot->group && symbol == inflight_symbol_)
    {
        return;
    }
    if (hop_ >= kMaxHops)
    {
        ++dropped_hops_;
        return;
    }

    // One hop is the whole fan-out. Do not increment hop_ inside the member loop.
    const int generation = ++generation_;
    const SymbolLinkGroup group = slot->group;
    const HopGuard hops{hop_};
    const InboundGuard inbound{inflight_active_, inflight_group_, inflight_symbol_, group, symbol};

    std::vector<std::string> targets;
    for (const Slot& other : slots_)
    {
        if (other.window_id != from && other.group == group && other.apply != nullptr)
        {
            targets.push_back(other.window_id);
        }
    }
    for (const std::string& id : targets)
    {
        if (generation != generation_)
        {
            return;
        }
        const Slot* dest = std::as_const(*this).find(id);
        if (dest == nullptr || dest->group != group || dest->apply == nullptr)
        {
            continue;
        }
        dest->apply(dest->context, symbol);
    }
}

CSymbolLink::Slot* CSymbolLink::find(std::string_view window_id) noexcept
{
    const auto found = std::ranges::find_if(slots_, [window_id](const Slot& slot) {
        return slot.window_id == window_id;
    });
    if (found == slots_.end())
    {
        return nullptr;
    }
    return &*found;
}

const CSymbolLink::Slot* CSymbolLink::find(std::string_view window_id) const noexcept
{
    const auto found = std::ranges::find_if(slots_, [window_id](const Slot& slot) {
        return slot.window_id == window_id;
    });
    if (found == slots_.end())
    {
        return nullptr;
    }
    return &*found;
}

}  // namespace terminal
