// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CSymbolLink.h"

#include <string>
#include <string_view>

namespace {

struct SymbolLinkRecorder
{
    terminal::CSymbolLink::Binding binding;
    std::string symbol{"OLD"};
    int applies{0};
    int* hits{nullptr};
    bool echo{false};
    // 1: an inbound SPX is stored as $SPX and published. 2: an inbound $SPX publishes QQQ.
    int rewrite{0};

    static void apply(void* self, std::string_view symbol)
    {
        auto* pane = static_cast<SymbolLinkRecorder*>(self);
        ++pane->applies;
        if (pane->hits != nullptr)
        {
            ++(*pane->hits);
        }
        pane->symbol.assign(symbol);
        if (pane->echo)
        {
            pane->binding.publish(symbol);
            return;
        }
        if (pane->rewrite == 1 && symbol == "SPX")
        {
            pane->symbol = "$SPX";
            pane->binding.publish("$SPX");
        }
        else if (pane->rewrite == 2 && symbol == "$SPX")
        {
            pane->binding.publish("QQQ");
        }
    }
};

void joinGroup(terminal::CSymbolLink& link, SymbolLinkRecorder& pane, std::string_view window_id,
               terminal::SymbolLinkGroup group)
{
    pane.binding.attach(link, window_id, &SymbolLinkRecorder::apply, &pane);
    pane.binding.setGroup(group);
}

}  // namespace

TEST_CASE("symbol link groups fan a commit out to the other members")
{
    using terminal::SymbolLinkGroup;

    terminal::CSymbolLink link;
    SymbolLinkRecorder chart;
    SymbolLinkRecorder financials;
    SymbolLinkRecorder options;
    joinGroup(link, chart, "pane:1", SymbolLinkGroup::One);
    joinGroup(link, financials, "financials:1", SymbolLinkGroup::One);
    joinGroup(link, options, "options:1", SymbolLinkGroup::One);

    SECTION("three members, one publish")
    {
        chart.binding.publish("MSFT");
        CHECK(financials.symbol == "MSFT");
        CHECK(options.symbol == "MSFT");
        CHECK(chart.symbol == "OLD");
        CHECK(chart.applies == 0);
        CHECK(financials.applies == 1);
        CHECK(options.applies == 1);
        CHECK(link.droppedHops() == 0);
    }

    SECTION("another group and an ungrouped pane stay put")
    {
        SymbolLinkRecorder other;
        SymbolLinkRecorder alone;
        joinGroup(link, other, "pane:2", SymbolLinkGroup::Two);
        alone.binding.attach(link, "financials:2", &SymbolLinkRecorder::apply, &alone);
        chart.binding.publish("MSFT");
        CHECK(other.symbol == "OLD");
        CHECK(other.applies == 0);
        CHECK(alone.symbol == "OLD");
        CHECK(alone.applies == 0);
        CHECK(financials.symbol == "MSFT");
    }

    SECTION("apply does not publish")
    {
        chart.binding.publish("MSFT");
        CHECK(chart.applies == 0);
        CHECK(financials.applies == 1);
        CHECK(options.applies == 1);
        CHECK(link.droppedHops() == 0);
    }

    SECTION("setting the current group is a no-op")
    {
        chart.binding.setGroup(SymbolLinkGroup::One);
        CHECK(chart.binding.group() == SymbolLinkGroup::One);
        CHECK(chart.applies == 0);
        CHECK(financials.applies == 0);
        CHECK(options.applies == 0);
        CHECK(chart.symbol == "OLD");
        CHECK(financials.symbol == "OLD");
    }

    SECTION("changing groups does not publish")
    {
        chart.binding.setGroup(SymbolLinkGroup::Two);
        CHECK(chart.applies == 0);
        CHECK(financials.applies == 0);
        CHECK(options.applies == 0);
        CHECK(chart.symbol == "OLD");
        CHECK(financials.symbol == "OLD");
        CHECK(options.symbol == "OLD");
        chart.binding.setGroup(SymbolLinkGroup::None);
        CHECK(chart.symbol == "OLD");
        CHECK(financials.symbol == "OLD");
        CHECK(chart.applies == 0);
    }

    SECTION("republishing the same text is an echo")
    {
        financials.echo = true;
        options.echo = true;
        chart.binding.publish("MSFT");
        CHECK(financials.applies == 1);
        CHECK(options.applies == 1);
        CHECK(chart.applies == 0);
        CHECK(financials.symbol == "MSFT");
        CHECK(options.symbol == "MSFT");
        CHECK(link.droppedHops() == 0);
    }

    SECTION("one rewritten spelling fans out once")
    {
        financials.rewrite = 1;
        chart.binding.publish("SPX");
        CHECK(chart.symbol == "$SPX");
        CHECK(financials.symbol == "$SPX");
        CHECK(options.symbol == "$SPX");
        CHECK(chart.applies == 1);
        CHECK(financials.applies == 1);
        CHECK(options.applies == 1);
        CHECK(link.droppedHops() == 0);
    }

    SECTION("a third spelling is dropped and a later commit still runs")
    {
        chart.rewrite = 2;
        financials.rewrite = 1;
        chart.binding.publish("SPX");
        CHECK(link.droppedHops() == 1);
        CHECK(chart.symbol == "$SPX");
        CHECK(financials.symbol == "$SPX");
        CHECK(options.symbol == "$SPX");
        CHECK(chart.symbol != "QQQ");
        CHECK(financials.symbol != "QQQ");
        CHECK(options.symbol != "QQQ");

        const int chart_applies = chart.applies;
        chart.symbol = "MSFT";
        chart.binding.publish("MSFT");
        CHECK(financials.symbol == "MSFT");
        CHECK(options.symbol == "MSFT");
        CHECK(financials.applies == chart_applies + 1);
        CHECK(link.droppedHops() == 1);
    }

    SECTION("closing one member leaves the others in the group")
    {
        int closed_hits = 0;
        {
            SymbolLinkRecorder closed;
            closed.hits = &closed_hits;
            closed.binding.attach(link, "options:9", &SymbolLinkRecorder::apply, &closed);
            closed.binding.setGroup(SymbolLinkGroup::One);
        }
        chart.binding.publish("MSFT");
        CHECK(closed_hits == 0);
        CHECK(financials.symbol == "MSFT");
        CHECK(options.symbol == "MSFT");
        CHECK(chart.symbol == "OLD");
        CHECK(chart.binding.group() == SymbolLinkGroup::One);
        CHECK(financials.binding.group() == SymbolLinkGroup::One);
        CHECK(options.binding.group() == SymbolLinkGroup::One);
    }

    SECTION("normalization keeps a leading dollar and a slash")
    {
        chart.binding.publish(" $spx ");
        CHECK(financials.symbol == "$SPX");
        CHECK(options.symbol == "$SPX");
        chart.binding.publish(" brk/b ");
        CHECK(financials.symbol == "BRK/B");
        CHECK(options.symbol == "BRK/B");
    }

    SECTION("an empty commit clears the other members")
    {
        chart.binding.publish(" ");
        CHECK(financials.symbol.empty());
        CHECK(options.symbol.empty());
        CHECK(financials.applies == 1);
    }

    SECTION("an ungrouped publish does nothing")
    {
        chart.binding.setGroup(SymbolLinkGroup::None);
        chart.binding.publish("MSFT");
        CHECK(chart.applies == 0);
        CHECK(financials.applies == 0);
        CHECK(options.applies == 0);
        CHECK(financials.symbol == "OLD");
    }
}
