#include "mustache_config.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>

#include "exception.hpp"
#include "node.hpp"
#include "tokenizer.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, std::size_t size)
{
  mustache::Tokenizer::Limits limits;
  limits.maxInputBytes = 4096;
  limits.maxNestingDepth = 64;
  limits.maxNodes = 1024;
  limits.maxTagBytes = 512;
  limits.maxDelimiterBytes = 64;

  mustache::Node root;
  try {
    mustache::Tokenizer tokenizer;
    const char * input = size == 0
        ? "" : reinterpret_cast<const char *>(data);
    tokenizer.tokenize(std::string_view(
        input, size), &root, limits);
  } catch( const mustache::Exception& ) {
    return 0;
  }

  if( root.type != mustache::Node::TypeRoot ) {
    std::abort();
  }

  // The parser counts sections and excludes the root; serialization counts
  // every node, including the root and an innermost closing node. Dotted-name
  // budgets likewise derive from the parser's byte and node budgets.
  mustache::Node::SerializationLimits serializationLimits;
  serializationLimits.maxNestingDepth = limits.maxNestingDepth + 2;
  serializationLimits.maxNodes = limits.maxNodes + 1;
  serializationLimits.maxDataPartsPerNode = limits.maxTagBytes + 1;
  serializationLimits.maxDataParts =
      limits.maxInputBytes + limits.maxNodes;

  std::unique_ptr<std::vector<uint8_t> > serial(
      root.serialize(serializationLimits));
  std::size_t position = 0;
  std::unique_ptr<mustache::Node> decoded(
      mustache::Node::unserialize(
          *serial, 0, &position, serializationLimits));
  if( position != serial->size() ||
      decoded->type != mustache::Node::TypeRoot ) {
    std::abort();
  }

  return 0;
}
