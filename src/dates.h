#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

/*
 * TPCH stores dates as five digit indexes YYDDD rather than YYYY-MM-DD strings
 *  - YY is the year relative to 1900, so 92 -> 1992
 *  - DDD is the day-of-year, so 001 = Jan 1, 365/366 = Dec 31
 *
 * The range of valid values extends for kTotalDateRange / 2557 days
 *
 * Formatting happens at the last possible moment:
 *   - Format() converts the index back into YYYY-MM-DD via ToYmd/ToJulian
 *   - ToUnixEpoch() shifts the TPCH index into a Unix-day count so Arrow's
 *     date32 columns store the correct timestamp
 *   - IsInPast() compares the generated index against the current Julian marker
 *     to decide whether date is already in the past
 *
 * If you, like me, didn’t know before what a Julian Day Number is: it's a continuous
 * day count from a fixed origin (day 0 = noon UTC, 1 Jan 4713 BCE) with no months
 * or leap-year rules. We use this linear day index to add offsets and compare
 * dates without touching calendar logic, converting to y/m/d only at output time
 *
 * TPCHDate therefore acts as the only place that understands the YYDDD encoding,
 * while the rest of the generators can work purely with integers
 */
class TPCHDate
{
public:
    /* Just 1992-01-01 encoded in YYDDD format */
    static constexpr std::int32_t kMinGenerateDate = 92001;
    /* Span of days we are allowed to generate */
    static constexpr std::int32_t kTotalDateRange = 2557;
    /* Julian marker representing today for TPCH */
    static constexpr std::int32_t kCurrentDate = 95168; 
    /* Days between 1992-01-01 and 1970-01-01 */
    static constexpr std::int32_t kUnixEpochOffset = 8035;

    static std::int32_t ToUnixEpoch(std::int32_t generated_date)
    {
        return (generated_date - kMinGenerateDate) + kUnixEpochOffset;
    }

    static bool IsInPast(std::int32_t generated_date)
    {
        return ToJulian(generated_date) <= kCurrentDate;
    }

    static std::string Format(std::int32_t generated_date)
    {
        std::int32_t y = 0;
        std::int32_t m = 0;
        std::int32_t d = 0;
        ToYmd(generated_date, y, m, d);

        char buffer[16];
        const int n = std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 1900 + y, m, d);
        return std::string(buffer, buffer + (n > 0 ? n : 0));
    }

private:
    static constexpr std::array<std::int32_t, 13> kMonthYearDayStart = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

    static void ToYmd(std::int32_t generated_date,
                      std::int32_t & y,
                      std::int32_t & m,
                      std::int32_t & d)
    {
        const std::int32_t julian = ToJulian(generated_date);
        const std::int32_t day = julian % 1000;
        y = julian / 1000;

        m = 0;
        while (day > kMonthYearDayStart[static_cast<std::size_t>(m)] +
                LeapYearAdjustment(y, m)) {
            ++m;
        }

        d = day - kMonthYearDayStart[static_cast<std::size_t>(m - 1)] -
            (IsLeapYear(y) && m > 2 ? 1 : 0);
    }

    static std::int32_t ToJulian(std::int32_t date)
    {
        std::int32_t offset = date - kMinGenerateDate;
        std::int32_t result = kMinGenerateDate;

        while (true) {
            const std::int32_t year = result / 1000;
            const std::int32_t year_end = year * 1000 + 365 + (IsLeapYear(year) ? 1 : 0);

            if (result + offset <= year_end) {
                break;
            }

            offset -= year_end - result + 1;
            result += 1000;
        }

        return result + offset;
    }

    static constexpr bool IsLeapYear(std::int32_t year)
    {
        return year % 4 == 0 && year % 100 != 0;
    }

    static constexpr std::int32_t LeapYearAdjustment(std::int32_t year, std::int32_t month)
    {
        return (IsLeapYear(year) && month >= 2) ? 1 : 0;
    }
};
