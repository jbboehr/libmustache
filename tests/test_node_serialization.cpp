#include "mustache_config.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
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

std::string_view byteView(const std::vector<uint8_t>& bytes)
{
  const char * data = bytes.empty()
      ? "" : reinterpret_cast<const char *>(bytes.data());
  return std::string_view(data, bytes.size());
}

void appendUint16(std::vector<uint8_t>& output, std::size_t value)
{
  output.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  output.push_back(static_cast<uint8_t>(value & 0xff));
}

void appendUint24(std::vector<uint8_t>& output, std::size_t value)
{
  output.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  output.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  output.push_back(static_cast<uint8_t>(value & 0xff));
}

void appendUint32(std::vector<uint8_t>& output, std::size_t value)
{
  output.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
  output.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  output.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  output.push_back(static_cast<uint8_t>(value & 0xff));
}

void writeUint16(
    std::vector<uint8_t>& output, std::size_t pos, std::size_t value)
{
  output[pos] = static_cast<uint8_t>((value >> 8) & 0xff);
  output[pos + 1] = static_cast<uint8_t>(value & 0xff);
}

void writeUint24(
    std::vector<uint8_t>& output, std::size_t pos, std::size_t value)
{
  output[pos] = static_cast<uint8_t>((value >> 16) & 0xff);
  output[pos + 1] = static_cast<uint8_t>((value >> 8) & 0xff);
  output[pos + 2] = static_cast<uint8_t>(value & 0xff);
}

void writeUint32(
    std::vector<uint8_t>& output, std::size_t pos, std::size_t value)
{
  output[pos] = static_cast<uint8_t>((value >> 24) & 0xff);
  output[pos + 1] = static_cast<uint8_t>((value >> 16) & 0xff);
  output[pos + 2] = static_cast<uint8_t>((value >> 8) & 0xff);
  output[pos + 3] = static_cast<uint8_t>(value & 0xff);
}

std::vector<uint8_t> makeSerialNode(mustache::Node::Type type, int flags,
    bool hasData, const std::string& data,
    const std::vector<std::vector<uint8_t> >& children)
{
  std::size_t childrenSize = 0;
  for( std::size_t i = 0; i < children.size(); ++i ) {
    childrenSize += children[i].size();
  }

  std::vector<uint8_t> serial;
  serial.push_back('M');
  serial.push_back('U');
  appendUint16(serial, static_cast<std::size_t>(type));
  serial.push_back(static_cast<uint8_t>(flags));
  appendUint24(serial, hasData ? data.size() + 1 : 0);
  appendUint16(serial, children.size());
  appendUint32(serial, childrenSize);
  if( hasData ) {
    serial.insert(serial.end(), data.begin(), data.end());
    serial.push_back(0);
  }
  for( std::size_t i = 0; i < children.size(); ++i ) {
    serial.insert(serial.end(), children[i].begin(), children[i].end());
  }
  return serial;
}

void expectInvalid(std::vector<uint8_t> serial, const char * message,
    std::size_t offset = 0)
{
  const std::size_t unchangedPosition = 1234567;
  std::size_t position = unchangedPosition;
  bool threw = false;
  try {
    std::unique_ptr<mustache::Node> node(
        mustache::Node::unserialize(serial, offset, &position));
  } catch( const mustache::Exception& ) {
    threw = true;
  }

  if( !threw ) {
    std::fprintf(stderr, "%s (decoder accepted invalid input)\n", message);
    ++failures;
  }
  if( position != unchangedPosition ) {
    std::fprintf(stderr, "%s (decoder published a partial position)\n",
        message);
    ++failures;
  }
}

void expectInvalidWithLimits(std::vector<uint8_t> serial,
    const mustache::Node::SerializationLimits& limits, const char * message)
{
  const std::size_t unchangedPosition = 1234567;
  std::size_t position = unchangedPosition;
  bool threw = false;
  try {
    std::unique_ptr<mustache::Node> node(
        mustache::Node::unserialize(serial, 0, &position, limits));
  } catch( const mustache::Exception& ) {
    threw = true;
  }

  if( !threw ) {
    std::fprintf(stderr, "%s (decoder accepted input over its limit)\n",
        message);
    ++failures;
  }
  if( position != unchangedPosition ) {
    std::fprintf(stderr, "%s (decoder published a partial position)\n",
        message);
    ++failures;
  }
}

void expectSerializeInvalid(mustache::Node& node, const char * message)
{
  bool threw = false;
  try {
    std::unique_ptr<std::vector<uint8_t> > serial(node.serialize());
  } catch( const mustache::Exception& ) {
    threw = true;
  }
  if( !threw ) {
    std::fprintf(stderr, "%s (serializer accepted invalid node)\n", message);
    ++failures;
  }
}

void expectSerializeInvalidWithLimits(mustache::Node& node,
    const mustache::Node::SerializationLimits& limits, const char * message)
{
  bool threw = false;
  try {
    std::unique_ptr<std::vector<uint8_t> > serial(node.serialize(limits));
  } catch( const mustache::Exception& ) {
    threw = true;
  }
  if( !threw ) {
    std::fprintf(stderr, "%s (serializer accepted input over its limit)\n",
        message);
    ++failures;
  }
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
  expect(!root->data.has_value(),
      "php-mustache fixture root unexpectedly has data");
  expect(root->children.size() == 1,
      "php-mustache fixture must contain one child");

  if( root->children.size() == 1 ) {
    mustache::Node * child = root->children[0].get();
    expect(child != NULL, "php-mustache fixture child must not be null");
    if( child != NULL ) {
      expect(child->type == mustache::Node::TypeVariable,
          "php-mustache fixture child type changed");
      expect(child->flags == mustache::Node::FlagEscape,
          "php-mustache fixture child flags changed");
      expect(child->data.has_value() && *child->data == "test",
          "php-mustache fixture child data changed");
      expect(child->children.empty(),
          "php-mustache fixture child unexpectedly has children");
    }
  }

  std::unique_ptr<std::vector<uint8_t> > serialized(root->serialize());
  expectBytes(*serialized, fixture,
      "legacy fixture must serialize back to its exact public byte sequence");

  const std::vector<uint8_t> serializedValue = root->serializeValue();
  expectBytes(serializedValue, fixture,
      "value serializer changed the public byte sequence");
  std::unique_ptr<mustache::Node> owned =
      mustache::Node::unserializeOwned(byteView(fixture));
  expect(owned->type == mustache::Node::TypeRoot &&
          owned->children.size() == 1,
      "RAII decoder did not return the complete fixture tree");
  expectBytes(owned->serializeValue(), fixture,
      "RAII codec did not round-trip the public byte sequence");

  mustache::Data data(mustache::Data::TypeMap, 0);
  data.set("test", mustache::Data::string("baz"));

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

void testStrictDecoderValidation()
{
  const std::vector<uint8_t> fixture = decodeHex(phpMustacheVariableAST);

  for( std::size_t size = 0; size < fixture.size(); ++size ) {
    expectInvalid(std::vector<uint8_t>(fixture.begin(), fixture.begin() + size),
        "every truncated php-mustache fixture must be rejected");
  }

  std::vector<uint8_t> invalid = fixture;
  invalid[0] = 'X';
  expectInvalid(invalid, "invalid magic must be rejected");

  invalid = fixture;
  writeUint16(invalid, 2, 0xffff);
  expectInvalid(invalid, "unknown node types must be rejected");

  invalid = fixture;
  invalid[4] = 0x02;
  expectInvalid(invalid, "invalid node flags must be rejected");

  invalid = fixture;
  writeUint24(invalid, 19, 6);
  expectInvalid(invalid, "out-of-bounds data lengths must be rejected");

  invalid = fixture;
  invalid[invalid.size() - 1] = 0x01;
  expectInvalid(invalid, "non-null data terminators must be rejected");

  invalid = fixture;
  writeUint16(invalid, 8, 2);
  expectInvalid(invalid, "incorrect child counts must be rejected");

  invalid = fixture;
  writeUint32(invalid, 10, 18);
  expectInvalid(invalid, "undersized child regions must be rejected");

  invalid = fixture;
  writeUint32(invalid, 10, 0xffffffff);
  expectInvalid(invalid, "oversized child regions must be rejected");

  invalid = fixture;
  invalid.push_back(0);
  expectInvalid(invalid, "trailing bytes must be rejected");

  const std::vector<std::vector<uint8_t> > noChildren;
  expectInvalid(makeSerialNode(
      mustache::Node::TypeRoot, 0, true, "x", noChildren),
      "root data must be rejected");
  expectInvalid(makeSerialNode(
      mustache::Node::TypeVariable, 0, false, "", noChildren),
      "missing variable data must be rejected");
  expectInvalid(makeSerialNode(
      mustache::Node::TypeContainer, 0, false, "", noChildren),
      "pointer-backed container nodes must be rejected");

  const std::vector<uint8_t> partialIndent = makeSerialNode(
      mustache::Node::TypeOutput,
      mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent,
      true, "  ", noChildren);
  expectInvalid(partialIndent,
      "top-level standalone partial metadata must be rejected");

  std::vector<std::vector<uint8_t> > invalidPartialMetadataChildren;
  invalidPartialMetadataChildren.push_back(partialIndent);
  invalidPartialMetadataChildren.push_back(makeSerialNode(
      mustache::Node::TypeVariable, 0, true, "value", noChildren));
  expectInvalid(makeSerialNode(mustache::Node::TypeRoot, 0, false, "",
      invalidPartialMetadataChildren),
      "decoded standalone partial metadata accepted a non-partial successor");

  std::vector<std::vector<uint8_t> > invalidPartialBytesChildren;
  invalidPartialBytesChildren.push_back(makeSerialNode(
      mustache::Node::TypeOutput,
      mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent,
      true, "x\n", noChildren));
  invalidPartialBytesChildren.push_back(makeSerialNode(
      mustache::Node::TypePartial, 0, true, "partial", noChildren));
  expectInvalid(makeSerialNode(mustache::Node::TypeRoot, 0, false, "",
      invalidPartialBytesChildren),
      "decoded standalone partial metadata accepted non-indent bytes");

  std::vector<std::vector<uint8_t> > child;
  child.push_back(makeSerialNode(
      mustache::Node::TypeOutput, 0, true, "x", noChildren));
  expectInvalid(makeSerialNode(
      mustache::Node::TypeVariable, 0, true, "x", child),
      "children on leaf nodes must be rejected");
  expectInvalid(makeSerialNode(mustache::Node::TypeVariable, 0, true,
      std::string(256, '.'), noChildren),
      "excessive dotted-name components must be rejected");

  std::vector<std::vector<uint8_t> > dottedChildren;
  for( int i = 0; i < 391; ++i ) {
    dottedChildren.push_back(makeSerialNode(mustache::Node::TypeVariable, 0,
        true, std::string(255, '.'), noChildren));
  }
  expectInvalid(makeSerialNode(
      mustache::Node::TypeRoot, 0, false, "", dottedChildren),
      "aggregate dotted-name components must be bounded");

  try {
    std::vector<uint8_t> copy = fixture;
    std::unique_ptr<mustache::Node> node(
        mustache::Node::unserialize(copy, 0, NULL));
    std::fprintf(stderr, "null output positions must be rejected\n");
    ++failures;
  } catch( const mustache::Exception& ) {
  }

  expectInvalid(fixture, "out-of-range offsets must be rejected",
      fixture.size() + 1);
}

void testOffsetAndScalarCompatibility()
{
  const std::vector<uint8_t> fixture = decodeHex(phpMustacheVariableAST);
  std::vector<uint8_t> prefixed;
  prefixed.push_back(0xaa);
  prefixed.push_back(0xbb);
  prefixed.insert(prefixed.end(), fixture.begin(), fixture.end());

  std::size_t position = 0;
  std::unique_ptr<mustache::Node> decoded(
      mustache::Node::unserialize(prefixed, 2, &position));
  expect(position == prefixed.size(),
      "nonzero decode offsets must report the exact final position");

  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeComment, ""));
  root.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, std::string("a\0b", 3)));

  std::unique_ptr<std::vector<uint8_t> > serial(root.serialize());
  position = 0;
  std::unique_ptr<mustache::Node> roundTrip(
      mustache::Node::unserialize(*serial, 0, &position));
  expect(roundTrip->children.size() == 2,
      "empty and embedded-NUL data must survive decoding");
  if( roundTrip->children.size() == 2 ) {
    expect(roundTrip->children[0]->data.has_value() &&
            roundTrip->children[0]->data->empty(),
        "empty serialized data must remain present");
    expect(roundTrip->children[1]->data.has_value() &&
            *roundTrip->children[1]->data == std::string("a\0b", 3),
        "embedded NUL bytes must survive decoding");
  }

  std::unique_ptr<std::vector<uint8_t> > serializedAgain(
      roundTrip->serialize());
  expectBytes(*serializedAgain, *serial,
      "empty and embedded-NUL data must round-trip byte-for-byte");
}

void testComplexTokenizerCompatibility()
{
  std::string tmpl(
      "a{{value}}{{^missing}}b{{/missing}}{{#section}}c{{/section}}"
      "{{! comment}}{{>partial}}");
  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(&tmpl, &root);

  std::unique_ptr<std::vector<uint8_t> > serial(root.serialize());
  std::size_t position = 0;
  std::unique_ptr<mustache::Node> decoded(
      mustache::Node::unserialize(*serial, 0, &position));
  expect(position == serial->size(),
      "complex tokenizer output must be consumed exactly");

  bool foundSection = false;
  for( std::size_t i = 0; i < decoded->children.size(); ++i ) {
    mustache::Node * child = decoded->children[i].get();
    if( child->type == mustache::Node::TypeSection ) {
      foundSection = true;
      expect(child->startSequence.has_value() &&
              *child->startSequence == "{{",
          "decoded sections must have a safe default start delimiter");
      expect(child->stopSequence.has_value() &&
              *child->stopSequence == "}}",
          "decoded sections must have a safe default stop delimiter");
    }
  }
  expect(foundSection, "complex tokenizer fixture must contain a section");

  std::unique_ptr<std::vector<uint8_t> > serializedAgain(
      decoded->serialize());
  expectBytes(*serializedAgain, *serial,
      "complex tokenizer output must retain its exact legacy bytes");
}

void testStandalonePartialMetadataRoundTrip()
{
  std::string source("  {{>partial}}\n");
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize(&source, &root);

  const std::vector<uint8_t> serial = root.serializeValue();
  std::unique_ptr<mustache::Node> decoded =
      mustache::Node::unserializeOwned(byteView(serial));
  expect(decoded->to_template_string("{{", "}}") == source,
      "serialized standalone partial metadata lost template source");

  mustache::Node::Partials partials;
  std::unique_ptr<mustache::Node> partial =
      std::make_unique<mustache::Node>();
  std::string partialSource("x\ny");
  engine.tokenize(&partialSource, partial.get());
  partials.emplace("partial", std::move(partial));
  const mustache::Data data = mustache::Data::null();
  std::string output;
  engine.render(decoded.get(), &data, &partials, &output);
  expect(output == "  x\n  y",
      "serialized standalone partial metadata did not affect rendering");
  expectBytes(decoded->serializeValue(), serial,
      "standalone partial metadata did not round-trip byte-for-byte");
}

void testDecoderDepthLimit()
{
  const std::vector<std::vector<uint8_t> > noChildren;
  std::vector<uint8_t> nested = makeSerialNode(
      mustache::Node::TypeOutput, 0, true, "x", noChildren);
  for( int depth = 0; depth < 64; ++depth ) {
    std::vector<std::vector<uint8_t> > child(1, nested);
    nested = makeSerialNode(
        mustache::Node::TypeSection, 0, true, "x", child);
  }

  std::vector<std::vector<uint8_t> > child(1, nested);
  expectInvalid(makeSerialNode(
      mustache::Node::TypeRoot, 0, false, "", child),
      "excessive decoder nesting must be rejected");
}

void testSerializationLimits()
{
  const std::vector<uint8_t> fixture = decodeHex(phpMustacheVariableAST);
  mustache::Node::SerializationLimits limits;

  expect(limits.maxInputBytes == 64 * 1024 * 1024,
      "default serialized-input limit changed");
  expect(limits.maxOutputBytes == 64 * 1024 * 1024,
      "default serialized-output limit changed");
  expect(limits.maxNestingDepth == 64,
      "default serialized nesting limit changed");
  expect(limits.maxNodes == 100000,
      "default serialized node-count limit changed");
  expect(limits.maxDataPartsPerNode == 256,
      "default per-node data-part limit changed");
  expect(limits.maxDataParts == 100000,
      "default aggregate data-part limit changed");

  limits.maxInputBytes = fixture.size() - 1;
  expectInvalidWithLimits(fixture, limits,
      "explicit input-byte limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxNestingDepth = 1;
  expectInvalidWithLimits(fixture, limits,
      "explicit decoder nesting limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxNodes = 1;
  expectInvalidWithLimits(fixture, limits,
      "explicit decoder node-count limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxDataPartsPerNode = 0;
  expectInvalidWithLimits(fixture, limits,
      "explicit per-node decoder data-part limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxDataParts = 0;
  expectInvalidWithLimits(fixture, limits,
      "explicit aggregate decoder data-part limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxInputBytes = fixture.size();
  limits.maxOutputBytes = fixture.size();
  limits.maxNestingDepth = 2;
  limits.maxNodes = 2;
  limits.maxDataPartsPerNode = 1;
  limits.maxDataParts = 1;
  std::vector<uint8_t> copy = fixture;
  std::size_t position = 0;
  std::unique_ptr<mustache::Node> root(
      mustache::Node::unserialize(copy, 0, &position, limits));
  expect(position == fixture.size(),
      "exact decoder resource limits must accept the fixture");
  std::unique_ptr<std::vector<uint8_t> > serialized(root->serialize(limits));
  expectBytes(*serialized, fixture,
      "exact serializer resource limits must accept the fixture");
  std::unique_ptr<mustache::Node> owned =
      mustache::Node::unserializeOwned(byteView(fixture), limits);
  expectBytes(owned->serializeValue(limits), fixture,
      "exact resource limits must accept the RAII codec APIs");

  limits.maxOutputBytes = fixture.size() - 1;
  expectSerializeInvalidWithLimits(*root, limits,
      "explicit output-byte limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxNestingDepth = 1;
  expectSerializeInvalidWithLimits(*root, limits,
      "explicit serializer nesting limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxNodes = 1;
  expectSerializeInvalidWithLimits(*root, limits,
      "explicit serializer node-count limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxDataPartsPerNode = 0;
  expectSerializeInvalidWithLimits(*root, limits,
      "explicit per-node serializer data-part limits must be enforced");

  limits = mustache::Node::SerializationLimits();
  limits.maxDataParts = 0;
  expectSerializeInvalidWithLimits(*root, limits,
      "explicit aggregate serializer data-part limits must be enforced");
}

void testSerializerValidation()
{
  mustache::Node invalidType;
  invalidType.type = mustache::Node::TypeContainer;
  expectSerializeInvalid(invalidType,
      "pointer-backed container nodes must not be serialized");

  mustache::Node invalidFlags;
  invalidFlags.type = mustache::Node::TypeRoot;
  invalidFlags.flags = 2;
  expectSerializeInvalid(invalidFlags,
      "invalid flag placement must not be serialized");

  mustache::Node misplacedLambdaFlag(
      mustache::Node::TypeComment, "comment",
      mustache::Node::FlagLambdaOnly);
  expectSerializeInvalid(misplacedLambdaFlag,
      "lambda-only flags on non-output nodes must not be serialized");

  mustache::Node misplacedPartialIndent(
      mustache::Node::TypeOutput, "  ",
      mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent);
  expectSerializeInvalid(misplacedPartialIndent,
      "standalone partial metadata without a following partial serialized");

  mustache::Node unpairedPartialIndent;
  unpairedPartialIndent.type = mustache::Node::TypeRoot;
  unpairedPartialIndent.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, "  ",
      mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent));
  unpairedPartialIndent.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeVariable, "value"));
  expectSerializeInvalid(unpairedPartialIndent,
      "standalone partial metadata accepted a non-partial successor");

  mustache::Node invalidPartialIndentBytes;
  invalidPartialIndentBytes.type = mustache::Node::TypeRoot;
  invalidPartialIndentBytes.children.push_back(
      std::make_unique<mustache::Node>(mustache::Node::TypeOutput, " \n",
          mustache::Node::FlagLambdaOnly |
              mustache::Node::FlagPartialIndent));
  invalidPartialIndentBytes.children.push_back(
      std::make_unique<mustache::Node>(
          mustache::Node::TypePartial, "partial"));
  expectSerializeInvalid(invalidPartialIndentBytes,
      "standalone partial metadata serialized non-indent bytes");

  mustache::Node missingData;
  missingData.type = mustache::Node::TypeVariable;
  expectSerializeInvalid(missingData,
      "nodes requiring data must not serialize without it");

  mustache::Node invalidRootData(
      mustache::Node::TypeRoot, "unexpected");
  expectSerializeInvalid(invalidRootData,
      "root nodes must not serialize with data");

  mustache::Node nullChild;
  nullChild.type = mustache::Node::TypeRoot;
  nullChild.children.push_back(std::unique_ptr<mustache::Node>());
  expectSerializeInvalid(nullChild, "null children must not be serialized");

  mustache::Node leafWithChild(mustache::Node::TypeVariable, "value");
  leafWithChild.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, "child"));
  expectSerializeInvalid(leafWithChild,
      "children on leaf nodes must not be serialized");

  mustache::Node oversizedData;
  oversizedData.type = mustache::Node::TypeOutput;
  oversizedData.data = std::string(0x00ffffff, 'x');
  expectSerializeInvalid(oversizedData,
      "data exceeding the 24-bit field must not be serialized");

  mustache::Node oversizedChildren;
  oversizedChildren.type = mustache::Node::TypeRoot;
  oversizedChildren.children.resize(0x00010000);
  expectSerializeInvalid(oversizedChildren,
      "child counts exceeding the 16-bit field must not be serialized");

  mustache::Node excessiveDataParts;
  excessiveDataParts.type = mustache::Node::TypeVariable;
  excessiveDataParts.data = std::string(256, '.');
  expectSerializeInvalid(excessiveDataParts,
      "excessive dotted-name components must not be serialized");

  mustache::Node aggregateDataParts;
  aggregateDataParts.type = mustache::Node::TypeRoot;
  for( int i = 0; i < 391; ++i ) {
    aggregateDataParts.children.push_back(std::make_unique<mustache::Node>(
        mustache::Node::TypeVariable, std::string(255, '.')));
  }
  expectSerializeInvalid(aggregateDataParts,
      "aggregate dotted-name components must be bounded while serializing");

  mustache::Node excessiveDepth;
  excessiveDepth.type = mustache::Node::TypeRoot;
  mustache::Node * parent = &excessiveDepth;
  for( int depth = 0; depth < 64; ++depth ) {
    parent->children.push_back(std::make_unique<mustache::Node>(
        mustache::Node::TypeSection, "x"));
    parent = parent->children.back().get();
  }
  expectSerializeInvalid(excessiveDepth,
      "excessive node nesting must not be serialized");
}

} // namespace

int main()
{
  testPHPFixture();
  testTokenizerCompatibility();
  testStrictDecoderValidation();
  testOffsetAndScalarCompatibility();
  testComplexTokenizerCompatibility();
  testStandalonePartialMetadataRoundTrip();
  testDecoderDepthLimit();
  testSerializationLimits();
  testSerializerValidation();
  return failures == 0 ? 0 : 1;
}
