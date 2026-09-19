//
// Created by cynickal on 9/18/26.
//

#include "CBarLoader.h"

#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

std::string Trim(std::string_view value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos)
    {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string{value.substr(begin, end - begin + 1)};
}

std::string NormalizeField(std::string_view raw)
{
    auto value = Trim(raw);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    {
        value = Trim(value.substr(1, value.size() - 2));
    }
    return value;
}

std::vector<std::string> SplitCsvLine(std::string_view line)
{
    std::vector<std::string> fields{};
    std::size_t start = 0;
    while (start <= line.size())
    {
        const auto comma = line.find(',', start);
        const auto end = comma == std::string_view::npos ? line.size() : comma;
        fields.push_back(NormalizeField(line.substr(start, end - start)));
        if (comma == std::string_view::npos)
        {
            break;
        }
        start = end + 1;
    }
    return fields;
}

void StripUtf8Bom(std::string& field)
{
    constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF"};
    if (field.starts_with(kUtf8Bom))
    {
        field.erase(0, kUtf8Bom.size());
    }
}

bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        const auto left = static_cast<unsigned char>(lhs[index]);
        const auto right = static_cast<unsigned char>(rhs[index]);
        if (std::tolower(left) != std::tolower(right))
        {
            return false;
        }
    }

    return true;
}

std::size_t FindRequiredColumn(const std::vector<std::string>& headerFields, const std::string& name)
{
    for (std::size_t index = 0; index < headerFields.size(); ++index)
    {
        if (EqualsIgnoreCase(headerFields[index], name))
        {
            return index;
        }
    }

    throw std::runtime_error("CSV is missing required column '" + name + "'");
}

float ParseFloat(std::string_view text, std::string_view fieldName)
{
    if (text.empty())
    {
        throw std::runtime_error("Empty value for column '" + std::string{fieldName} + "'");
    }

    float value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        throw std::runtime_error("Failed to parse " + std::string{fieldName} + " value '" +
                                 std::string{text} + "'");
    }
    return value;
}

std::optional<int> ParseFixedInt(std::string_view text, std::size_t offset, std::size_t width)
{
    if (offset + width > text.size())
    {
        return std::nullopt;
    }

    int value{};
    const char* const begin = text.data() + offset;
    const char* const end = begin + width;
    const std::from_chars_result result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        return std::nullopt;
    }

    return value;
}

std::chrono::system_clock::time_point ParseDate(std::string_view text)
{
    const auto year = ParseFixedInt(text, 0, 4);
    const auto month = ParseFixedInt(text, 5, 2);
    const auto day = ParseFixedInt(text, 8, 2);
    const bool hasDate = year.has_value() && month.has_value() && day.has_value() && text.size() >= 10 &&
                         text[4] == '-' && text[7] == '-';

    int hour = 0;
    int minute = 0;
    int second = 0;
    bool parsed = hasDate && text.size() == 10;

    if (hasDate && text.size() == 19 && text[10] == ' ' && text[13] == ':' && text[16] == ':')
    {
        const auto parsedHour = ParseFixedInt(text, 11, 2);
        const auto parsedMinute = ParseFixedInt(text, 14, 2);
        const auto parsedSecond = ParseFixedInt(text, 17, 2);
        if (parsedHour.has_value() && parsedMinute.has_value() && parsedSecond.has_value() &&
            *parsedHour >= 0 && *parsedHour <= 23 && *parsedMinute >= 0 && *parsedMinute <= 59 &&
            *parsedSecond >= 0 && *parsedSecond <= 59)
        {
            hour = *parsedHour;
            minute = *parsedMinute;
            second = *parsedSecond;
            parsed = true;
        }
    }

    if (!parsed)
    {
        throw std::runtime_error("Failed to parse Date '" + std::string{text} +
                                 "'; expected YYYY-MM-DD or YYYY-MM-DD HH:MM:SS");
    }

    const std::chrono::year_month_day ymd{std::chrono::year{*year},
                                          std::chrono::month{static_cast<unsigned>(*month)},
                                          std::chrono::day{static_cast<unsigned>(*day)}};
    if (!ymd.ok())
    {
        throw std::runtime_error("Failed to parse Date '" + std::string{text} + "'; invalid calendar date");
    }

    const auto absTime = std::chrono::sys_days{ymd} + std::chrono::hours{hour} +
                         std::chrono::minutes{minute} + std::chrono::seconds{second};
    return std::chrono::system_clock::time_point{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(absTime.time_since_epoch())};
}

const std::string& RequireField(const std::vector<std::string>& fields,
                                std::size_t index,
                                const std::string& name,
                                std::size_t lineNumber)
{
    if (index >= fields.size())
    {
        throw std::runtime_error("CSV row " + std::to_string(lineNumber) + " is missing column '" +
                                 name + "'");
    }
    return fields[index];
}

}  // namespace

std::vector<CBarData> CBarLoader::LoadFromFile(const std::filesystem::path& filePath)
{
    return LoadFromFile(filePath, HeaderNames{});
}

std::vector<CBarData> CBarLoader::LoadFromFile(const std::filesystem::path& filePath,
                                               const HeaderNames& headers)
{
    std::ifstream input{filePath};
    if (!input)
    {
        throw std::runtime_error("Failed to open CSV file: " + filePath.string());
    }

    std::string line{};
    std::size_t lineNumber = 0;
    std::vector<std::string> headerFields{};

    while (std::getline(input, line))
    {
        ++lineNumber;
        if (Trim(line).empty())
        {
            continue;
        }

        headerFields = SplitCsvLine(line);
        if (!headerFields.empty())
        {
            StripUtf8Bom(headerFields.front());
        }
        break;
    }

    if (headerFields.empty())
    {
        throw std::runtime_error("CSV file has no header row: " + filePath.string());
    }

    const auto openIndex = FindRequiredColumn(headerFields, headers.Open);
    const auto highIndex = FindRequiredColumn(headerFields, headers.High);
    const auto lowIndex = FindRequiredColumn(headerFields, headers.Low);
    const auto closeIndex = FindRequiredColumn(headerFields, headers.Close);
    const auto volumeIndex = FindRequiredColumn(headerFields, headers.Volume);
    const auto dateIndex = FindRequiredColumn(headerFields, headers.Date);

    std::vector<CBarData> loaded{};
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (Trim(line).empty())
        {
            continue;
        }

        const auto fields = SplitCsvLine(line);

        CBarData bar{};
        bar.m_Open = ParseFloat(RequireField(fields, openIndex, headers.Open, lineNumber), headers.Open);
        bar.m_High = ParseFloat(RequireField(fields, highIndex, headers.High, lineNumber), headers.High);
        bar.m_Low = ParseFloat(RequireField(fields, lowIndex, headers.Low, lineNumber), headers.Low);
        bar.m_Close = ParseFloat(RequireField(fields, closeIndex, headers.Close, lineNumber), headers.Close);
        bar.m_Volume = ParseFloat(RequireField(fields, volumeIndex, headers.Volume, lineNumber),
                                  headers.Volume);
        bar.m_StartTime = ParseDate(RequireField(fields, dateIndex, headers.Date, lineNumber));
        loaded.push_back(bar);
    }

    return loaded;
}
