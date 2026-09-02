
#include "mustache.hpp"

#include "exception.hpp"

#include <limits>
#include <string_view>

namespace {

constexpr int encodeVersion(std::string_view version)
{
  unsigned long long components[3] = {};
  std::size_t component = 0;
  bool hasDigit = false;

  for (const char value : version) {
    if (value >= '0' && value <= '9') {
      const unsigned int digit = static_cast<unsigned int>(value - '0');
      if (components[component] > (std::numeric_limits<unsigned long long>::max() - digit) / 10) {
        return -1;
      }
      components[component] = components[component] * 10 + digit;
      hasDigit = true;
    } else if (value == '.' && hasDigit && component < 2) {
      ++component;
      hasDigit = false;
    } else {
      return -1;
    }
  }

  if (component != 2 || !hasDigit) {
    return -1;
  }
  constexpr unsigned long long maxEncoded = static_cast<unsigned long long>(std::numeric_limits<int>::max());
  if (components[0] > maxEncoded / 10000) {
    return -1;
  }
  unsigned long long encoded = components[0] * 10000;
  if (components[1] > (maxEncoded - encoded) / 100) {
    return -1;
  }
  encoded += components[1] * 100;
  if (components[2] > maxEncoded - encoded) {
    return -1;
  }
  return static_cast<int>(encoded + components[2]);
}

constexpr int packageVersionInt = encodeVersion(MUSTACHE_PACKAGE_VERSION);
static_assert(packageVersionInt >= 0, "MUSTACHE_PACKAGE_VERSION must contain three numeric components");

} // namespace

const char * mustache_version()
{
  return MUSTACHE_PACKAGE_VERSION;
}

int mustache_version_int()
{
  return packageVersionInt;
}

namespace mustache {

Exception::~Exception() = default;

void Mustache::tokenize(std::string * tmpl, Node * root)
{
  tokenizer.tokenize(tmpl, root);
}

void Mustache::tokenize(std::string * tmpl, Node * root, const Tokenizer::Limits& limits)
{
  tokenizer.tokenize(tmpl, root, limits);
}

void Mustache::tokenize(std::string_view tmpl, Node * root)
{
  tokenizer.tokenize(tmpl, root);
}

void Mustache::tokenize(std::string_view tmpl, Node * root, const Tokenizer::Limits& limits)
{
  tokenizer.tokenize(tmpl, root, limits);
}

void Mustache::render(const Node * node, const Data * data, const Node::Partials * partials, std::string * output)
{
  renderer.init(node, data, partials, output);
  renderer.render();
}

void Mustache::render(const Node * node, const Data * data, const Node::Partials * partials, std::string * output,
    const RenderLimits& limits)
{
  renderer.init(node, data, partials, output, limits);
  renderer.render();
}

} // namespace mustache
