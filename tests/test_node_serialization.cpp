#include "mustache_config.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "mustache.hpp"

namespace {

int failures = 0;

const char phpMustacheVariableAST[] =
    "4d55000100000000000100000013"
    "4d550010010000050000000000007465737400";

unsigned int hexDigit(char value)
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

std::vector<uint8_t> decodeHex(const char * hex)
{
  const std::string encoded(hex);
  std::vector<uint8_t> decoded;
  if( encoded.size() % 2 != 0 ) {
    return decoded;
  }

  decoded.reserve(encoded.size() / 2);
  for( std::size_t i = 0; i < encoded.size(); i += 2 ) {
    const unsigned int high = hexDigit(encoded[i]);
    const unsigned int low = hexDigit(encoded[i + 1]);
    if( high > 15 || low > 15 ) {
      decoded.clear();
      return decoded;
    }
    decoded.push_back(static_cast<uint8_t>((high << 4) | low));
  }
  return decoded;
}

void expect(bool condition, const char * message)
{
  if( condition ) {
    return;
  }
  std::fprintf(stderr, "%s\n", message);
  ++failures;
}

void expectBytes(
    const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected,
    const char * message)
{
  if( actual == expected ) {
    return;
  }
  std::fprintf(stderr, "%s (expected %zu bytes, got %zu)\n", message,
      expected.size(), actual.size());
  ++failures;
}

void testPHPFixture()
{
  // This exact byte sequence is part of php-mustache's public MustacheAST
  // constructor, __sleep/__wakeup, __toString, toArray, and APC tests.
  std::vector<uint8_t> fixture = decodeHex(phpMustacheVariableAST);
  expect(fixture.size() == 33, "php-mustache fixture must contain 33 bytes");

  std::size_t position = 0;
  std::unique_ptr<mustache::Node> root(
      mustache::Node::unserialize(fixture, 0, &position));

  expect(position == fixture.size(),
      "legacy decoder must consume the complete php-mustache fixture");
  expect(root.get() != NULL, "legacy decoder must return a root node");
  if( root.get() == NULL ) {
    return;
  }

  expect(root->type == mustache::Node::TypeRoot,
      "php-mustache fixture root type changed");
  expect(root->flags == mustache::Node::FlagNone,
      "php-mustache fixture root flags changed");
  expect(root->data == NULL, "php-mustache fixture root unexpectedly has data");
  expect(root->children.size() == 1,
      "php-mustache fixture must contain one child");

  if( root->children.size() == 1 ) {
    mustache::Node * child = root->children[0];
    expect(child != NULL, "php-mustache fixture child must not be null");
    if( child != NULL ) {
      expect(child->type == mustache::Node::TypeVariable,
          "php-mustache fixture child type changed");
      expect(child->flags == mustache::Node::FlagEscape,
          "php-mustache fixture child flags changed");
      expect(child->data != NULL && *child->data == "test",
          "php-mustache fixture child data changed");
      expect(child->children.empty(),
          "php-mustache fixture child unexpectedly has children");
    }
  }

  std::unique_ptr<std::vector<uint8_t> > serialized(root->serialize());
  expectBytes(*serialized, fixture,
      "legacy fixture must serialize back to its exact public byte sequence");

  mustache::Data data(mustache::Data::TypeMap, 0);
  mustache::Data * value = new mustache::Data(mustache::Data::TypeString, 3);
  value->val->assign("baz");
  data.data.insert(std::make_pair(std::string("test"), value));

  mustache::Mustache mustache;
  std::string output;
  mustache.render(root.get(), &data, NULL, &output);
  expect(output == "baz", "decoded php-mustache fixture must render '{{test}}'");
}

void testTokenizerCompatibility()
{
  const std::vector<uint8_t> fixture = decodeHex(phpMustacheVariableAST);
  std::string tmpl("{{test}}");
  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(&tmpl, &root);

  std::unique_ptr<std::vector<uint8_t> > serialized(root.serialize());
  expectBytes(*serialized, fixture,
      "tokenizing '{{test}}' must retain the php-mustache byte format");
}

} // namespace

int main()
{
  testPHPFixture();
  testTokenizerCompatibility();
  return failures == 0 ? 0 : 1;
}
