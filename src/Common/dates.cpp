#include "Common/dates.h"

std::int32_t TPCHDate::ToUnixEpoch(std::int32_t generated_date)
{
    return (generated_date - kMinGenerateDate) + kUnixEpochOffset;
}

bool TPCHDate::IsInPast(std::int32_t generated_date)
{
    return ToJulian(generated_date) <= kCurrentDate;
}

std::string TPCHDate::Format(std::int32_t generated_date)
{
    std::int32_t y = 0;
    std::int32_t m = 0;
    std::int32_t d = 0;
    ToYmd(generated_date, y, m, d);

    char buffer[16];
    const int n = std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 1900 + y, m, d);
    return std::string(buffer, buffer + (n > 0 ? n : 0));
}

void TPCHDate::ToYmd(std::int32_t generated_date, std::int32_t & y, std::int32_t & m, std::int32_t & d)
{
    const std::int32_t julian = ToJulian(generated_date);
    const std::int32_t day = julian % 1000;
    y = julian / 1000;

    m = 0;
    while (day > kMonthYearDayStart[static_cast<std::size_t>(m)] + LeapYearAdjustment(y, m))
    {
        ++m;
    }

    d = day - kMonthYearDayStart[static_cast<std::size_t>(m - 1)] - (IsLeapYear(y) && m > 2 ? 1 : 0);
}

std::int32_t TPCHDate::ToJulian(std::int32_t date)
{
    std::int32_t offset = date - kMinGenerateDate;
    std::int32_t result = kMinGenerateDate;

    while (true)
    {
        const std::int32_t year = result / 1000;
        const std::int32_t year_end = year * 1000 + 365 + (IsLeapYear(year) ? 1 : 0);

        if (result + offset <= year_end)
        {
            break;
        }

        offset -= year_end - result + 1;
        result += 1000;
    }

    return result + offset;
}

bool TPCHDate::IsLeapYear(std::int32_t year)
{
    return year % 4 == 0 && year % 100 != 0;
}

std::int32_t TPCHDate::LeapYearAdjustment(std::int32_t year, std::int32_t month)
{
    return (IsLeapYear(year) && month >= 2) ? 1 : 0;
}
