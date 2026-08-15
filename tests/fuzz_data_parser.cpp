#include "mustache_config.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "data.hpp"
#include "exception.hpp"

namespace {

struct ParsedUsage {
  std::size_t nodes = 0;
  std::size_t stringBytes = 0;
  std::size_t containerEntries = 0;
};

void addUsage(std::size_t& used, std::size_t amount, std::size_t maximum)
{
  if( used > maximum || amount > maximum - used ) {
    std::abort();
  }
  used += amount;
}

void validateData(const mustache::Data& data,
    const mustache::Data::ParseLimits& limits,
    std::size_t depth, ParsedUsage& usage)
{
  if( depth >= limits.maxNestingDepth ) {
    std::abort();
  }
  addUsage(usage.nodes, 1, limits.maxNodes);

  switch( data.type() ) {
    case mustache::Data::TypeNone:
    case mustache::Data::TypeBoolean:
    case mustache::Data::TypeInteger:
      static_cast<void>(data.toString());
      return;
    case mustache::Data::TypeDouble: {
      const std::string spelling = data.toString();
      addUsage(usage.stringBytes, spelling.size(), limits.maxStringBytes);
      return;
    }
    case mustache::Data::TypeString:
      addUsage(usage.stringBytes, data.stringValue().size(),
          limits.maxStringBytes);
      static_cast<void>(data.toString());
      return;
    case mustache::Data::TypeList:
      for( const mustache::Data& child : data.listItems() ) {
        addUsage(usage.containerEntries, 1,
            limits.maxContainerEntries);
        validateData(child, limits, depth + 1, usage);
      }
      return;
    case mustache::Data::TypeMap:
      for( const mustache::Data::Map::value_type& member :
          data.objectItems() ) {
        addUsage(usage.containerEntries, 1,
            limits.maxContainerEntries);
        addUsage(usage.stringBytes, member.first.size(),
            limits.maxStringBytes);
        validateData(member.second, limits, depth + 1, usage);
      }
      return;
    case mustache::Data::TypeArray:
      for( const mustache::Data& child : data.arrayItems() ) {
        addUsage(usage.containerEntries, 1,
            limits.maxContainerEntries);
        validateData(child, limits, depth + 1, usage);
      }
      return;
    case mustache::Data::TypeLambda:
      std::abort();
  }
  std::abort();
}

void validateParsedData(const mustache::Data& data,
    const mustache::Data::ParseLimits& limits)
{
  ParsedUsage usage;
  validateData(data, limits, 0, usage);

  mustache::Data copy(data);
  ParsedUsage copiedUsage;
  validateData(copy, limits, 0, copiedUsage);
  if( copiedUsage.nodes != usage.nodes ||
      copiedUsage.stringBytes != usage.stringBytes ||
      copiedUsage.containerEntries != usage.containerEntries ||
      copy.type() != data.type() ) {
    std::abort();
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, std::size_t size)
{
  mustache::Data::ParseLimits limits;
  limits.maxInputBytes = 2048;
  limits.maxNestingDepth = 32;
  limits.maxNodes = 512;
  limits.maxStringBytes = 4096;
  limits.maxContainerEntries = 512;

  const char * input = size == 0
      ? "" : reinterpret_cast<const char *>(data);
  const std::string_view view(input, size);

  std::optional<mustache::Data> json;
  try {
    json.emplace(mustache::Data::fromJSON(view, limits));
  } catch( const mustache::Exception& ) {
  }
  if( json.has_value() ) {
    validateParsedData(*json, limits);
  }

  std::optional<mustache::Data> yaml;
  try {
    yaml.emplace(mustache::Data::fromYAML(view, limits));
  } catch( const mustache::Exception& ) {
  }
  if( yaml.has_value() ) {
    validateParsedData(*yaml, limits);
  }

  return 0;
}
