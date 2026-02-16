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

    static std::int32_t ToUnixEpoch(std::int32_t generated_date);

    static bool IsInPast(std::int32_t generated_date);

    static std::string Format(std::int32_t generated_date);

private:
    static constexpr std::array<std::int32_t, 13> kMonthYearDayStart = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

    static void ToYmd(std::int32_t generated_date, std::int32_t & y, std::int32_t & m, std::int32_t & d);

    static std::int32_t ToJulian(std::int32_t date);

    static bool IsLeapYear(std::int32_t year);

    static std::int32_t LeapYearAdjustment(std::int32_t year, std::int32_t month);
};
