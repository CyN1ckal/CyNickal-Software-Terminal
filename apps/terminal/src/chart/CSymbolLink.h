// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

// One symbol group inside a chartbook. None is ungrouped. A pane is in at most one group.
enum class SymbolLinkGroup : std::uint8_t
{
    None = 0,
    One = 1,
    Two = 2,
    Three = 3,
    Four = 4,
};

inline constexpr int kSymbolLinkGroupCount = 4;

[[nodiscard]] constexpr bool isSymbolLinkGroup(int value) noexcept
{
    return value >= static_cast<int>(SymbolLinkGroup::One) &&
           value <= static_cast<int>(SymbolLinkGroup::Four);
}

[[nodiscard]] inline SymbolLinkGroup symbolLinkGroupFromInt(int value) noexcept
{
    if (isSymbolLinkGroup(value))
    {
        return static_cast<SymbolLinkGroup>(value);
    }
    return SymbolLinkGroup::None;
}

[[nodiscard]] inline const char* symbolLinkGroupLabel(SymbolLinkGroup group) noexcept
{
    switch (group)
    {
    case SymbolLinkGroup::One:
        return "1";
    case SymbolLinkGroup::Two:
        return "2";
    case SymbolLinkGroup::Three:
        return "3";
    case SymbolLinkGroup::Four:
        return "4";
    case SymbolLinkGroup::None:
        return "None";
    }
    return "None";
}

// Empty when the pane is ungrouped. Otherwise " #1" through " #4", for a window tab.
inline void writeSymbolLinkMark(char* out, std::size_t out_n, SymbolLinkGroup group) noexcept
{
    if (out_n == 0)
    {
        return;
    }
    if (group == SymbolLinkGroup::None)
    {
        out[0] = '\0';
        return;
    }
    std::snprintf(out, out_n, " #%s", symbolLinkGroupLabel(group));
}

// Fans a symbol commit out to the other members of one group. The chartbook owns one table.
// Panes opt in by embedding Binding. The table does not know pane types.
class CSymbolLink
{
public:
    static constexpr int kMaxHops = 2;

    using ApplySymbol = void (*)(void* context, std::string_view symbol);

    class Binding
    {
    public:
        Binding() = default;
        Binding(const Binding&) = delete;
        Binding& operator=(const Binding&) = delete;
        Binding(Binding&&) = delete;
        Binding& operator=(Binding&&) = delete;
        ~Binding();

        // Re-attach detaches first. window_id is the layout id the pane already builds.
        // apply is required. context is the pane and must outlive this binding.
        // The table does not call apply from detach or from its destructor.
        // A second live binding that attaches the same window id detaches the first.
        // A newly attached binding is SymbolLinkGroup::None.
        void attach(CSymbolLink& link, std::string_view window_id, ApplySymbol apply, void* context);
        void detach() noexcept;

        [[nodiscard]] bool attached() const noexcept;
        [[nodiscard]] std::string_view windowId() const noexcept;
        [[nodiscard]] SymbolLinkGroup group() const noexcept;

        // Same group: return, no publish. Otherwise store the group.
        // Does not change any symbol. None leaves the group.
        void setGroup(SymbolLinkGroup group) noexcept;

        // Normalizes. No-op when this binding is not attached or its group is None.
        // Fans out to every other attached member of the same group.
        void publish(std::string_view symbol);

    private:
        friend class CSymbolLink;
        CSymbolLink* link_{nullptr};
        std::string window_id_;
    };

    CSymbolLink() = default;
    CSymbolLink(const CSymbolLink&) = delete;
    CSymbolLink& operator=(const CSymbolLink&) = delete;
    CSymbolLink(CSymbolLink&&) = delete;
    CSymbolLink& operator=(CSymbolLink&&) = delete;
    ~CSymbolLink();

    [[nodiscard]] int droppedHops() const noexcept;

private:
    friend class Binding;
    struct Slot
    {
        Binding* binding{nullptr};
        std::string window_id;
        ApplySymbol apply{nullptr};
        void* context{nullptr};
        SymbolLinkGroup group{SymbolLinkGroup::None};
    };

    void publish(std::string_view from, std::string_view raw);
    [[nodiscard]] Slot* find(std::string_view window_id) noexcept;
    [[nodiscard]] const Slot* find(std::string_view window_id) const noexcept;

    std::vector<Slot> slots_;
    int hop_{0};
    int generation_{0};
    int dropped_hops_{0};
    bool inflight_active_{false};
    SymbolLinkGroup inflight_group_{SymbolLinkGroup::None};
    std::string inflight_symbol_;
};

}  // namespace terminal
