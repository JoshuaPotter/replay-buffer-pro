/**
 * @file duration-parse.cpp
 * @brief Parses user-written durations into whole seconds
 */

#include "utils/duration-parse.hpp"

#include <climits>
#include <cstdint>
#include <string>
#include <vector>

namespace ReplayBufferPro
{
  namespace
  {
    // Exact integer nanoseconds, so "4.35m" is 261 seconds rather than a float
    // that rounds the wrong way.
    constexpr int64_t NS_PER_SECOND = 1'000'000'000;
    constexpr int MAX_WHOLE_DIGITS = 6;     // Keeps every term within int64
    constexpr int MAX_FRACTION_DIGITS = 9;  // Extra digits cannot change the rounding
    constexpr int64_t MAX_TOTAL_NS = static_cast<int64_t>(INT_MAX) * NS_PER_SECOND;

    struct Number
    {
      int64_t whole = 0;
      int wholeDigits = 0;
      int64_t fraction = 0;
      int fractionDigits = 0;
    };

    struct Unit
    {
      std::string_view name;
      int64_t seconds;
    };

    constexpr Unit UNITS[] = {
        {"h", 3600}, {"hr", 3600}, {"hrs", 3600}, {"hour", 3600}, {"hours", 3600},
        {"m", 60}, {"min", 60}, {"mins", 60}, {"minute", 60}, {"minutes", 60},
        {"s", 1}, {"sec", 1}, {"secs", 1}, {"second", 1}, {"seconds", 1},
    };

    bool isDigit(char c) { return c >= '0' && c <= '9'; }
    bool isLetter(char c) { return c >= 'a' && c <= 'z'; }
    bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

    void skipSpaces(std::string_view text, size_t &pos)
    {
      while (pos < text.size() && isSpace(text[pos]))
        ++pos;
    }

    // digits ('.' digits)?
    bool readNumber(std::string_view text, size_t &pos, bool allowFraction, Number &number)
    {
      while (pos < text.size() && isDigit(text[pos]))
      {
        if (++number.wholeDigits > MAX_WHOLE_DIGITS)
          return false;
        number.whole = number.whole * 10 + (text[pos++] - '0');
      }
      if (number.wholeDigits == 0)
        return false;
      if (!allowFraction || pos >= text.size() || text[pos] != '.')
        return true;

      const size_t fractionStart = ++pos;
      for (; pos < text.size() && isDigit(text[pos]); ++pos)
      {
        if (number.fractionDigits < MAX_FRACTION_DIGITS)
        {
          number.fraction = number.fraction * 10 + (text[pos] - '0');
          ++number.fractionDigits;
        }
      }
      return pos > fractionStart;
    }

    bool add(int64_t &totalNs, const Number &number, int64_t unitSeconds)
    {
      int64_t fractionNs = number.fraction;
      for (int digits = number.fractionDigits; digits < MAX_FRACTION_DIGITS; ++digits)
        fractionNs *= 10;
      totalNs += number.whole * unitSeconds * NS_PER_SECOND + fractionNs * unitSeconds;
      return totalNs <= MAX_TOTAL_NS;
    }

    // m:ss or h:mm:ss. The leading part is unbounded ("90:00"); later parts are
    // two digits up to 59, and only the last may have a fraction.
    bool parseClock(std::string_view text, int64_t &totalNs)
    {
      std::vector<std::string_view> parts;
      for (size_t start = 0;;)
      {
        const size_t colon = text.find(':', start);
        parts.push_back(text.substr(start, colon - start));
        if (colon == std::string_view::npos)
          break;
        start = colon + 1;
      }
      if (parts.size() != 2 && parts.size() != 3)
        return false;

      int64_t unitSeconds = parts.size() == 3 ? 3600 : 60;
      for (size_t i = 0; i < parts.size(); ++i, unitSeconds /= 60)
      {
        Number number;
        size_t pos = 0;
        if (!readNumber(parts[i], pos, i + 1 == parts.size(), number) || pos != parts[i].size())
          return false;
        if (i > 0 && (number.wholeDigits != 2 || number.whole > 59))
          return false;
        if (!add(totalNs, number, unitSeconds))
          return false;
      }
      return true;
    }

    // A bare number of seconds, or number-unit terms with each unit smaller than
    // the one before it ("1h30m", "5m 22s").
    bool parseUnits(std::string_view text, int64_t &totalNs)
    {
      int64_t previousUnit = INT64_MAX;
      size_t pos = 0;
      do
      {
        Number number;
        if (!readNumber(text, pos, true, number))
          return false;
        skipSpaces(text, pos);
        if (pos == text.size() && previousUnit == INT64_MAX)
          return add(totalNs, number, 1);

        const size_t wordStart = pos;
        while (pos < text.size() && isLetter(text[pos]))
          ++pos;
        const std::string_view word = text.substr(wordStart, pos - wordStart);
        int64_t unitSeconds = 0;
        for (const Unit &unit : UNITS)
          if (unit.name == word)
            unitSeconds = unit.seconds;
        if (unitSeconds == 0 || unitSeconds >= previousUnit || !add(totalNs, number, unitSeconds))
          return false;
        previousUnit = unitSeconds;
        skipSpaces(text, pos);
      } while (pos < text.size());
      return true;
    }
  } // namespace

  std::optional<int> parseDurationSeconds(std::string_view text)
  {
    size_t start = 0;
    size_t end = text.size();
    while (start < end && isSpace(text[start]))
      ++start;
    while (end > start && isSpace(text[end - 1]))
      --end;

    std::string lowered(text.substr(start, end - start));
    for (char &c : lowered)
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c - 'A' + 'a');

    int64_t totalNs = 0;
    const bool parsed = lowered.find(':') != std::string::npos ? parseClock(lowered, totalNs)
                                                                : parseUnits(lowered, totalNs);
    if (!parsed)
      return std::nullopt;
    return static_cast<int>((totalNs + NS_PER_SECOND / 2) / NS_PER_SECOND);
  }
}
