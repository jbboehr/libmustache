#include "mustache_config.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>

#include "data.hpp"
#include "exception.hpp"

namespace {

void validateData(const mustache::Data& data,
    const mustache::Data::ParseLimits& limits,
    std::size_t depth, std::size_t& nodes)
{
  if( depth >= limits.maxNestingDepth || nodes >= limits.maxNodes ) {
    std::abort();
  }
  ++nodes;

  switch( data.type() ) {
    case mustache::Data::TypeNone:
    case mustache::Data::TypeBoolean:
    case mustache::Data::TypeInteger:
    case mustache::Data::TypeDouble:
    case mustache::Data::TypeString:
      static_cast<void>(data.toString());
      return;
    case mustache::Data::TypeList:
      for( const mustache::Data& child : data.listItems() ) {
        validateData(child, limits, depth + 1, nodes);
      }
      return;
    case mustache::Data::TypeMap:
      for( const mustache::Data::Map::value_type& member :
          data.objectItems() ) {
        validateData(member.second, limits, depth + 1, nodes);
      }
      return;
    case mustache::Data::TypeArray:
      for( const mustache::Data& child : data.arrayItems() ) {
        validateData(child, limits, depth + 1, nodes);
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
  std::size_t nodes = 0;
  validateData(data, limits, 0, nodes);

  mustache::Data copy(data);
  std::size_t copiedNodes = 0;
  validateData(copy, limits, 0, copiedNodes);
  if( copiedNodes != nodes || copy.type() != data.type() ) {
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
