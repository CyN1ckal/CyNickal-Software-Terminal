// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "catch_amalgamated.hpp"
#include "chart/CChartbookFile.h"
#include "options/OptionPayoff.h"

#include <cstddef>
#include <string>
#include <vector>

namespace {

terminal::ChartbookPayoff samplePayoffPanel()
{
    terminal::ChartbookPayoff panel;
    panel.id = 1;
    panel.symbol = "SPY";
    panel.figi = "BBG000BDTBL9";
    panel.expiration = 20261016;
    panel.expiration_type = "weekly";

    terminal::PayoffLeg call;
    call.instrument = terminal::LegInstrument::Call;
    call.strike = 100.0;
    call.expiration = 20261016;
    call.quantity = 1.0;
    call.price = 5.0;

    terminal::PayoffLeg put = call;
    put.instrument = terminal::LegInstrument::Put;
    put.strike = 95.0;
    put.quantity = -2.0;
    put.price = 1.25;

    terminal::PayoffLeg shares;
    shares.instrument = terminal::LegInstrument::Underlying;
    shares.quantity = 100.0;
    shares.price = 99.5;
    shares.multiplier = 1.0;

    panel.legs = {call, put, shares};
    return panel;
}

terminal::CChartbookDocument payoffChartbook(const terminal::ChartbookPayoff& panel)
{
    terminal::CChartbookDocument document = terminal::makeDefaultChartbook("chartbook1");
    terminal::chartbookInsertPayoff(document.layout, panel.id);
    document.payoffs.push_back(panel);
    document.focused_payoff = panel.id;
    document.next_payoff_id = panel.id + 1;
    return document;
}

void replaceFirst(std::string& text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    text.replace(at, from.size(), to);
}

}  // namespace

TEST_CASE("payoff windows parse their window id")
{
    int id = 0;
    CHECK(terminal::payoffIdFromWindow("payoff:7", id));
    CHECK(id == 7);
    CHECK(terminal::payoffWindowId(7) == "payoff:7");
    CHECK_FALSE(terminal::payoffIdFromWindow("payoff:0", id));
    CHECK_FALSE(terminal::payoffIdFromWindow("payoff:x", id));
    CHECK_FALSE(terminal::payoffIdFromWindow("options:7", id));
}

TEST_CASE("payoff windows round trip their chain, spot, and legs")
{
    terminal::ChartbookPayoff panel = samplePayoffPanel();
    panel.spot = 101.5;
    const terminal::CChartbookDocument document = payoffChartbook(panel);
    CHECK(terminal::chartbookPayoffIsOpen(document, 1));

    const terminal::ChartbookLoadResult loaded = terminal::chartbookFromJson(terminal::chartbookToJson(document));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.payoffs.size() == 1);
    const terminal::ChartbookPayoff& back = loaded.document.payoffs[0];
    CHECK(back.id == 1);
    CHECK(back.symbol == "SPY");
    CHECK(back.figi == "BBG000BDTBL9");
    CHECK(back.expiration == 20261016);
    CHECK(back.expiration_type == "weekly");
    CHECK(back.spot == 101.5);
    CHECK(loaded.document.focused_payoff == 1);
    CHECK(loaded.document.next_payoff_id == 2);
    CHECK(terminal::chartbookPayoffIsOpen(loaded.document, 1));

    REQUIRE(back.legs.size() == panel.legs.size());
    for (std::size_t index = 0; index < panel.legs.size(); ++index)
    {
        INFO(index);
        const terminal::PayoffLeg& want = panel.legs[index];
        const terminal::PayoffLeg& got = back.legs[index];
        CHECK(got.instrument == want.instrument);
        CHECK(got.strike == want.strike);
        CHECK(got.expiration == want.expiration);
        CHECK(got.quantity == want.quantity);
        CHECK(got.price == want.price);
        CHECK(got.multiplier == want.multiplier);
    }
}

TEST_CASE("an empty payoff window and a book without payoff windows both open")
{
    terminal::ChartbookPayoff empty;
    empty.id = 3;
    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(payoffChartbook(empty)));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.document.payoffs.size() == 1);
    CHECK(loaded.document.payoffs[0].legs.empty());
    CHECK(loaded.document.payoffs[0].spot == 0.0);

    const char* legacy = R"({
        "format": 1,
        "name": "old",
        "focused_pane": 0,
        "next_pane_id": 1,
        "data": {},
        "layout": {"windows": ["data"], "selected": "data"},
        "panes": []
    })";
    const terminal::ChartbookLoadResult old = terminal::chartbookFromJson(legacy);
    REQUIRE(old.ok);
    CHECK(old.document.payoffs.empty());
    CHECK(old.document.next_payoff_id == 1);
    CHECK(old.document.focused_payoff == 0);
}

TEST_CASE("a saved payoff with legs that cannot share an expiration is rejected")
{
    terminal::ChartbookPayoff panel = samplePayoffPanel();
    panel.legs[1].expiration = 20261023;
    const terminal::ChartbookLoadResult loaded =
        terminal::chartbookFromJson(terminal::chartbookToJson(payoffChartbook(panel)));
    CHECK_FALSE(loaded.ok);
    CHECK(loaded.error.find("payoff legs are invalid") != std::string::npos);
}

TEST_CASE("a saved payoff rejects an unknown leg kind, a zero quantity, and a missing window")
{
    const std::string good = terminal::chartbookToJson(payoffChartbook(samplePayoffPanel()));
    REQUIRE(terminal::chartbookFromJson(good).ok);

    std::string unknown = good;
    replaceFirst(unknown, "\"shares\"", "\"bond\"");
    CHECK_FALSE(terminal::chartbookFromJson(unknown).ok);

    terminal::ChartbookPayoff zero = samplePayoffPanel();
    zero.legs[0].quantity = 0.0;
    CHECK_FALSE(terminal::chartbookFromJson(terminal::chartbookToJson(payoffChartbook(zero))).ok);

    std::string dangling = good;
    replaceFirst(dangling, "\"payoff:1\"", "\"payoff:9\"");
    CHECK_FALSE(terminal::chartbookFromJson(dangling).ok);
}
