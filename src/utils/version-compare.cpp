#include "utils/version-compare.hpp"

#include <algorithm>
#include <vector>

namespace ReplayBufferPro
{
  namespace
  {
    std::optional<std::vector<long>> parseVersion(std::string_view text)
    {
      if (!text.empty() && (text.front() == 'v' || text.front() == 'V'))
        text.remove_prefix(1);

      std::vector<long> parts;
      long part = 0;
      size_t digits = 0;
      for (char c : text)
      {
        if (c >= '0' && c <= '9')
        {
          if (++digits > 9)
            return std::nullopt;
          part = part * 10 + (c - '0');
        }
        else if (c == '.' && digits > 0)
        {
          parts.push_back(part);
          part = 0;
          digits = 0;
        }
        else
        {
          return std::nullopt;
        }
      }
      if (digits == 0)
        return std::nullopt;
      parts.push_back(part);
      return parts;
    }
  }

  std::optional<int> compareVersions(std::string_view a, std::string_view b)
  {
    std::optional<std::vector<long>> left = parseVersion(a);
    std::optional<std::vector<long>> right = parseVersion(b);
    if (!left || !right)
      return std::nullopt;

    size_t count = std::max(left->size(), right->size());
    for (size_t i = 0; i < count; i++)
    {
      long l = i < left->size() ? (*left)[i] : 0;
      long r = i < right->size() ? (*right)[i] : 0;
      if (l != r)
        return l < r ? -1 : 1;
    }
    return 0;
  }
}
