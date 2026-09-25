// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "options/OptionStrategy.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace terminal {
namespace {

constexpr double kStrikeMatch = 1e-4;

using enum LegInstrument;

constexpr StrikeSlot slot(const char* label, int offset) noexcept
{
    return StrikeSlot{.label = label, .offset = offset};
}

constexpr TemplateLeg option(LegInstrument instrument, int strike_slot, double ratio) noexcept
{
    return TemplateLeg{.instrument = instrument, .slot = strike_slot, .ratio = ratio};
}

constexpr TemplateLeg shares(double ratio) noexcept
{
    return TemplateLeg{.instrument = Underlying, .slot = -1, .ratio = ratio};
}

constexpr StrategyTemplate recipe(const char* id,
                                  const char* name,
                                  const char* note,
                                  std::span<const StrikeSlot> slots,
                                  std::span<const TemplateLeg> legs) noexcept
{
    return StrategyTemplate{.id = id, .name = name, .note = note, .slots = slots, .legs = legs};
}

constexpr StrikeSlot kOneStrike[] = {slot("Strike", 0)};
constexpr StrikeSlot kCoveredSlot[] = {slot("Call", 1)};
constexpr StrikeSlot kProtectiveSlot[] = {slot("Put", -1)};
constexpr StrikeSlot kCollarSlots[] = {slot("Put", -1), slot("Call", 1)};
constexpr StrikeSlot kCallDebitSlots[] = {slot("Long", 0), slot("Short", 2)};
constexpr StrikeSlot kPutDebitSlots[] = {slot("Short", -2), slot("Long", 0)};
constexpr StrikeSlot kPutCreditSlots[] = {slot("Long", -2), slot("Short", 0)};
constexpr StrikeSlot kCallCreditSlots[] = {slot("Short", 0), slot("Long", 2)};
constexpr StrikeSlot kStrangleSlots[] = {slot("Put", -2), slot("Call", 2)};
constexpr StrikeSlot kButterflySlots[] = {slot("Lower", -2), slot("Middle", 0), slot("Upper", 2)};
constexpr StrikeSlot kIronButterflySlots[] = {slot("Long put", -2), slot("Body", 0), slot("Long call", 2)};
constexpr StrikeSlot kCondorSlots[] = {
    slot("Long put", -3),
    slot("Short put", -1),
    slot("Short call", 1),
    slot("Long call", 3),
};

constexpr TemplateLeg kLongShares[] = {shares(1.0)};
constexpr TemplateLeg kShortShares[] = {shares(-1.0)};
constexpr TemplateLeg kLongCall[] = {option(Call, 0, 1.0)};
constexpr TemplateLeg kLongPut[] = {option(Put, 0, 1.0)};
constexpr TemplateLeg kShortCall[] = {option(Call, 0, -1.0)};
constexpr TemplateLeg kShortPut[] = {option(Put, 0, -1.0)};
constexpr TemplateLeg kCoveredCall[] = {shares(1.0), option(Call, 0, -1.0)};
constexpr TemplateLeg kProtectivePut[] = {shares(1.0), option(Put, 0, 1.0)};
constexpr TemplateLeg kCollar[] = {shares(1.0), option(Put, 0, 1.0), option(Call, 1, -1.0)};
constexpr TemplateLeg kBullCall[] = {option(Call, 0, 1.0), option(Call, 1, -1.0)};
constexpr TemplateLeg kBearPut[] = {option(Put, 0, -1.0), option(Put, 1, 1.0)};
constexpr TemplateLeg kBullPut[] = {option(Put, 0, 1.0), option(Put, 1, -1.0)};
constexpr TemplateLeg kBearCall[] = {option(Call, 0, -1.0), option(Call, 1, 1.0)};
constexpr TemplateLeg kLongStraddle[] = {option(Call, 0, 1.0), option(Put, 0, 1.0)};
constexpr TemplateLeg kShortStraddle[] = {option(Call, 0, -1.0), option(Put, 0, -1.0)};
constexpr TemplateLeg kLongStrangle[] = {option(Put, 0, 1.0), option(Call, 1, 1.0)};
constexpr TemplateLeg kShortStrangle[] = {option(Put, 0, -1.0), option(Call, 1, -1.0)};
constexpr TemplateLeg kCallButterfly[] = {option(Call, 0, 1.0), option(Call, 1, -2.0), option(Call, 2, 1.0)};
constexpr TemplateLeg kIronButterfly[] = {
    option(Put, 0, 1.0),
    option(Put, 1, -1.0),
    option(Call, 1, -1.0),
    option(Call, 2, 1.0),
};
constexpr TemplateLeg kIronCondor[] = {
    option(Put, 0, 1.0),
    option(Put, 1, -1.0),
    option(Call, 2, -1.0),
    option(Call, 3, 1.0),
};

constexpr StrategyTemplate kTemplates[] = {
    recipe("long_call", "Long call", "Buy a call.", kOneStrike, kLongCall),
    recipe("long_put", "Long put", "Buy a put.", kOneStrike, kLongPut),
    recipe("short_call", "Short call", "Sell a call. Loss is unbounded as the underlying rises.",
           kOneStrike, kShortCall),
    recipe("short_put", "Short put", "Sell a put.", kOneStrike, kShortPut),
    recipe("long_shares", "Long shares", "Buy 100 shares per unit at the spot price.", {}, kLongShares),
    recipe("short_shares", "Short shares", "Sell 100 shares per unit short at the spot price.", {}, kShortShares),
    recipe("covered_call", "Covered call", "Own 100 shares per contract and sell a call against them.",
           kCoveredSlot, kCoveredCall),
    recipe("protective_put", "Protective put", "Own 100 shares per contract and buy a put under them.",
           kProtectiveSlot, kProtectivePut),
    recipe("collar", "Collar", "Own the shares, buy a put below, and sell a call above.", kCollarSlots, kCollar),
    recipe("bull_call_spread", "Bull call spread", "Buy the lower call and sell the upper call. Net debit.",
           kCallDebitSlots, kBullCall),
    recipe("bear_put_spread", "Bear put spread", "Buy the upper put and sell the lower put. Net debit.",
           kPutDebitSlots, kBearPut),
    recipe("bull_put_spread", "Bull put spread", "Sell the upper put and buy the lower put. Net credit.",
           kPutCreditSlots, kBullPut),
    recipe("bear_call_spread", "Bear call spread", "Sell the lower call and buy the upper call. Net credit.",
           kCallCreditSlots, kBearCall),
    recipe("long_straddle", "Long straddle", "Buy a call and a put at one strike.", kOneStrike, kLongStraddle),
    recipe("short_straddle", "Short straddle", "Sell a call and a put at one strike.", kOneStrike, kShortStraddle),
    recipe("long_strangle", "Long strangle", "Buy a put below and a call above.", kStrangleSlots, kLongStrangle),
    recipe("short_strangle", "Short strangle", "Sell a put below and a call above.", kStrangleSlots, kShortStrangle),
    recipe("call_butterfly", "Call butterfly", "Buy one lower and one upper call, sell two middle calls.",
           kButterflySlots, kCallButterfly),
    recipe("iron_butterfly", "Iron butterfly", "Sell a straddle at the body and buy the wings.",
           kIronButterflySlots, kIronButterfly),
    recipe("iron_condor", "Iron condor", "Sell a put spread and a call spread around the money.",
           kCondorSlots, kIronCondor),
};

[[nodiscard]] double midPrice(const OptionQuote& quote) noexcept
{
    if (quote.mid > 0.0)
    {
        return quote.mid;
    }
    if (quote.bid > 0.0 && quote.ask > 0.0)
    {
        return (quote.bid + quote.ask) * 0.5;
    }
    return std::max(quote.last, 0.0);
}

[[nodiscard]] OptionRight rightOf(LegInstrument instrument) noexcept
{
    return instrument == Put ? OptionRight::Put : OptionRight::Call;
}

[[nodiscard]] std::string formatStrike(double strike)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", strike);
    return buf;
}

}  // namespace

double quotePrice(const OptionQuote& quote, PriceBasis basis, bool buy) noexcept
{
    switch (basis)
    {
    case PriceBasis::Mid:
        return midPrice(quote);
    case PriceBasis::Natural:
        if (buy)
        {
            return quote.ask > 0.0 ? quote.ask : midPrice(quote);
        }
        return std::max(quote.bid, 0.0);
    case PriceBasis::Last:
        return quote.last > 0.0 ? quote.last : midPrice(quote);
    }
    return midPrice(quote);
}

PayoffLeg legFromQuote(const OptionQuote& quote, double quantity, double price) noexcept
{
    PayoffLeg leg;
    leg.instrument = quote.right == OptionRight::Put ? Put : Call;
    leg.strike = quote.strike;
    leg.expiration = quote.expiration;
    leg.quantity = quantity;
    leg.price = price;
    leg.multiplier = kContractMultiplier;
    return leg;
}

PayoffLeg underlyingLeg(double shares, double price) noexcept
{
    PayoffLeg leg;
    leg.instrument = Underlying;
    leg.quantity = shares;
    leg.price = price;
    leg.multiplier = 1.0;
    return leg;
}

std::vector<double> listedStrikes(std::span<const OptionQuote> chain)
{
    std::vector<double> strikes;
    strikes.reserve(chain.size());
    for (const OptionQuote& quote : chain)
    {
        if (std::isfinite(quote.strike) && quote.strike > 0.0)
        {
            strikes.push_back(quote.strike);
        }
    }
    std::ranges::sort(strikes);
    const auto repeats = std::ranges::unique(strikes, [](double a, double b) { return b - a <= kStrikeMatch; });
    strikes.erase(repeats.begin(), repeats.end());
    return strikes;
}

std::optional<std::size_t> nearestStrikeIndex(std::span<const double> strikes, double spot) noexcept
{
    if (strikes.empty() || !std::isfinite(spot))
    {
        return std::nullopt;
    }
    std::size_t best = 0;
    for (std::size_t index = 1; index < strikes.size(); ++index)
    {
        if (std::fabs(strikes[index] - spot) < std::fabs(strikes[best] - spot))
        {
            best = index;
        }
    }
    return best;
}

const OptionQuote* findQuote(std::span<const OptionQuote> chain, OptionRight right, double strike) noexcept
{
    for (const OptionQuote& quote : chain)
    {
        if (quote.right == right && std::fabs(quote.strike - strike) <= kStrikeMatch)
        {
            return &quote;
        }
    }
    return nullptr;
}

std::optional<double> parityImpliedSpot(std::span<const OptionQuote> chain)
{
    std::optional<double> spot;
    double closest = 0.0;
    for (const OptionQuote& call : chain)
    {
        if (call.right != OptionRight::Call)
        {
            continue;
        }
        const OptionQuote* put = findQuote(chain, OptionRight::Put, call.strike);
        if (put == nullptr)
        {
            continue;
        }
        const double call_price = midPrice(call);
        const double put_price = midPrice(*put);
        if (call_price <= 0.0 || put_price <= 0.0)
        {
            continue;
        }
        const double gap = std::fabs(call_price - put_price);
        if (!spot.has_value() || gap < closest)
        {
            closest = gap;
            spot = call.strike + call_price - put_price;
        }
    }
    if (spot.has_value() && *spot <= 0.0)
    {
        return std::nullopt;
    }
    return spot;
}

std::span<const StrategyTemplate> strategyTemplates() noexcept
{
    return kTemplates;
}

const StrategyTemplate* findStrategyTemplate(std::string_view id) noexcept
{
    for (const StrategyTemplate& recipe : kTemplates)
    {
        if (id == recipe.id)
        {
            return &recipe;
        }
    }
    return nullptr;
}

std::vector<std::size_t> defaultSlotStrikes(const StrategyTemplate& recipe,
                                            std::size_t strike_count,
                                            std::size_t atm_index)
{
    std::vector<std::size_t> picks;
    if (strike_count == 0)
    {
        return picks;
    }
    const auto last = static_cast<long long>(strike_count - 1);
    const long long atm = std::min(static_cast<long long>(atm_index), last);
    for (const StrikeSlot& slot : recipe.slots)
    {
        picks.push_back(static_cast<std::size_t>(std::clamp(atm + slot.offset, 0LL, last)));
    }
    return picks;
}

TemplateBuild buildTemplate(const StrategyTemplate& recipe,
                            std::span<const OptionQuote> chain,
                            const TemplateInputs& inputs)
{
    TemplateBuild build;
    if (inputs.slot_strikes.size() != recipe.slots.size())
    {
        build.error = "choose a strike for every slot";
        return build;
    }
    for (std::size_t index = 1; index < inputs.slot_strikes.size(); ++index)
    {
        if (!(inputs.slot_strikes[index] > inputs.slot_strikes[index - 1]))
        {
            build.error = std::string(recipe.slots[index].label) + " must be above " + recipe.slots[index - 1].label;
            return build;
        }
    }
    if (!std::isfinite(inputs.quantity) || inputs.quantity <= 0.0)
    {
        build.error = "quantity must be positive";
        return build;
    }

    std::vector<PayoffLeg> legs;
    for (const TemplateLeg& part : recipe.legs)
    {
        const double quantity = part.ratio * inputs.quantity;
        if (part.instrument == Underlying)
        {
            if (!std::isfinite(inputs.underlying_price) || inputs.underlying_price <= 0.0)
            {
                build.error = "enter a spot price for the share leg";
                return build;
            }
            legs.push_back(underlyingLeg(quantity * kContractMultiplier, inputs.underlying_price));
            continue;
        }
        if (part.slot < 0 || static_cast<std::size_t>(part.slot) >= inputs.slot_strikes.size())
        {
            build.error = std::string(recipe.name) + " has a leg without a strike";
            return build;
        }
        const double strike = inputs.slot_strikes[static_cast<std::size_t>(part.slot)];
        const OptionRight right = rightOf(part.instrument);
        const OptionQuote* quote = findQuote(chain, right, strike);
        if (quote == nullptr)
        {
            build.error = formatStrike(strike) + (right == OptionRight::Call ? " call" : " put") + " is not listed";
            return build;
        }
        legs.push_back(legFromQuote(*quote, quantity, quotePrice(*quote, inputs.basis, quantity > 0.0)));
    }
    build.legs = std::move(legs);
    return build;
}

}  // namespace terminal
