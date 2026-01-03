#pragma once

#include "distribution.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <cstdint>

class RowRandomInt
{
public:
    /* The default multiplier value is a TPC-H constant */
    static constexpr std::int64_t kMultiplier = 16807;
    /* Modulus value is a TPC-H constant 2^31 - 1 */
    static constexpr std::int64_t kModulus = 2147483647;
    /* Default seed value as specified in CMU's benchbase */
    static constexpr std::int64_t kDefaultSeed = 19620718;

    RowRandomInt() = default;

    RowRandomInt(std::int64_t seed, std::int32_t seeds_per_row)
        : seed_(seed)
        , seeds_per_row_(seeds_per_row)
    {
    }

    static RowRandomInt NewWithDefaultSeedAndColumn(std::int32_t column_number,
                                                    std::int32_t seeds_per_row)
    {
        return NewWithColumnNumber(column_number, kDefaultSeed, seeds_per_row);
    }

    static RowRandomInt NewWithColumnNumber(std::int32_t column_number,
                                            std::int64_t seed,
                                            std::int32_t seeds_per_row)
    {
        const std::int64_t adjusted =
            seed + static_cast<std::int64_t>(column_number) * (kModulus / 799);
        return RowRandomInt(adjusted, seeds_per_row);
    }

    /*
     * Returns a value in [lo, hi]. The range computation deliberately keeps
     * 32-bit wrap semantics to mirror dbgen behavior when (hi - lo + 1) overflows
     *
     * Example:
     *   lo = 0, hi = INT32_MAX
     *   hi - lo + 1 == 2^31, which overflows signed 32-bit integer
     */
    std::int32_t NextInt(std::int32_t lo, std::int32_t hi)
    {
        NextRand();

        const auto range_u =
            static_cast<std::uint32_t>(hi - lo) + 1U;
        const auto range = static_cast<std::int32_t>(range_u);
        const double value =
            (static_cast<double>(seed_) / static_cast<double>(kModulus)) *
            static_cast<double>(range);
        const auto value_i = static_cast<std::int32_t>(value);
        const std::uint32_t result_u =
            static_cast<std::uint32_t>(lo) + static_cast<std::uint32_t>(value_i);
        return static_cast<std::int32_t>(result_u);
    }

    std::int64_t NextRand()
    {
        seed_ = (seed_ * kMultiplier) % kModulus;
        ++usage_;
        return seed_;
    }

    /* Skips the remaining draws for the current row */
    void RowFinished()
    {
        const std::int64_t remaining =
            static_cast<std::int64_t>(seeds_per_row_ - usage_);
        if (remaining > 0) {
            AdvanceSeed(remaining);
        }
        usage_ = 0;
    }

    /* Skips full rows deterministically (for partitioned generation) */
    void AdvanceRows(std::int64_t row_count)
    {
        if (usage_ != 0) {
            RowFinished();
        }
        const std::int64_t count =
            static_cast<std::int64_t>(seeds_per_row_) * row_count;
        if (count > 0) {
            AdvanceSeed(count);
        }
    }

private:
    /* X_{n + count} = (multiplier^{count} \cdot X_n) \mod modulus  */
    /* See: https://en.wikipedia.org/wiki/Exponentiation_by_squaring*/
    void AdvanceSeed(std::int64_t count)
    {
        std::int64_t multiplier = kMultiplier;
        std::int64_t remaining = count;

        while (remaining > 0) {
            if (remaining % 2 != 0) {
                seed_ = (multiplier * seed_) % kModulus;
            }
            remaining /= 2;
            multiplier = (multiplier * multiplier) % kModulus;
        }
    }

    std::int64_t seed_ = 0;
    std::int32_t usage_ = 0;
    std::int32_t seeds_per_row_ = 0;
};


class TextPool
{
public:
    /* By default limit text pool size to 300MB */
    static constexpr std::int32_t kDefaultTextPoolSize = 300 * 1024 * 1024; 
    static constexpr std::int32_t kMaxSentenceLength = 256;

    static const TextPool & Default()
    {
        static const TextPool pool(kDefaultTextPoolSize);
        return pool;
    }

    explicit TextPool(std::int32_t size)
    {
        RowRandomInt random(933588178, std::numeric_limits<std::int32_t>::max());
        text_.reserve(static_cast<std::size_t>(size) + kMaxSentenceLength);

        while (static_cast<std::int32_t>(text_.size()) < size) {
            GenerateSentence(text_, random);
        }
        text_.resize(static_cast<std::size_t>(size));
    }

    std::int32_t GetSize() const { return static_cast<std::int32_t>(text_.size()); }

    std::string_view GetView(std::int32_t begin, std::int32_t end) const
    {
        const auto start = static_cast<std::size_t>(begin);
        const auto count = static_cast<std::size_t>(end - begin);
        return std::string_view(text_.data() + start, count);
    }

private:
    static std::string_view PickToken(const DistributionView & dist, RowRandomInt & random)
    {
        const std::int32_t total = dist.total_weight;
        const std::int32_t sample = random.NextInt(0, total - 1);
        std::int32_t cumulative = 0;

        for (std::size_t i = 0; i < dist.size; ++i) {
            cumulative += dist.entries[i].weight;
            if (sample < cumulative) {
                return dist.entries[i].token;
            }
        }

        return dist.entries[dist.size - 1].token;
    }

    static void GenerateSentence(std::string & output, RowRandomInt & random)
    {
        const std::string_view syntax = PickToken(kGrammarDist, random);

        for (std::size_t i = 0; i < syntax.size(); i += 2) {
            const char token = syntax[i];
            switch (token) {
                case 'V':
                    GenerateVerbPhrase(output, random);
                    break;
                case 'N':
                    GenerateNounPhrase(output, random);
                    break;
                case 'P': {
                    const std::string_view preposition = PickToken(kPrepositionsDist, random);
                    output.append(preposition.data(), preposition.size());
                    output.append(" the ");
                    GenerateNounPhrase(output, random);
                    break;
                }
                case 'T': {
                    if (!output.empty() && output.back() == ' ') {
                        output.pop_back();
                    }
                    const std::string_view terminator = PickToken(kTerminatorsDist, random);
                    output.append(terminator.data(), terminator.size());
                    break;
                }
                default:
                    break;
            }

            if (!output.empty() && output.back() != ' ') {
                output.push_back(' ');
            }
        }
    }

    static void GenerateVerbPhrase(std::string & output, RowRandomInt & random)
    {
        const std::string_view syntax = PickToken(kVerbPhraseDist, random);

        for (std::size_t i = 0; i < syntax.size(); i += 2) {
            const char token = syntax[i];
            const DistributionView * source = nullptr;

            switch (token) {
                case 'D':
                    source = &kAdverbsDist;
                    break;
                case 'V':
                    source = &kVerbsDist;
                    break;
                case 'X':
                    source = &kAuxiliariesDist;
                    break;
                default:
                    break;
            }

            if (source == nullptr) {
                continue;
            }

            const std::string_view word = PickToken(*source, random);
            output.append(word.data(), word.size());
            output.push_back(' ');
        }
    }

    static void GenerateNounPhrase(std::string & output, RowRandomInt & random)
    {
        const std::string_view syntax = PickToken(kNounPhraseDist, random);

        for (char token : syntax) {
            const DistributionView * source = nullptr;

            switch (token) {
                case 'A':
                    source = &kArticlesDist;
                    break;
                case 'J':
                    source = &kAdjectivesDist;
                    break;
                case 'D':
                    source = &kAdverbsDist;
                    break;
                case 'N':
                    source = &kNounsDist;
                    break;
                case ',':
                    if (!output.empty() && output.back() == ' ') {
                        output.pop_back();
                    }
                    output.append(", ");
                    continue;
                case ' ':
                    continue;
                default:
                    continue;
            }

            const std::string_view word = PickToken(*source, random);
            output.append(word.data(), word.size());
            output.push_back(' ');
        }
    }

    std::string text_;
};

class RandomText
{
public:
    static constexpr double kLowLengthMultiplier = 0.4;
    static constexpr double kHighLengthMultiplier = 1.6;

    RandomText() = default;

    RandomText(std::int64_t seed,
               const TextPool & text_pool,
               double average_text_length,
               std::int32_t expected_row_count = 1)
        : inner_(seed, expected_row_count * 2)
        , text_pool_(&text_pool)
        , min_length_(static_cast<std::int32_t>(average_text_length * kLowLengthMultiplier))
        , max_length_(static_cast<std::int32_t>(average_text_length * kHighLengthMultiplier))
    {
    }

    std::string_view NextValue()
    {
        if (text_pool_ == nullptr || text_pool_->GetSize() <= max_length_) {
            return {};
        }

        const std::int32_t offset =
            inner_.NextInt(0, text_pool_->GetSize() - max_length_);
        const auto length = inner_.NextInt(min_length_, max_length_);
        return text_pool_->GetView(offset, offset + length);
    }

    void AdvanceRows(std::int64_t row_count) { inner_.AdvanceRows(row_count); }

    void RowFinished() { inner_.RowFinished(); }

private:
    RowRandomInt inner_;
    const TextPool * text_pool_ = nullptr;
    std::int32_t min_length_ = 0;
    std::int32_t max_length_ = 0;
};
