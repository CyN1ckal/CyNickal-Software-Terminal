// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CChartbookFile.h"

#include "RepoRoot.h"
#include "chart/CStudyCompute.h"
#include "chart/studies/StudyRegistry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace terminal {
namespace {

constexpr float kMinSplitRatio = 0.05f;
constexpr float kMaxSplitRatio = 0.95f;
constexpr int kLayoutDepthLimit = 32;
constexpr std::size_t kNoParent = static_cast<std::size_t>(-1);
constexpr std::string_view kReservedStemChars = "<>:\"/\\|?*";

using nlohmann::json;

[[nodiscard]] float clampSplitRatio(float ratio) noexcept
{
    if (ratio < kMinSplitRatio)
    {
        return kMinSplitRatio;
    }
    if (ratio > kMaxSplitRatio)
    {
        return kMaxSplitRatio;
    }
    return ratio;
}

[[nodiscard]] const char* periodName(ChartBarPeriod period) noexcept
{
    return chartPeriodCode(period);
}

[[nodiscard]] bool parsePeriod(std::string_view text, ChartBarPeriod& period)
{
    if (text == "1m")
    {
        period = ChartBarPeriod::Minute1;
        return true;
    }
    if (text == "5m")
    {
        period = ChartBarPeriod::Minute5;
        return true;
    }
    if (text == "15m")
    {
        period = ChartBarPeriod::Minute15;
        return true;
    }
    if (text == "1h")
    {
        period = ChartBarPeriod::Hour1;
        return true;
    }
    if (text == "1d")
    {
        period = ChartBarPeriod::Day1;
        return true;
    }
    return false;
}

[[nodiscard]] const char* barTypeName(ChartBarType type) noexcept
{
    switch (type)
    {
    case ChartBarType::Candlestick:
        return "candlestick";
    case ChartBarType::Ohlc:
        return "ohlc";
    case ChartBarType::LineOnClose:
        return "line_on_close";
    }
    return "candlestick";
}

[[nodiscard]] bool parseBarType(std::string_view text, ChartBarType& type)
{
    if (text == "candlestick")
    {
        type = ChartBarType::Candlestick;
        return true;
    }
    if (text == "ohlc")
    {
        type = ChartBarType::Ohlc;
        return true;
    }
    if (text == "line_on_close")
    {
        type = ChartBarType::LineOnClose;
        return true;
    }
    return false;
}

[[nodiscard]] const char* limitName(ChartDataLimitMode mode) noexcept
{
    switch (mode)
    {
    case ChartDataLimitMode::SessionCount:
        return "session_count";
    case ChartDataLimitMode::BarCount:
        return "bar_count";
    case ChartDataLimitMode::DateRange:
        return "date_range";
    }
    return "session_count";
}

[[nodiscard]] bool parseLimit(std::string_view text, ChartDataLimitMode& mode)
{
    if (text == "session_count")
    {
        mode = ChartDataLimitMode::SessionCount;
        return true;
    }
    if (text == "bar_count")
    {
        mode = ChartDataLimitMode::BarCount;
        return true;
    }
    if (text == "date_range")
    {
        mode = ChartDataLimitMode::DateRange;
        return true;
    }
    return false;
}

[[nodiscard]] const char* scaleName(ChartScaleRange range) noexcept
{
    switch (range)
    {
    case ChartScaleRange::Automatic:
        return "automatic";
    case ChartScaleRange::ConstantRange:
        return "constant_range";
    case ChartScaleRange::UserDefined:
        return "user_defined";
    }
    return "automatic";
}

[[nodiscard]] bool parseScale(std::string_view text, ChartScaleRange& range)
{
    if (text == "automatic")
    {
        range = ChartScaleRange::Automatic;
        return true;
    }
    if (text == "constant_range")
    {
        range = ChartScaleRange::ConstantRange;
        return true;
    }
    if (text == "user_defined")
    {
        range = ChartScaleRange::UserDefined;
        return true;
    }
    return false;
}

[[nodiscard]] const char* interactiveName(ChartInteractiveScale scale) noexcept
{
    switch (scale)
    {
    case ChartInteractiveScale::Range:
        return "range";
    case ChartInteractiveScale::Move:
        return "move";
    case ChartInteractiveScale::Locked:
        return "locked";
    }
    return "move";
}

[[nodiscard]] bool parseInteractive(std::string_view text, ChartInteractiveScale& scale)
{
    if (text == "range")
    {
        scale = ChartInteractiveScale::Range;
        return true;
    }
    if (text == "move")
    {
        scale = ChartInteractiveScale::Move;
        return true;
    }
    if (text == "locked")
    {
        scale = ChartInteractiveScale::Locked;
        return true;
    }
    return false;
}

[[nodiscard]] bool fail(std::string& error, std::string message)
{
    error = std::move(message);
    return false;
}

[[nodiscard]] bool readObject(const json& value, std::string_view what, const json*& out, std::string& error)
{
    if (!value.is_object())
    {
        return fail(error, std::string(what) + " is not an object");
    }
    out = &value;
    return true;
}

[[nodiscard]] bool readInt(const json& object, const char* key, int& out, std::string& error)
{
    if (!object.contains(key) || !object.at(key).is_number_integer())
    {
        return fail(error, std::string(key) + " is missing");
    }
    out = object.at(key).get<int>();
    return true;
}

[[nodiscard]] bool readBool(const json& object, const char* key, bool& out, std::string& error)
{
    if (!object.contains(key) || !object.at(key).is_boolean())
    {
        return fail(error, std::string(key) + " is missing");
    }
    out = object.at(key).get<bool>();
    return true;
}

[[nodiscard]] bool readNumber(const json& object, const char* key, double& out, std::string& error)
{
    if (!object.contains(key) || !object.at(key).is_number())
    {
        return fail(error, std::string(key) + " is missing");
    }
    out = object.at(key).get<double>();
    return true;
}

[[nodiscard]] bool readString(const json& object, const char* key, std::string& out, std::string& error)
{
    if (!object.contains(key) || !object.at(key).is_string())
    {
        return fail(error, std::string(key) + " is missing");
    }
    out = object.at(key).get<std::string>();
    return true;
}

[[nodiscard]] bool readNamedColor(const json& object, const char* key, std::uint32_t& out, std::string& error)
{
    if (!object.contains(key) || !object.at(key).is_number_unsigned())
    {
        return fail(error, std::string(key) + " is missing");
    }
    const auto raw = object.at(key).get<std::uint64_t>();
    if (raw > 0xFFFFFFFFu)
    {
        return fail(error, std::string(key) + " is out of range");
    }
    out = static_cast<std::uint32_t>(raw);
    return true;
}

[[nodiscard]] bool readColor(const json& object, std::uint32_t& out, std::string& error)
{
    return readNamedColor(object, "color", out, error);
}

[[nodiscard]] json settingsToJson(const CChartSettings& settings)
{
    json object = json::object();
    object["symbol"] = settings.symbol;
    object["period"] = periodName(settings.period);
    object["bar_type"] = barTypeName(settings.bar_type);
    object["limit_mode"] = limitName(settings.limit_mode);
    object["intraday_session_count"] = settings.intraday_session_count;
    object["historical_session_count"] = settings.historical_session_count;
    object["scale_range"] = scaleName(settings.scale_range);
    object["constant_range"] = settings.constant_range;
    object["user_top"] = settings.user_top;
    object["user_bottom"] = settings.user_bottom;
    object["bar_spacing_px"] = settings.bar_spacing_px;
    object["bar_width_frac"] = settings.bar_width_frac;
    object["scale_padding_pct"] = settings.scale_padding_pct;
    return object;
}

[[nodiscard]] bool settingsFromJson(const json& value, CChartSettings& settings, std::string& error)
{
    const json* object = nullptr;
    if (!readObject(value, "settings", object, error))
    {
        return false;
    }
    if (!readString(*object, "symbol", settings.symbol, error))
    {
        return false;
    }
    std::string text;
    if (!readString(*object, "period", text, error) || !parsePeriod(text, settings.period))
    {
        return fail(error, "unknown period");
    }
    if (!readString(*object, "bar_type", text, error) || !parseBarType(text, settings.bar_type))
    {
        return fail(error, "unknown bar type");
    }
    if (!readString(*object, "limit_mode", text, error) || !parseLimit(text, settings.limit_mode))
    {
        return fail(error, "unknown limit mode");
    }
    if (!readInt(*object, "intraday_session_count", settings.intraday_session_count, error) ||
        !readInt(*object, "historical_session_count", settings.historical_session_count, error))
    {
        return false;
    }
    if (!readString(*object, "scale_range", text, error) || !parseScale(text, settings.scale_range))
    {
        return fail(error, "unknown scale range");
    }
    double number = 0.0;
    if (!readNumber(*object, "constant_range", number, error))
    {
        return false;
    }
    settings.constant_range = number;
    if (!readNumber(*object, "user_top", number, error))
    {
        return false;
    }
    settings.user_top = number;
    if (!readNumber(*object, "user_bottom", number, error))
    {
        return false;
    }
    settings.user_bottom = number;
    if (!readNumber(*object, "bar_spacing_px", number, error))
    {
        return false;
    }
    settings.bar_spacing_px = static_cast<float>(number);
    if (!readNumber(*object, "bar_width_frac", number, error))
    {
        return false;
    }
    settings.bar_width_frac = static_cast<float>(number);
    if (!readNumber(*object, "scale_padding_pct", number, error))
    {
        return false;
    }
    settings.scale_padding_pct = static_cast<float>(number);
    return true;
}

void writeStudyOptions(json& object, const CStudyInstance& study, const StudyType& type)
{
    for (std::size_t index = 0; index < type.options.size(); ++index)
    {
        const StudyOption& option = type.options[index];
        int value = option.fallback;
        if (index < study.options.size())
        {
            value = study.options[index];
        }
        if (option.choices.empty())
        {
            object[option.key] = value;
            continue;
        }
        const auto count = static_cast<int>(option.choices.size());
        if (value < 0 || value >= count)
        {
            value = option.fallback;
        }
        if (value < 0 || value >= count)
        {
            value = 0;
        }
        object[option.key] = option.choices[static_cast<std::size_t>(value)].token;
    }
}

[[nodiscard]] bool readStudyOptions(const json& object,
                                    const StudyType& type,
                                    CStudyInstance& study,
                                    std::string& error)
{
    study.options.assign(type.options.size(), 0);
    for (std::size_t index = 0; index < type.options.size(); ++index)
    {
        const StudyOption& option = type.options[index];
        if (option.choices.empty())
        {
            if (!readInt(object, option.key, study.options[index], error))
            {
                return false;
            }
            continue;
        }
        std::string text;
        if (!readString(object, option.key, text, error))
        {
            return false;
        }
        const auto found = std::ranges::find_if(option.choices, [&](const StudyChoice& choice) {
            return text == choice.token;
        });
        if (found == option.choices.end())
        {
            return fail(error, std::string("unknown ") + option.key);
        }
        study.options[index] = static_cast<int>(found - option.choices.begin());
    }
    return true;
}

void writeStudyOutputs(json& object, const CStudyInstance& study, const StudyType& type)
{
    if (type.outputs.empty())
    {
        return;
    }
    json rows = json::array();
    const bool lines = type.graph == StudyGraph::Line;
    for (std::size_t index = 0; index < type.outputs.size(); ++index)
    {
        const StudyOutput& output = type.outputs[index];
        const bool have = index < study.outputs.size();
        const std::uint32_t color = have ? study.outputs[index].color : study.color;
        const StudyLineStyle line = have ? study.outputs[index].line : StudyLineStyle::Solid;
        json row = json::object();
        row["key"] = output.key != nullptr ? output.key : "";
        row["color"] = color;
        if (lines)
        {
            row["line"] = studyLineStyleToken(line);
        }
        rows.push_back(std::move(row));
    }
    object["outputs"] = std::move(rows);
}

[[nodiscard]] bool readStudyOutputs(const json& object,
                                    const StudyType& type,
                                    CStudyInstance& study,
                                    std::string& error)
{
    study.outputs.assign(type.outputs.size(),
                         CStudyOutputStyle{.color = study.color, .line = StudyLineStyle::Solid});
    if (!object.contains("outputs"))
    {
        if (type.color_by_bar)
        {
            assignStudyOutputDefaults(study, 0);
        }
        else if (!study.outputs.empty())
        {
            study.color = study.outputs.front().color;
        }
        return true;
    }
    const json& rows = object.at("outputs");
    if (!rows.is_array())
    {
        return fail(error, "outputs is not an array");
    }
    bool saw_output = false;
    for (const json& row : rows)
    {
        if (!row.is_object())
        {
            return fail(error, "output is not an object");
        }
        std::string key;
        if (!readString(row, "key", key, error))
        {
            return false;
        }
        // Older volume studies stored one label color under "volume". Bars did not use it.
        if (type.color_by_bar && key == "volume")
        {
            continue;
        }
        const auto found = std::ranges::find_if(type.outputs, [&](const StudyOutput& output) {
            return output.key != nullptr && key == output.key;
        });
        if (found == type.outputs.end())
        {
            return fail(error, "unknown output");
        }
        saw_output = true;
        const auto index = static_cast<std::size_t>(found - type.outputs.begin());
        if (!readNamedColor(row, "color", study.outputs[index].color, error))
        {
            return false;
        }
        if (!row.contains("line"))
        {
            continue;
        }
        std::string line;
        if (!readString(row, "line", line, error))
        {
            return false;
        }
        StudyLineStyle style = StudyLineStyle::Solid;
        if (!parseStudyLineStyle(line, style))
        {
            return fail(error, "unknown line");
        }
        study.outputs[index].line = style;
    }
    if (type.color_by_bar && !saw_output)
    {
        assignStudyOutputDefaults(study, 0);
        return true;
    }
    if (!study.outputs.empty())
    {
        study.color = study.outputs.front().color;
    }
    return true;
}

[[nodiscard]] json studyToJson(const CStudyInstance& study)
{
    json object = json::object();
    object["id"] = study.id;
    object["kind"] = study.type_id;
    object["enabled"] = study.enabled;
    object["color"] = studyPrimaryColor(study);
    object["chart_region"] = study.chart_region;
    if (const StudyType* type = findStudy(study.type_id))
    {
        writeStudyOptions(object, study, *type);
        writeStudyOutputs(object, study, *type);
    }
    return object;
}

[[nodiscard]] bool studyFromJson(const json& value, CStudyInstance& study, bool& skip, std::string& error)
{
    skip = false;
    const json* object = nullptr;
    if (!readObject(value, "study", object, error))
    {
        return false;
    }
    std::string kind_text;
    if (!readString(*object, "kind", kind_text, error))
    {
        return false;
    }
    const StudyType* type = findStudy(kind_text);
    if (type == nullptr)
    {
        skip = true;
        return true;
    }
    study.type_id = kind_text;
    if (!readInt(*object, "id", study.id, error) || !readBool(*object, "enabled", study.enabled, error) ||
        !readColor(*object, study.color, error) ||
        !readInt(*object, "chart_region", study.chart_region, error))
    {
        return false;
    }
    return readStudyOptions(*object, *type, study, error) &&
           readStudyOutputs(*object, *type, study, error);
}

[[nodiscard]] json paneToJson(const ChartbookPane& pane)
{
    json studies = json::array();
    for (const CStudyInstance& study : pane.studies)
    {
        studies.push_back(studyToJson(study));
    }
    json ratios = json::array();
    for (const float ratio : pane.region_ratios)
    {
        ratios.push_back(ratio);
    }
    json object = json::object();
    object["id"] = pane.id;
    object["settings"] = settingsToJson(pane.settings);
    object["interactive_scale"] = interactiveName(pane.interactive);
    object["region_ratios"] = std::move(ratios);
    object["next_study_id"] = pane.next_study_id;
    object["studies"] = std::move(studies);
    return object;
}

[[nodiscard]] bool paneFromJson(const json& value, ChartbookPane& pane, std::string& error)
{
    const json* object = nullptr;
    if (!readObject(value, "pane", object, error))
    {
        return false;
    }
    if (!readInt(*object, "id", pane.id, error) || pane.id <= 0)
    {
        return fail(error, "pane id is missing");
    }
    if (!object->contains("settings"))
    {
        return fail(error, "settings is missing");
    }
    if (!settingsFromJson(object->at("settings"), pane.settings, error))
    {
        return false;
    }
    std::string text;
    if (!readString(*object, "interactive_scale", text, error) || !parseInteractive(text, pane.interactive))
    {
        return fail(error, "unknown interactive scale");
    }
    if (object->contains("region_ratios"))
    {
        const json& ratios = object->at("region_ratios");
        if (!ratios.is_array())
        {
            return fail(error, "region_ratios is not an array");
        }
        for (const json& ratio : ratios)
        {
            if (!ratio.is_number())
            {
                return fail(error, "region ratio is not a number");
            }
            pane.region_ratios.push_back(ratio.get<float>());
        }
    }
    if (!readInt(*object, "next_study_id", pane.next_study_id, error) || pane.next_study_id <= 0)
    {
        return fail(error, "next_study_id is missing");
    }
    if (!object->contains("studies") || !object->at("studies").is_array())
    {
        return fail(error, "studies is missing");
    }
    for (const json& study_value : object->at("studies"))
    {
        CStudyInstance study;
        bool skip = false;
        if (!studyFromJson(study_value, study, skip, error))
        {
            return false;
        }
        if (!skip)
        {
            pane.studies.push_back(study);
        }
    }
    return true;
}

[[nodiscard]] json dataToJson(const ChartbookData& data)
{
    json columns = json::array();
    for (const ChartbookColumn& column : data.columns)
    {
        json item = json::object();
        item["id"] = column.id;
        item["width"] = column.width;
        item["visible"] = column.visible;
        item["order"] = column.order;
        columns.push_back(std::move(item));
    }
    json object = json::object();
    object["symbol"] = data.symbol;
    object["from"] = data.from;
    object["to"] = data.to;
    object["ingest_timeframe"] = data.ingest_timeframe;
    object["selected_symbol"] = data.selected_symbol;
    object["selected_timeframe"] = data.selected_timeframe;
    if (!data.sort_column.empty())
    {
        object["sort_column"] = data.sort_column;
        object["sort_descending"] = data.sort_descending;
    }
    object["columns"] = std::move(columns);
    return object;
}

[[nodiscard]] bool readFinancialsFields(const json& object, ChartbookFinancials& financials, std::string& error)
{
    if (object.contains("symbol") && !readString(object, "symbol", financials.symbol, error))
    {
        return false;
    }
    if (object.contains("statement"))
    {
        if (!readString(object, "statement", financials.statement, error))
        {
            return false;
        }
        if (financials.statement != "income" && financials.statement != "balance" &&
            financials.statement != "cashflow")
        {
            return fail(error, "unknown statement");
        }
    }
    if (object.contains("timeframe"))
    {
        if (!readString(object, "timeframe", financials.timeframe, error))
        {
            return false;
        }
        if (financials.timeframe != "annually" && financials.timeframe != "quarterly")
        {
            return fail(error, "unknown statement timeframe");
        }
    }
    return true;
}

[[nodiscard]] bool acceptFinancialsId(const CChartbookDocument& document, int id, std::string& error)
{
    if (id <= 0)
    {
        return fail(error, "financials id is missing");
    }
    const bool duplicate = std::ranges::any_of(document.financials, [id](const ChartbookFinancials& existing) {
        return existing.id == id;
    });
    if (duplicate || document.next_financials_id <= id)
    {
        return fail(error, "financials id is out of range");
    }
    return true;
}

[[nodiscard]] json financialsToJson(const std::vector<ChartbookFinancials>& financials)
{
    json array = json::array();
    for (const ChartbookFinancials& panel : financials)
    {
        json object = json::object();
        object["id"] = panel.id;
        object["symbol"] = panel.symbol;
        object["statement"] = panel.statement;
        object["timeframe"] = panel.timeframe;
        array.push_back(std::move(object));
    }
    return array;
}

// An object is the older single sheet. An array is the collection. next_present says the file
// already stored next_financials_id; a legacy object fills that in when it was omitted.
[[nodiscard]] bool financialsFromJson(const json& value, CChartbookDocument& document, bool next_present,
                                      std::string& error)
{
    if (value.is_object())
    {
        ChartbookFinancials panel;
        if (value.contains("id"))
        {
            if (!readInt(value, "id", panel.id, error) || panel.id <= 0)
            {
                return fail(error, "financials id is missing");
            }
        }
        else
        {
            panel.id = 1;
        }
        if (!readFinancialsFields(value, panel, error))
        {
            return false;
        }
        if (!next_present || document.next_financials_id <= panel.id)
        {
            document.next_financials_id = panel.id + 1;
        }
        if (!acceptFinancialsId(document, panel.id, error))
        {
            return false;
        }
        document.financials.push_back(std::move(panel));
        return true;
    }
    if (!value.is_array())
    {
        return fail(error, "financials is not an array");
    }
    if (!next_present)
    {
        return fail(error, "next_financials_id is missing");
    }
    for (const json& item_value : value)
    {
        const json* item = nullptr;
        if (!readObject(item_value, "financials", item, error))
        {
            return false;
        }
        ChartbookFinancials panel;
        if (!readInt(*item, "id", panel.id, error) || panel.id <= 0)
        {
            return fail(error, "financials id is missing");
        }
        if (!readFinancialsFields(*item, panel, error))
        {
            return false;
        }
        if (!acceptFinancialsId(document, panel.id, error))
        {
            return false;
        }
        document.financials.push_back(std::move(panel));
    }
    return true;
}

[[nodiscard]] bool validOptionExpiration(int expiration)
{
    if (expiration == 0)
    {
        return true;
    }
    if (expiration < 19000101 || expiration > 21001231)
    {
        return false;
    }
    const int month = (expiration / 100) % 100;
    const int day = expiration % 100;
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

[[nodiscard]] bool readOptionsFields(const json& object, ChartbookOptions& panel, std::string& error)
{
    if (object.contains("symbol") && !readString(object, "symbol", panel.symbol, error))
    {
        return false;
    }
    if (object.contains("expiration") && !readInt(object, "expiration", panel.expiration, error))
    {
        return false;
    }
    if (!validOptionExpiration(panel.expiration))
    {
        return fail(error, "option expiration is invalid");
    }
    if (object.contains("expiration_type") &&
        !readString(object, "expiration_type", panel.expiration_type, error))
    {
        return false;
    }
    if (panel.expiration == 0)
    {
        if (!panel.expiration_type.empty())
        {
            return fail(error, "option expiration type needs a date");
        }
        return true;
    }
    if (panel.expiration_type != "weekly" && panel.expiration_type != "monthly")
    {
        return fail(error, "unknown option expiration type");
    }
    return true;
}

[[nodiscard]] bool acceptOptionsId(const CChartbookDocument& document, int id, std::string& error)
{
    if (id <= 0)
    {
        return fail(error, "options id is missing");
    }
    const bool duplicate = std::ranges::any_of(document.options, [id](const ChartbookOptions& existing) {
        return existing.id == id;
    });
    if (duplicate || document.next_options_id <= id)
    {
        return fail(error, "options id is out of range");
    }
    return true;
}

[[nodiscard]] json optionsToJson(const std::vector<ChartbookOptions>& options)
{
    json array = json::array();
    for (const ChartbookOptions& panel : options)
    {
        json object = json::object();
        object["id"] = panel.id;
        object["symbol"] = panel.symbol;
        object["expiration"] = panel.expiration;
        object["expiration_type"] = panel.expiration_type;
        array.push_back(std::move(object));
    }
    return array;
}

[[nodiscard]] bool optionsFromJson(const json& value, CChartbookDocument& document, bool next_present,
                                   std::string& error)
{
    if (!value.is_array())
    {
        return fail(error, "options is not an array");
    }
    if (!next_present)
    {
        return fail(error, "next_options_id is missing");
    }
    for (const json& item_value : value)
    {
        const json* item = nullptr;
        if (!readObject(item_value, "options", item, error))
        {
            return false;
        }
        ChartbookOptions panel;
        if (!readInt(*item, "id", panel.id, error) || panel.id <= 0)
        {
            return fail(error, "options id is missing");
        }
        if (!readOptionsFields(*item, panel, error))
        {
            return false;
        }
        if (!acceptOptionsId(document, panel.id, error))
        {
            return false;
        }
        document.options.push_back(std::move(panel));
    }
    return true;
}

[[nodiscard]] bool dataFromJson(const json& value, ChartbookData& data, std::string& error)
{
    const json* object = nullptr;
    if (!readObject(value, "data", object, error))
    {
        return false;
    }
    if (object->contains("symbol") && !readString(*object, "symbol", data.symbol, error))
    {
        return false;
    }
    if (object->contains("from") && !readString(*object, "from", data.from, error))
    {
        return false;
    }
    if (object->contains("to") && !readString(*object, "to", data.to, error))
    {
        return false;
    }
    if (object->contains("ingest_timeframe"))
    {
        if (!readString(*object, "ingest_timeframe", data.ingest_timeframe, error))
        {
            return false;
        }
        if (data.ingest_timeframe != "1m" && data.ingest_timeframe != "1d")
        {
            return fail(error, "unknown ingest timeframe");
        }
    }
    if (object->contains("selected_symbol") &&
        !readString(*object, "selected_symbol", data.selected_symbol, error))
    {
        return false;
    }
    if (object->contains("selected_timeframe"))
    {
        if (!readString(*object, "selected_timeframe", data.selected_timeframe, error))
        {
            return false;
        }
        if (!data.selected_timeframe.empty() && data.selected_timeframe != "1m" &&
            data.selected_timeframe != "1d")
        {
            return fail(error, "unknown selected timeframe");
        }
    }
    if (object->contains("sort_column"))
    {
        if (!readString(*object, "sort_column", data.sort_column, error))
        {
            return false;
        }
        if (!data.sort_column.empty() && !isChartbookDataColumn(data.sort_column))
        {
            data.sort_column.clear();
        }
        if (!data.sort_column.empty())
        {
            if (!object->contains("sort_descending"))
            {
                data.sort_descending = false;
            }
            else if (!readBool(*object, "sort_descending", data.sort_descending, error))
            {
                return false;
            }
        }
    }
    if (!object->contains("columns"))
    {
        return true;
    }
    const json& columns = object->at("columns");
    if (!columns.is_array())
    {
        return fail(error, "columns is not an array");
    }
    for (const json& column_value : columns)
    {
        const json* column_object = nullptr;
        if (!readObject(column_value, "column", column_object, error))
        {
            return false;
        }
        ChartbookColumn column;
        if (!readString(*column_object, "id", column.id, error))
        {
            return false;
        }
        if (!isChartbookDataColumn(column.id))
        {
            continue;
        }
        if (column_object->contains("width"))
        {
            double width = 0.0;
            if (!readNumber(*column_object, "width", width, error))
            {
                return false;
            }
            column.width = static_cast<float>(width);
        }
        if (column_object->contains("visible") &&
            !readBool(*column_object, "visible", column.visible, error))
        {
            return false;
        }
        if (column_object->contains("order") && !readInt(*column_object, "order", column.order, error))
        {
            return false;
        }
        data.columns.push_back(std::move(column));
    }
    return true;
}

[[nodiscard]] bool parseWindowToken(std::string_view window, std::string& error)
{
    int financials_id = 0;
    int options_id = 0;
    if (window == "data" || window == "financials" || financialsIdFromWindow(window, financials_id) ||
        optionsIdFromWindow(window, options_id))
    {
        return true;
    }
    constexpr std::string_view prefix = "pane:";
    if (!window.starts_with(prefix))
    {
        return fail(error, "unknown window id");
    }
    const std::string_view digits = window.substr(prefix.size());
    if (digits.empty() || digits.size() > 9)
    {
        return fail(error, "unknown window id");
    }
    int value = 0;
    for (const char digit : digits)
    {
        if (digit < '0' || digit > '9')
        {
            return fail(error, "unknown window id");
        }
        value = (value * 10) + (digit - '0');
    }
    if (value <= 0)
    {
        return fail(error, "unknown window id");
    }
    return true;
}

[[nodiscard]] bool paneIdFromWindow(std::string_view window, int& pane_id)
{
    constexpr std::string_view prefix = "pane:";
    if (!window.starts_with(prefix))
    {
        return false;
    }
    pane_id = 0;
    for (const char digit : window.substr(prefix.size()))
    {
        pane_id = (pane_id * 10) + (digit - '0');
    }
    return pane_id > 0;
}

[[nodiscard]] json layoutToJson(const ChartbookLayout& layout)
{
    if (layout.root < 0 || std::cmp_greater_equal(layout.root, layout.nodes.size()))
    {
        return json::object();
    }
    std::vector<json> built(layout.nodes.size());
    std::vector<char> done(layout.nodes.size(), 0);
    std::vector<int> stack;
    stack.push_back(layout.root);
    for (int steps = 0; !stack.empty() && steps < kLayoutDepthLimit * 4; ++steps)
    {
        const int index = stack.back();
        if (index < 0 || std::cmp_greater_equal(index, layout.nodes.size()))
        {
            stack.pop_back();
            continue;
        }
        const ChartbookLayoutNode& node = layout.nodes[static_cast<std::size_t>(index)];
        if (!node.is_split)
        {
            json windows = json::array();
            for (const std::string& window : node.windows)
            {
                windows.push_back(window);
            }
            json object = json::object();
            object["windows"] = std::move(windows);
            object["selected"] = node.selected;
            built[static_cast<std::size_t>(index)] = std::move(object);
            done[static_cast<std::size_t>(index)] = 1;
            stack.pop_back();
            continue;
        }
        const bool first_ready = node.first < 0 || done[static_cast<std::size_t>(node.first)] != 0;
        const bool second_ready = node.second < 0 || done[static_cast<std::size_t>(node.second)] != 0;
        if (!first_ready || !second_ready)
        {
            if (!second_ready)
            {
                stack.push_back(node.second);
            }
            if (!first_ready)
            {
                stack.push_back(node.first);
            }
            continue;
        }
        json object = json::object();
        object["split"] = node.axis == ChartbookSplitAxis::Vertical ? "vertical" : "horizontal";
        object["ratio"] = node.ratio;
        object["first"] = node.first >= 0 ? built[static_cast<std::size_t>(node.first)] : json::object();
        object["second"] = node.second >= 0 ? built[static_cast<std::size_t>(node.second)] : json::object();
        built[static_cast<std::size_t>(index)] = std::move(object);
        done[static_cast<std::size_t>(index)] = 1;
        stack.pop_back();
    }
    return built[static_cast<std::size_t>(layout.root)];
}

struct LayoutWork
{
    const json* value{nullptr};
    int phase{0};
    int depth{0};
    std::size_t parent{kNoParent};
    bool second{false};
    int first{-1};
    int second_index{-1};
};

[[nodiscard]] bool readLayout(const json& value, ChartbookLayout& layout, int& out_index, int depth,
                             std::string& error)
{
    std::vector<LayoutWork> stack;
    LayoutWork root_work;
    root_work.value = &value;
    root_work.depth = depth;
    stack.push_back(root_work);
    for (int steps = 0; !stack.empty() && steps < kLayoutDepthLimit * 4; ++steps)
    {
        const std::size_t pos = stack.size() - 1;
        if (stack[pos].phase == 0)
        {
            const json* object = nullptr;
            if (!readObject(*stack[pos].value, "layout", object, error))
            {
                return false;
            }
            if (object->contains("split"))
            {
                if (stack[pos].depth >= kLayoutDepthLimit)
                {
                    return fail(error, "layout is too deep");
                }
                if (!object->contains("first") || !object->contains("second"))
                {
                    return fail(error, "split is missing a child");
                }
                stack[pos].phase = 1;
                LayoutWork second_work;
                second_work.value = &object->at("second");
                second_work.depth = stack[pos].depth + 1;
                second_work.parent = pos;
                second_work.second = true;
                LayoutWork first_work;
                first_work.value = &object->at("first");
                first_work.depth = stack[pos].depth + 1;
                first_work.parent = pos;
                stack.push_back(second_work);
                stack.push_back(first_work);
                continue;
            }
            if (!object->contains("windows") || !object->at("windows").is_array())
            {
                return fail(error, "layout leaf is missing windows");
            }
            ChartbookLayoutNode node;
            for (const json& window_value : object->at("windows"))
            {
                if (!window_value.is_string())
                {
                    return fail(error, "window id is not a string");
                }
                std::string window = window_value.get<std::string>();
                if (!parseWindowToken(window, error))
                {
                    return false;
                }
                node.windows.push_back(std::move(window));
            }
            if (node.windows.empty())
            {
                return fail(error, "layout leaf has no windows");
            }
            if (!readString(*object, "selected", node.selected, error))
            {
                return false;
            }
            if (std::ranges::find(node.windows, node.selected) == node.windows.end())
            {
                return fail(error, "selected window is not in the leaf");
            }
            layout.nodes.push_back(std::move(node));
            const int result = static_cast<int>(layout.nodes.size()) - 1;
            const std::size_t parent = stack[pos].parent;
            const bool second = stack[pos].second;
            stack.pop_back();
            if (parent == kNoParent)
            {
                out_index = result;
                return true;
            }
            if (second)
            {
                stack[parent].second_index = result;
            }
            else
            {
                stack[parent].first = result;
            }
            continue;
        }

        const json* object = nullptr;
        if (!readObject(*stack[pos].value, "layout", object, error))
        {
            return false;
        }
        std::string axis_text;
        if (!readString(*object, "split", axis_text, error))
        {
            return false;
        }
        ChartbookSplitAxis axis = ChartbookSplitAxis::Horizontal;
        if (axis_text == "vertical")
        {
            axis = ChartbookSplitAxis::Vertical;
        }
        else if (axis_text != "horizontal")
        {
            return fail(error, "unknown split");
        }
        double ratio = 0.0;
        if (!readNumber(*object, "ratio", ratio, error))
        {
            return false;
        }
        ChartbookLayoutNode node;
        node.is_split = true;
        node.axis = axis;
        node.ratio = clampSplitRatio(static_cast<float>(ratio));
        node.first = stack[pos].first;
        node.second = stack[pos].second_index;
        layout.nodes.push_back(std::move(node));
        const int result = static_cast<int>(layout.nodes.size()) - 1;
        const std::size_t parent = stack[pos].parent;
        const bool second = stack[pos].second;
        stack.pop_back();
        if (parent == kNoParent)
        {
            out_index = result;
            return true;
        }
        if (second)
        {
            stack[parent].second_index = result;
        }
        else
        {
            stack[parent].first = result;
        }
    }
    return fail(error, "layout is too deep");
}

void collectWindows(const ChartbookLayout& layout, int start, std::vector<std::string>& out)
{
    std::vector<int> pending;
    if (start >= 0)
    {
        pending.push_back(start);
    }
    for (int steps = 0; !pending.empty() && steps < kLayoutDepthLimit * 4; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        if (index < 0 || std::cmp_greater_equal(index, layout.nodes.size()))
        {
            continue;
        }
        const ChartbookLayoutNode& node = layout.nodes[static_cast<std::size_t>(index)];
        if (node.is_split)
        {
            if (node.second >= 0)
            {
                pending.push_back(node.second);
            }
            if (node.first >= 0)
            {
                pending.push_back(node.first);
            }
            continue;
        }
        for (const std::string& window : node.windows)
        {
            out.push_back(window);
        }
    }
}

[[nodiscard]] bool documentWindowsOk(const CChartbookDocument& document, std::string& error)
{
    std::vector<std::string> windows;
    collectWindows(document.layout, document.layout.root, windows);
    for (const ChartbookFloating& floating : document.floating)
    {
        if (!parseWindowToken(floating.window, error))
        {
            return false;
        }
        if (floating.w <= 1.f || floating.h <= 1.f)
        {
            return fail(error, "floating window has no size");
        }
        windows.push_back(floating.window);
    }
    std::vector<std::string> sorted = windows;
    std::ranges::sort(sorted);
    if (std::ranges::adjacent_find(sorted) != sorted.end())
    {
        return fail(error, "window id is repeated");
    }
    for (const std::string& window : windows)
    {
        int pane_id = 0;
        if (!paneIdFromWindow(window, pane_id))
        {
            continue;
        }
        const bool found = std::ranges::any_of(document.panes, [&](const ChartbookPane& pane) {
            return pane.id == pane_id;
        });
        if (!found)
        {
            return fail(error, "layout names a missing pane");
        }
    }
    for (const std::string& window : windows)
    {
        if (window == "financials")
        {
            return fail(error, "financials window is missing an id");
        }
        int financials_id = 0;
        if (!financialsIdFromWindow(window, financials_id))
        {
            continue;
        }
        const bool found = std::ranges::any_of(document.financials, [&](const ChartbookFinancials& panel) {
            return panel.id == financials_id;
        });
        if (!found)
        {
            return fail(error, "layout names a missing financials panel");
        }
    }
    for (const std::string& window : windows)
    {
        int options_id = 0;
        if (!optionsIdFromWindow(window, options_id))
        {
            continue;
        }
        const bool found = std::ranges::any_of(document.options, [&](const ChartbookOptions& panel) {
            return panel.id == options_id;
        });
        if (!found)
        {
            return fail(error, "layout names a missing options panel");
        }
    }
    return true;
}

void replaceBareFinancials(std::string& window, const std::string& replacement)
{
    if (window == "financials")
    {
        window = replacement;
    }
}

[[nodiscard]] bool documentHasBareFinancials(const CChartbookDocument& document)
{
    if (std::ranges::any_of(document.floating, [](const ChartbookFloating& item) {
            return item.window == "financials";
        }))
    {
        return true;
    }
    return std::ranges::any_of(document.layout.nodes, [](const ChartbookLayoutNode& node) {
        return !node.is_split && (node.selected == "financials" ||
                                  std::ranges::find(node.windows, "financials") != node.windows.end());
    });
}

// Older files name the one sheet "financials". Rewrite that token onto the single panel id.
[[nodiscard]] bool migrateLegacyFinancialsWindow(CChartbookDocument& document, std::string& error)
{
    if (!documentHasBareFinancials(document))
    {
        return true;
    }
    if (document.financials.size() > 1)
    {
        return fail(error, "financials window is missing an id");
    }
    if (document.financials.empty())
    {
        ChartbookFinancials panel;
        panel.id = 1;
        if (document.next_financials_id <= panel.id)
        {
            document.next_financials_id = panel.id + 1;
        }
        document.financials.push_back(std::move(panel));
    }
    const std::string replacement = financialsWindowId(document.financials.front().id);
    for (ChartbookLayoutNode& node : document.layout.nodes)
    {
        if (node.is_split)
        {
            continue;
        }
        for (std::string& window : node.windows)
        {
            replaceBareFinancials(window, replacement);
        }
        replaceBareFinancials(node.selected, replacement);
    }
    for (ChartbookFloating& item : document.floating)
    {
        replaceBareFinancials(item.window, replacement);
    }
    return true;
}

[[nodiscard]] json documentToJson(const CChartbookDocument& document)
{
    json panes = json::array();
    for (const ChartbookPane& pane : document.panes)
    {
        panes.push_back(paneToJson(pane));
    }
    json floating = json::array();
    for (const ChartbookFloating& item : document.floating)
    {
        json object = json::object();
        object["window"] = item.window;
        object["x"] = item.x;
        object["y"] = item.y;
        object["w"] = item.w;
        object["h"] = item.h;
        floating.push_back(std::move(object));
    }
    json layout = layoutToJson(document.layout);
    json root = json::object();
    root["format"] = document.format;
    root["name"] = document.name;
    root["focused_pane"] = document.focused_pane;
    root["next_pane_id"] = document.next_pane_id;
    root["focused_financials"] = document.focused_financials;
    root["next_financials_id"] = document.next_financials_id;
    root["focused_options"] = document.focused_options;
    root["next_options_id"] = document.next_options_id;
    root["data"] = dataToJson(document.data);
    root["financials"] = financialsToJson(document.financials);
    root["options"] = optionsToJson(document.options);
    root["layout"] = std::move(layout);
    root["floating"] = std::move(floating);
    root["panes"] = std::move(panes);
    return root;
}

[[nodiscard]] std::filesystem::path normalizePath(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error)
    {
        return canonical;
    }
    return path.lexically_normal();
}

[[nodiscard]] std::string_view stripChartbookSuffix(std::string_view name) noexcept
{
    const std::string_view suffix = kChartbookFileSuffix;
    if (name.size() > suffix.size() && name.ends_with(suffix))
    {
        name.remove_suffix(suffix.size());
    }
    return name;
}

}  // namespace

std::filesystem::path chartbooksDirectory()
{
    return findRepoRoot() / "data" / "chartbooks";
}

std::filesystem::path defaultTerminalSettingsPath()
{
    return findRepoRoot() / "data" / "terminal.json";
}

bool isSafeChartbookStem(std::string_view stem)
{
    if (stem.empty() || stem == "." || stem == "..")
    {
        return false;
    }
    return stem.find_first_of(kReservedStemChars) == std::string_view::npos;
}

std::filesystem::path chartbookPathForStem(std::string_view stem)
{
    const std::string stripped{stripChartbookSuffix(stem)};
    if (!isSafeChartbookStem(stripped))
    {
        return {};
    }
    return chartbooksDirectory() / (stripped + std::string(kChartbookFileSuffix));
}

std::string chartbookStemFromPath(const std::filesystem::path& path)
{
    return std::string(stripChartbookSuffix(path.filename().string()));
}

std::string pathForStorage(const std::filesystem::path& path)
{
    const std::filesystem::path root = normalizePath(findRepoRoot());
    const std::filesystem::path absolute = normalizePath(path);
    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(absolute, root, error);
    if (!error && !relative.empty() && *relative.begin() != "..")
    {
        return relative.generic_string();
    }
    return absolute.generic_string();
}

std::filesystem::path pathFromStorage(std::string_view stored)
{
    std::filesystem::path path{std::string(stored)};
    if (path.is_absolute())
    {
        return path;
    }
    return findRepoRoot() / path;
}

bool chartbookPathsEqual(const std::filesystem::path& left, const std::filesystem::path& right)
{
    return normalizePath(left) == normalizePath(right);
}

std::string chartbookToJson(const CChartbookDocument& document)
{
    return documentToJson(document).dump(2);
}

ChartbookLoadResult chartbookFromJson(std::string_view text)
{
    ChartbookLoadResult result;
    json root;
    try
    {
        root = json::parse(text);
    }
    catch (const json::exception& ex)
    {
        result.error = ex.what();
        return result;
    }
    const json* object = nullptr;
    if (!readObject(root, "chartbook", object, result.error))
    {
        return result;
    }
    if (!readInt(*object, "format", result.document.format, result.error) ||
        result.document.format != kChartbookFormatVersion)
    {
        result.error = "unsupported chartbook format";
        result.document = {};
        return result;
    }
    if (!readString(*object, "name", result.document.name, result.error) ||
        !readInt(*object, "focused_pane", result.document.focused_pane, result.error) ||
        !readInt(*object, "next_pane_id", result.document.next_pane_id, result.error))
    {
        result.document = {};
        return result;
    }
    if (object->contains("focused_financials") &&
        !readInt(*object, "focused_financials", result.document.focused_financials, result.error))
    {
        result.document = {};
        return result;
    }
    bool next_financials_present = false;
    if (object->contains("next_financials_id"))
    {
        next_financials_present = true;
        if (!readInt(*object, "next_financials_id", result.document.next_financials_id, result.error))
        {
            result.document = {};
            return result;
        }
    }
    if (!object->contains("data") || !dataFromJson(object->at("data"), result.document.data, result.error))
    {
        result.document = {};
        return result;
    }
    if (object->contains("financials") &&
        !financialsFromJson(object->at("financials"), result.document, next_financials_present, result.error))
    {
        result.document = {};
        return result;
    }
    bool next_options_present = false;
    if (object->contains("next_options_id"))
    {
        next_options_present = true;
        if (!readInt(*object, "next_options_id", result.document.next_options_id, result.error))
        {
            result.document = {};
            return result;
        }
    }
    if (object->contains("focused_options") &&
        !readInt(*object, "focused_options", result.document.focused_options, result.error))
    {
        result.document = {};
        return result;
    }
    if (object->contains("options") &&
        !optionsFromJson(object->at("options"), result.document, next_options_present, result.error))
    {
        result.document = {};
        return result;
    }
    if (result.document.focused_options != 0)
    {
        const bool focused_ok = std::ranges::any_of(result.document.options, [&](const ChartbookOptions& panel) {
            return panel.id == result.document.focused_options;
        });
        if (!focused_ok)
        {
            result.error = "focused options is missing";
            result.document = {};
            return result;
        }
    }
    if (result.document.focused_financials != 0)
    {
        const bool focused_ok =
            std::ranges::any_of(result.document.financials, [&](const ChartbookFinancials& panel) {
                return panel.id == result.document.focused_financials;
            });
        if (!focused_ok)
        {
            result.error = "focused financials is missing";
            result.document = {};
            return result;
        }
    }
    if (!object->contains("panes") || !object->at("panes").is_array())
    {
        result.error = "panes is missing";
        result.document = {};
        return result;
    }
    for (const json& pane_value : object->at("panes"))
    {
        ChartbookPane pane;
        if (!paneFromJson(pane_value, pane, result.error))
        {
            result.document = {};
            return result;
        }
        const bool duplicate = std::ranges::any_of(result.document.panes, [&](const ChartbookPane& existing) {
            return existing.id == pane.id;
        });
        if (duplicate || result.document.next_pane_id <= pane.id)
        {
            result.error = "pane id is out of range";
            result.document = {};
            return result;
        }
        result.document.panes.push_back(std::move(pane));
    }
    if (result.document.focused_pane != 0)
    {
        const bool focused_ok =
            std::ranges::any_of(result.document.panes, [&](const ChartbookPane& pane) {
                return pane.id == result.document.focused_pane;
            });
        if (!focused_ok)
        {
            result.error = "focused pane is missing";
            result.document = {};
            return result;
        }
    }
    if (object->contains("floating"))
    {
        const json& floating = object->at("floating");
        if (!floating.is_array())
        {
            result.error = "floating is not an array";
            result.document = {};
            return result;
        }
        for (const json& item_value : floating)
        {
            const json* item = nullptr;
            if (!readObject(item_value, "floating window", item, result.error))
            {
                result.document = {};
                return result;
            }
            ChartbookFloating placed;
            double number = 0.0;
            if (!readString(*item, "window", placed.window, result.error) ||
                !readNumber(*item, "x", number, result.error))
            {
                result.document = {};
                return result;
            }
            placed.x = static_cast<float>(number);
            if (!readNumber(*item, "y", number, result.error))
            {
                result.document = {};
                return result;
            }
            placed.y = static_cast<float>(number);
            if (!readNumber(*item, "w", number, result.error))
            {
                result.document = {};
                return result;
            }
            placed.w = static_cast<float>(number);
            if (!readNumber(*item, "h", number, result.error))
            {
                result.document = {};
                return result;
            }
            placed.h = static_cast<float>(number);
            result.document.floating.push_back(std::move(placed));
        }
    }
    if (!object->contains("layout") ||
        !readLayout(object->at("layout"), result.document.layout, result.document.layout.root, 0, result.error))
    {
        if (result.error.empty())
        {
            result.error = "layout is missing";
        }
        result.document = {};
        return result;
    }
    if (!migrateLegacyFinancialsWindow(result.document, result.error) ||
        !documentWindowsOk(result.document, result.error))
    {
        result.document = {};
        return result;
    }
    result.ok = true;
    return result;
}

namespace {

[[nodiscard]] bool writeTextFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        return false;
    }
    out << text << '\n';
    out.flush();
    if (!out)
    {
        out.close();
        std::error_code error;
        std::filesystem::remove(path, error);
        return false;
    }
    out.close();
    if (!out)
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        return false;
    }
    return true;
}

// POSIX rename replaces a file. std::filesystem::rename does not have to, and fails on Windows when the destination exists.
[[nodiscard]] bool replaceFile(const std::filesystem::path& from, const std::filesystem::path& to, std::error_code& error)
{
#ifdef _WIN32
    error.clear();
    if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
    {
        error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return false;
    }
    return true;
#else
    std::filesystem::rename(from, to, error);
    return !error;
#endif
}

// Copies an existing destination to path.bak only after path itself has been replaced.
[[nodiscard]] std::string replaceKeepingBackup(const std::filesystem::path& temporary, const std::filesystem::path& destination,
                                              std::string_view backup_error, std::string_view replace_error)
{
    std::error_code error;
    const std::filesystem::path backup = destination.string() + ".bak";
    const std::filesystem::path pending = destination.string() + ".bak.tmp";
    std::filesystem::remove(pending, error);
    error.clear();
    const bool exists = std::filesystem::exists(destination, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        return std::string(replace_error);
    }
    if (exists)
    {
        std::filesystem::copy_file(destination, pending, std::filesystem::copy_options::overwrite_existing, error);
        if (error)
        {
            std::filesystem::remove(temporary, error);
            std::filesystem::remove(pending, error);
            return std::string(backup_error);
        }
    }
    if (!replaceFile(temporary, destination, error))
    {
        std::filesystem::remove(temporary, error);
        std::filesystem::remove(pending, error);
        return std::string(replace_error);
    }
    if (exists && !replaceFile(pending, backup, error))
    {
        std::filesystem::remove(pending, error);
    }
    return {};
}

}  // namespace

std::string saveChartbook(const std::filesystem::path& path, const CChartbookDocument& document)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        return "cannot create chartbook directory";
    }
    const std::filesystem::path temporary = path.string() + ".tmp";
    if (!writeTextFile(temporary, chartbookToJson(document)))
    {
        return "cannot write chartbook";
    }
    return replaceKeepingBackup(temporary, path, "cannot back up chartbook", "cannot replace chartbook");
}

ChartbookLoadResult loadChartbook(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        ChartbookLoadResult result;
        result.error = "cannot open " + path.string();
        return result;
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return chartbookFromJson(text);
}

StartupLoadResult loadStartupSettings(const std::filesystem::path& path)
{
    StartupLoadResult result;
    result.ok = true;
    if (!std::filesystem::exists(path))
    {
        return result;
    }
    std::ifstream in(path);
    if (!in)
    {
        result.ok = false;
        result.error = "cannot open startup settings";
        return result;
    }
    json root;
    try
    {
        root = json::parse(in);
    }
    catch (const json::exception& ex)
    {
        result.ok = false;
        result.error = ex.what();
        return result;
    }
    if (!root.is_object() || !root.contains("open_on_startup") || !root.at("open_on_startup").is_array())
    {
        result.ok = false;
        result.error = "startup settings are invalid";
        return result;
    }
    for (const json& item : root.at("open_on_startup"))
    {
        if (!item.is_string())
        {
            result.ok = false;
            result.error = "startup path is not a string";
            result.settings = {};
            return result;
        }
        result.settings.open_on_startup.push_back(item.get<std::string>());
    }
    return result;
}

std::string saveStartupSettings(const std::filesystem::path& path, const StartupSettings& settings)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        return "cannot create settings directory";
    }
    json paths = json::array();
    for (const std::string& item : settings.open_on_startup)
    {
        paths.push_back(item);
    }
    json root = json::object();
    root["version"] = 1;
    root["open_on_startup"] = std::move(paths);
    const std::filesystem::path temporary = path.string() + ".tmp";
    if (!writeTextFile(temporary, root.dump(2)))
    {
        return "cannot write startup settings";
    }
    return replaceKeepingBackup(temporary, path, "cannot back up startup settings", "cannot replace startup settings");
}

StartupOpenResult openStartupChartbooks(const StartupSettings& settings)
{
    StartupOpenResult result;
    for (const std::string& stored : settings.open_on_startup)
    {
        const std::filesystem::path path = pathFromStorage(stored);
        ChartbookLoadResult loaded = loadChartbook(path);
        if (!loaded.ok)
        {
            result.errors.push_back(stored + ": " + loaded.error);
            continue;
        }
        StartupOpenResult::Opened opened;
        opened.path = path;
        opened.document = std::move(loaded.document);
        result.books.push_back(std::move(opened));
    }
    return result;
}

}  // namespace terminal
