// Unit tests for parseDurationSeconds. Built only with -DENABLE_TESTS=ON.

#include "utils/duration-parse.hpp"

#include <cstdio>
#include <optional>

namespace
{
  struct Case
  {
    const char *text;
    std::optional<int> expected;
  };

  const Case CASES[] = {
      // Seconds
      {"120", 120},
      {"  120  ", 120},
      {"120.6", 121},
      {"120.4", 120},
      {"0.5", 1},
      {"0.4", 0},
      {"0", 0},
      {"999999", 999999},

      // Clock
      {"5:22", 322},
      {"5:22.6", 323},
      {"5:22.4", 322},
      {"5:22.5", 323},
      {"0:00.5", 1},
      {"90:00", 5400},
      {"1:05:00", 3900},
      {"6:00:00", 21600},
      {"6:00:00.4", 21600},
      {"6:00:00.5", 21601},
      {"0:59", 59},

      // Units
      {"90s", 90},
      {"5m", 300},
      {"1.5h", 5400},
      {"4.35m", 261},
      {"0.0002h", 1},
      {"1h30m", 5400},
      {"5m 22s", 322},
      {"5m22s", 322},
      {"2 min", 120},
      {"2 Minutes", 120},
      {"1 hour 5 minutes 3 seconds", 3903},
      {"1HR 2MINS", 3720},
      {"10 secs", 10},
      {"1h30m15.5s", 5416},

      // Rejected
      {"", std::nullopt},
      {"   ", std::nullopt},
      {"-5", std::nullopt},
      {"+5", std::nullopt},
      {"5,5", std::nullopt},
      {".5", std::nullopt},
      {"5.", std::nullopt},
      {"abc", std::nullopt},
      {"5x", std::nullopt},
      {"5:2", std::nullopt},
      {"5:60", std::nullopt},
      {"5:022", std::nullopt},
      {"5.5:00", std::nullopt},
      {"1:2:3:4", std::nullopt},
      {":30", std::nullopt},
      {"5:", std::nullopt},
      {"1:30m", std::nullopt},
      {"30m1h", std::nullopt},
      {"5m5m", std::nullopt},
      {"5m 22", std::nullopt},
      {"PT5M", std::nullopt},
      {"1234567", std::nullopt},
      {"999999h", std::nullopt},
  };
}

int main()
{
  int failures = 0;
  for (const Case &c : CASES)
  {
    const std::optional<int> actual = ReplayBufferPro::parseDurationSeconds(c.text);
    if (actual == c.expected)
      continue;
    ++failures;
    std::printf("FAIL \"%s\": expected %s%d, got %s%d\n", c.text,
                c.expected ? "" : "nullopt/", c.expected.value_or(0),
                actual ? "" : "nullopt/", actual.value_or(0));
  }
  std::printf("%d of %zu cases passed\n", static_cast<int>(sizeof(CASES) / sizeof(CASES[0])) - failures,
              sizeof(CASES) / sizeof(CASES[0]));
  return failures == 0 ? 0 : 1;
}
