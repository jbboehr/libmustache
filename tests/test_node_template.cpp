#include <cstdio>
#include <cstddef>
#include <memory>
#include <string>

#include "exception.hpp"
#include "node.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if( condition ) {
    return;
  }
  std::fprintf(stderr, "%s\n", message);
  ++failures;
}

template<typename Callable>
void expectException(Callable callable, const char * expected,
    const char * message)
{
  try {
    callable();
    std::fprintf(stderr, "%s (no exception)\n", message);
    ++failures;
  } catch( const mustache::Exception& exception ) {
    if( std::string(exception.what()) != expected ) {
      std::fprintf(stderr, "%s (expected '%s', got '%s')\n",
          message, expected, exception.what());
      ++failures;
    }
  }
}

std::unique_ptr<mustache::Node> makeNode(
    mustache::Node::Type type, const std::string& data, int flags = 0)
{
  return std::make_unique<mustache::Node>(type, data, flags);
}

void testCompatibilityOutput()
{
  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(makeNode(mustache::Node::TypeOutput, "before"));
  root.children.push_back(makeNode(
      mustache::Node::TypeVariable, "value", mustache::Node::FlagEscape));

  std::unique_ptr<mustache::Node> section =
      makeNode(mustache::Node::TypeSection, "section");
  section->children.push_back(makeNode(
      mustache::Node::TypeOutput, std::string("a\0b", 3)));
  section->children.push_back(
      makeNode(mustache::Node::TypeStop, "section"));
  root.children.push_back(std::move(section));
  root.children.push_back(makeNode(mustache::Node::TypeStop, "ignored"));

  std::string expected("before{{value}}{{#section}}");
  expected.append("a\0b", 3);
  expected.append("{{/section}}");
  expect(root.children_to_template_string("{{", "}}") == expected,
      "bounded child reconstruction changed compatibility output");

  const std::string fullExpected = expected + "{{/ignored}}";
  expect(root.to_template_string("{{", "}}") == fullExpected,
      "bounded node reconstruction changed compatibility output");

  std::string alternateExpected("before<%value%><%#section%>");
  alternateExpected.append("a\0b", 3);
  alternateExpected.append("<%/section%><%/ignored%>");
  expect(root.to_template_string("<%", "%>") == alternateExpected,
      "bounded reconstruction changed alternate-delimiter output");
}

void testDefaultLimits()
{
  const mustache::Node::TemplateStringLimits limits;
  expect(limits.maxOutputBytes == 64 * 1024 * 1024,
      "default template reconstruction output limit changed");
  expect(limits.maxNestingDepth == 64,
      "default template reconstruction nesting limit changed");
  expect(limits.maxNodes == 100001,
      "default template reconstruction node limit changed");
}

void testOutputLimit()
{
  mustache::Node output(mustache::Node::TypeOutput, "abcd");
  mustache::Node::TemplateStringLimits limits;
  limits.maxOutputBytes = 4;
  expect(output.to_template_string("{{", "}}", limits) == "abcd",
      "the exact template reconstruction output limit was rejected");

  limits.maxOutputBytes = 3;
  expectException([&output, &limits]() {
    static_cast<void>(
        output.to_template_string("{{", "}}", limits));
  }, "Template reconstruction size limit exceeded",
      "oversized template reconstruction output was accepted");

  mustache::Node emptyRoot;
  emptyRoot.type = mustache::Node::TypeRoot;
  limits.maxOutputBytes = 0;
  expect(emptyRoot.to_template_string("{{", "}}", limits).empty(),
      "a zero-byte limit rejected empty reconstruction output");
}

void testNestingLimit()
{
  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(makeNode(mustache::Node::TypeSection, "outer"));
  root.children.front()->children.push_back(
      makeNode(mustache::Node::TypeOutput, "value"));

  mustache::Node::TemplateStringLimits limits;
  limits.maxNestingDepth = 3;
  expect(root.to_template_string("{{", "}}", limits) ==
          "{{#outer}}value",
      "the exact template reconstruction nesting limit was rejected");

  limits.maxNestingDepth = 2;
  expectException([&root, &limits]() {
    static_cast<void>(root.to_template_string("{{", "}}", limits));
  }, "Template node nesting limit exceeded",
      "oversized template reconstruction nesting was accepted");

  limits.maxNestingDepth = 0;
  expectException([&root, &limits]() {
    static_cast<void>(root.to_template_string("{{", "}}", limits));
  }, "Template node nesting limit exceeded",
      "a zero template reconstruction nesting limit was unlimited");

  mustache::Node excessive;
  excessive.type = mustache::Node::TypeRoot;
  mustache::Node * parent = &excessive;
  for( std::size_t depth = 0; depth < 256; ++depth ) {
    parent->children.push_back(
        makeNode(mustache::Node::TypeSection, "section"));
    parent = parent->children.back().get();
  }
  limits = mustache::Node::TemplateStringLimits();
  limits.maxNestingDepth = 1000;
  limits.maxNodes = 1000;
  expectException([&excessive, &limits]() {
    static_cast<void>(
        excessive.to_template_string("{{", "}}", limits));
  }, "Template node nesting limit exceeded",
      "configured limits bypassed the reconstruction stack ceiling");
}

void testNodeLimit()
{
  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(makeNode(mustache::Node::TypeOutput, "a"));
  root.children.push_back(makeNode(mustache::Node::TypeStop, "ignored"));

  mustache::Node::TemplateStringLimits limits;
  limits.maxNodes = 3;
  expect(root.children_to_template_string("{{", "}}", limits) == "a",
      "the exact child reconstruction node limit was rejected");

  limits.maxNodes = 2;
  expectException([&root, &limits]() {
    static_cast<void>(
        root.children_to_template_string("{{", "}}", limits));
  }, "Template node count limit exceeded",
      "a skipped closing node bypassed the reconstruction work budget");

  limits.maxNodes = 0;
  expectException([&root, &limits]() {
    static_cast<void>(root.to_template_string("{{", "}}", limits));
  }, "Template node count limit exceeded",
      "a zero template reconstruction node limit was unlimited");
}

void testMalformedNodesAndReuse()
{
  mustache::Node missingData;
  missingData.type = mustache::Node::TypeVariable;
  expectException([&missingData]() {
    static_cast<void>(missingData.to_template_string("{{", "}}"));
  }, "Invalid node without data",
      "template reconstruction accepted a node without required data");

  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(makeNode(mustache::Node::TypeOutput, "prefix"));
  root.children.push_back(std::unique_ptr<mustache::Node>());
  expectException([&root]() {
    static_cast<void>(root.to_template_string("{{", "}}"));
  }, "Invalid null child node",
      "template reconstruction accepted a null child");

  root.children.pop_back();
  root.children.push_back(makeNode(mustache::Node::TypeOutput, "suffix"));
  expect(root.to_template_string("{{", "}}") == "prefixsuffix",
      "a failed reconstruction left the source tree unusable");
}

} // namespace

int main()
{
  testCompatibilityOutput();
  testDefaultLimits();
  testOutputLimit();
  testNestingLimit();
  testNodeLimit();
  testMalformedNodesAndReuse();
  return failures == 0 ? 0 : 1;
}
