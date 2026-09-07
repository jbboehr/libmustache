#ifndef MUSTACHE_PARSE_BUDGET_HPP
#define MUSTACHE_PARSE_BUDGET_HPP

#include "data.hpp"

#include <cstddef>
#include <string>

namespace mustache {

// Keep the shared implementation local to each parser translation unit.
namespace {

const std::size_t parseNestingCeiling = 256;

class ParseBudget {
  public:
    ParseBudget(const Data::ParseLimits& limits, const char * format) :
        limits_(limits),
        format_(format),
        nodes_(0),
        stringBytes_(0),
        containerEntries_(0)
    {}

    void addNode(std::size_t depth)
    {
      if (depth >= limits_.maxNestingDepth || depth >= parseNestingCeiling) {
        fail(" nesting limit exceeded");
      }
      consume(nodes_, 1, limits_.maxNodes, " node count limit exceeded");
    }

    void addString(std::size_t bytes)
    {
      consume(stringBytes_, bytes, limits_.maxStringBytes, " string byte limit exceeded");
    }

    void addContainerEntries(std::size_t entries)
    {
      consume(containerEntries_, entries, limits_.maxContainerEntries, " container entry limit exceeded");
    }

  private:
    const Data::ParseLimits& limits_;
    const char * format_;
    std::size_t nodes_;
    std::size_t stringBytes_;
    std::size_t containerEntries_;

    [[noreturn]] void fail(const char * description) const
    {
      throw Exception(std::string(format_) + description);
    }

    void consume(std::size_t& used, std::size_t amount, std::size_t maximum, const char * description)
    {
      if (used > maximum || amount > maximum - used) {
        fail(description);
      }
      used += amount;
    }
};

} // namespace
} // namespace mustache

#endif
