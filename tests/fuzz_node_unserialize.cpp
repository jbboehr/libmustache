#include "mustache_config.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

#include "mustache.hpp"

namespace {

unsigned int hexDigit(uint8_t value)
{
  if( value >= '0' && value <= '9' ) {
    return static_cast<unsigned int>(value - '0');
  }
  if( value >= 'a' && value <= 'f' ) {
    return static_cast<unsigned int>(value - 'a' + 10);
  }
  if( value >= 'A' && value <= 'F' ) {
    return static_cast<unsigned int>(value - 'A' + 10);
  }
  return 16;
}

bool decodeHexSeed(
    const uint8_t * data, std::size_t size, std::vector<uint8_t>& output)
{
  if( size < 4 || data[0] != 'h' || data[1] != 'e' || data[2] != 'x' ||
      data[3] != ':' ) {
    return false;
  }

  std::vector<uint8_t> decoded;
  unsigned int high = 16;
  for( std::size_t i = 4; i < size; ++i ) {
    if( std::isspace(static_cast<unsigned char>(data[i])) != 0 ) {
      continue;
    }

    const unsigned int digit = hexDigit(data[i]);
    if( digit > 15 ) {
      return false;
    }
    if( high > 15 ) {
      high = digit;
    } else {
      decoded.push_back(static_cast<uint8_t>((high << 4) | digit));
      high = 16;
    }
  }

  if( high <= 15 ) {
    return false;
  }
  output.swap(decoded);
  return true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, std::size_t size)
{
  std::vector<uint8_t> serial;
  if( !decodeHexSeed(data, size, serial) && size > 0 ) {
    serial.assign(data, data + size);
  }

  std::size_t position = 0;
  try {
    std::unique_ptr<mustache::Node> node(
        mustache::Node::unserialize(serial, 0, &position));
    std::unique_ptr<std::vector<uint8_t> > encoded(node->serialize());
    if( position != serial.size() || *encoded != serial ) {
      std::abort();
    }
  } catch( const mustache::Exception& ) {
  }

  return 0;
}
