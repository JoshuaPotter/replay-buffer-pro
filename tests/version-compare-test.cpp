// Unit tests for compareVersions. Built only with -DENABLE_TESTS=ON.

#include "utils/version-compare.hpp"

#include <cstdio>
#include <optional>

namespace
{
  struct Case
  {
    const char *a;
    const char *b;
    std::optional<int> expected;
  };

  const Case CASES[] = {
      // Same release
      {"1.9.0", "1.9.0", 0},
      {"v1.9.0", "1.9.0", 0},
      {"V1.9.0", "v1.9.0", 0},
      {"1.9", "1.9.0", 0},
      {"1.9.0.0", "1.9", 0},

      // Ordering is numeric per part, not textual
      {"1.10.0", "1.9.0", 1},
      {"1.9.0", "1.10.0", -1},
      {"2.0.0", "1.99.99", 1},
      {"1.9.1", "1.9.0", 1},
      {"1.9.0", "1.9.1", -1},
      {"1.9.0.1", "1.9", 1},

      // Not versions
      {"", "1.9.0", std::nullopt},
      {"1.9.0", "", std::nullopt},
      {"v", "1.9.0", std::nullopt},
      {"1..0", "1.9.0", std::nullopt},
      {".1.0", "1.9.0", std::nullopt},
      {"1.0.", "1.9.0", std::nullopt},
      {"1.10.0-beta.1", "1.9.0", std::nullopt},
      {" 1.9.0", "1.9.0", std::nullopt},
      {"latest", "1.9.0", std::nullopt},
      {"1.9999999999.0", "1.9.0", std::nullopt},
  };
}

int main()
{
  int failures = 0;
  for (const Case &c : CASES)
  {
    const std::optional<int> actual = ReplayBufferPro::compareVersions(c.a, c.b);
    if (actual == c.expected)
      continue;
    ++failures;
    std::printf("FAIL \"%s\" vs \"%s\": expected %s%d, got %s%d\n", c.a, c.b,
                c.expected ? "" : "nullopt/", c.expected.value_or(0),
                actual ? "" : "nullopt/", actual.value_or(0));
  }
  std::printf("%d of %zu cases passed\n", static_cast<int>(sizeof(CASES) / sizeof(CASES[0])) - failures,
              sizeof(CASES) / sizeof(CASES[0]));
  return failures == 0 ? 0 : 1;
}
