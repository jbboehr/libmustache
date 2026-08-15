#include "mustache_config.h"

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "exception.hpp"
#include "mustache.hpp"
#include "node.hpp"
#include "tokenizer.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

void populateExistingTree(mustache::Node * root)
{
  root->type = mustache::Node::TypeSection;
  root->flags = mustache::Node::FlagEscape;
  root->setData("original.path");
  root->children.push_back(std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "original child"));
  root->child = std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "container child");
  root->partials.emplace(
      "original partial", std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "partial body"));
  root->startSequence = "<%";
  root->stopSequence = "%>";
}

void testFailedTokenizePreservesExistingTree()
{
  mustache::Node root;
  populateExistingTree(&root);
  mustache::Node * originalChild = root.children.front().get();
  mustache::Node * originalContainerChild = root.child.get();

  mustache::Tokenizer tokenizer;
  bool rejected = false;
  try {
    tokenizer.tokenize("prefix {{#unfinished}}body", &root);
  } catch (const mustache::TokenizerException&) {
    rejected = true;
  }

  expect(rejected, "an unfinished template was accepted");
  expect(root.type == mustache::Node::TypeSection && root.flags == mustache::Node::FlagEscape,
      "failed tokenization changed the existing root state");
  expect(root.data.has_value() && *root.data == "original.path" && root.dataParts.size() == 2,
      "failed tokenization changed the existing root data");
  expect(root.children.size() == 1 && root.children.front().get() == originalChild &&
          root.child.get() == originalContainerChild,
      "failed tokenization changed the existing child ownership");
  expect(root.partials.size() == 1 && root.startSequence.has_value() && *root.startSequence == "<%" &&
          root.stopSequence.has_value() && *root.stopSequence == "%>",
      "failed tokenization changed existing auxiliary AST state");
}

void testSuccessfulTokenizeReplacesExistingTree()
{
  mustache::Node root;
  populateExistingTree(&root);

  mustache::Tokenizer tokenizer;
  tokenizer.tokenize("new {{value}}", &root);

  expect(root.type == mustache::Node::TypeRoot && root.flags == mustache::Node::FlagNone,
      "successful tokenization did not publish a new root");
  expect(!root.data.has_value() && root.dataParts.empty() && root.child == NULL && !root.startSequence.has_value() &&
          !root.stopSequence.has_value(),
      "successful tokenization retained state from the previous tree");
  expect(root.partials.size() == 1 && root.partials.find("original partial") != root.partials.end(),
      "successful tokenization discarded caller-managed partials");
  expect(root.children.size() == 2 && root.children[0]->type == mustache::Node::TypeOutput &&
          *root.children[0]->data == "new " && root.children[1]->type == mustache::Node::TypeVariable &&
          *root.children[1]->data == "value",
      "successful tokenization published an unexpected tree");
}

void testSetDataReplacesOwnedState()
{
  mustache::Node node(mustache::Node::TypeVariable, "one.two");
  expect(node.dataParts.size() == 2, "the initial dotted name was not split");

  node.setData("plain");
  expect(node.data.has_value() && *node.data == "plain" && node.dataParts.empty(),
      "setData retained stale dotted-name state");

  node.setData("three.parts.here");
  expect(node.data.has_value() && *node.data == "three.parts.here" && node.dataParts.size() == 3,
      "setData did not publish its replacement state");
}

void expectLimitFailure(std::string_view source, const mustache::Tokenizer::Limits& limits,
    const char * expectedMessage, const char * failureMessage)
{
  mustache::Node root;
  populateExistingTree(&root);
  mustache::Node * originalChild = root.children.front().get();
  mustache::Node * originalContainerChild = root.child.get();
  mustache::Tokenizer tokenizer;

  bool rejected = false;
  try {
    tokenizer.tokenize(source, &root, limits);
  } catch (const mustache::TokenizerException& exception) {
    rejected = std::string(exception.what()).find(expectedMessage) != std::string::npos;
  }

  expect(rejected, failureMessage);
  expect(root.type == mustache::Node::TypeSection && root.children.size() == 1 &&
          root.children.front().get() == originalChild && root.child.get() == originalContainerChild,
      "a parser limit failure changed the destination tree");
}

std::string nestedSections(std::size_t depth)
{
  std::string source;
  source.reserve(depth * 12);
  for (std::size_t i = 0; i < depth; ++i) {
    source.append("{{#s}}");
  }
  for (std::size_t i = 0; i < depth; ++i) {
    source.append("{{/s}}");
  }
  return source;
}

void testParserLimits()
{
  mustache::Tokenizer::Limits limits;
  expect(limits.maxNestingDepth == 62, "default parser nesting limit changed");

  mustache::Node defaultDepthRoot;
  mustache::Tokenizer tokenizer;
  tokenizer.tokenize(nestedSections(limits.maxNestingDepth), &defaultDepthRoot, limits);
  bool defaultDepthSerializable = false;
  try {
    std::unique_ptr<std::vector<uint8_t>> serial(defaultDepthRoot.serialize());
    defaultDepthSerializable = !serial->empty();
  } catch (const mustache::Exception&) {
  }
  expect(defaultDepthSerializable, "the default parser depth produced an unserializable AST");
  expectLimitFailure(nestedSections(limits.maxNestingDepth + 1), limits, "nesting limit",
      "the default parser accepted a depth that default "
      "serialization rejects");

  limits.maxInputBytes = 2;
  mustache::Node root;
  tokenizer.tokenize("ab", &root, limits);
  expect(root.children.size() == 1 && *root.children[0]->data == "ab", "the exact template input limit was rejected");
  limits.maxInputBytes = 1;
  expectLimitFailure("ab", limits, "input size limit", "an oversized template input was accepted");

  limits = mustache::Tokenizer::Limits();
  limits.maxNodes = 2;
  tokenizer.tokenize("a{{b}}", &root, limits);
  expect(root.children.size() == 2, "the exact parser node limit was rejected");
  limits.maxNodes = 1;
  expectLimitFailure("a{{b}}", limits, "node count limit", "an oversized parser node count was accepted");

  limits = mustache::Tokenizer::Limits();
  limits.maxNestingDepth = 2;
  tokenizer.tokenize("{{#a}}{{#b}}{{/b}}{{/a}}", &root, limits);
  expect(root.children.size() == 1 && root.children[0]->children.size() == 2,
      "the exact parser nesting limit was rejected");
  limits.maxNestingDepth = 1;
  expectLimitFailure(
      "{{#a}}{{#b}}{{/b}}{{/a}}", limits, "nesting limit", "an oversized parser nesting depth was accepted");
  limits.maxNestingDepth = 0;
  expectLimitFailure("{{#a}}{{/a}}", limits, "nesting limit", "a zero parser nesting limit was treated as unlimited");

  limits = mustache::Tokenizer::Limits();
  limits.maxTagBytes = 3;
  tokenizer.tokenize("{{abc}}", &root, limits);
  expect(root.children.size() == 1 && *root.children[0]->data == "abc", "the exact template tag limit was rejected");
  limits.maxTagBytes = 2;
  expectLimitFailure("{{abc}}", limits, "tag size limit", "an oversized template tag was accepted");
  limits.maxTagBytes = 0;
  expectLimitFailure("{{a}}", limits, "tag size limit", "a zero template tag limit was treated as unlimited");

  limits = mustache::Tokenizer::Limits();
  limits.maxDelimiterBytes = 2;
  tokenizer.tokenize("{{=<% %>=}}<%value%>", &root, limits);
  expect(root.children.size() == 1 && *root.children[0]->data == "value",
      "the exact changed-delimiter limit was rejected");
  expectLimitFailure("{{=<<< >>>=}}", limits, "delimiter size limit", "an oversized changed delimiter was accepted");
  limits.maxDelimiterBytes = 0;
  expectLimitFailure("", limits, "delimiter size limit", "a zero delimiter limit was treated as unlimited");

  limits.maxDelimiterBytes = 2;

  mustache::Tokenizer longDelimiterTokenizer;
  longDelimiterTokenizer.setStartSequence("<<<");
  bool rejected = false;
  try {
    longDelimiterTokenizer.tokenize("", &root, limits);
  } catch (const mustache::TokenizerException& exception) {
    rejected = std::string(exception.what()).find("delimiter size limit") != std::string::npos;
  }
  expect(rejected, "an oversized configured delimiter was accepted");

  limits = mustache::Tokenizer::Limits();
  limits.maxInputBytes = 0;
  limits.maxNodes = 0;
  tokenizer.tokenize("", &root, limits);
  expect(root.type == mustache::Node::TypeRoot && root.children.empty(),
      "zero limits rejected an empty template that consumes no resources");
}

void testSectionClosureValidation()
{
  mustache::Tokenizer tokenizer;
  mustache::Node root;
  bool rejected = false;
  try {
    tokenizer.tokenize("before{{/orphan}}after", &root);
  } catch (const mustache::TokenizerException& exception) {
    rejected = std::string(exception.what()).find("Extra closing section 'orphan'") != std::string::npos;
  }
  expect(rejected, "an orphan closing section was accepted");

  rejected = false;
  try {
    tokenizer.tokenize("{{#open}}body{{/different}}", &root);
  } catch (const mustache::TokenizerException& exception) {
    const std::string message(exception.what());
    rejected = message.find("Mismatched closing section 'different'") != std::string::npos &&
        message.find("expected 'open'") != std::string::npos && message.find("opened at") != std::string::npos;
  }
  expect(rejected, "a mismatched closing section was accepted");

  tokenizer.tokenize("{{#outer}}{{=<% %>=}}<%#inner%><%/inner%><%/outer%>", &root);
  expect(root.children.size() == 1 && root.children[0]->children.size() == 2 &&
          root.children[0]->children[0]->type == mustache::Node::TypeSection,
      "valid nested sections with changed delimiters were rejected");
}

std::string renderSource(std::string_view source, const mustache::Data& data)
{
  return mustache::render(mustache::compile(source), data);
}

void testStandaloneTagStripping()
{
  const mustache::Data empty = mustache::Data::object();

  expect(renderSource("Begin.\n  {{! Comment Block! }}\nEnd.\n", empty) == "Begin.\nEnd.\n",
      "an indented standalone comment line was not removed");
  expect(renderSource("Begin.\n\t{{!\ninside\n}}\nEnd.", empty) == "Begin.\nEnd.",
      "a multiline standalone comment was not removed");
  expect(renderSource("|\r\n{{! Standalone Comment }}\r\n|", empty) == "|\r\n|",
      "CRLF standalone comment stripping changed line endings");
  expect(renderSource(" \t{{! no previous line }}\nnext", empty) == "next",
      "a standalone comment required a preceding newline");
  expect(renderSource("first\n \t{{! no following newline }}", empty) == "first\n",
      "a standalone comment required a following newline");
  expect(renderSource("inline {{! comment }}\n", empty) == "inline \n",
      "an inline comment incorrectly stripped its surrounding line");
  expect(renderSource("\v{{! comment }}\n", empty) == "\v\n",
      "non-indentation whitespace incorrectly made a tag standalone");

  mustache::Data truthy = mustache::Data::object();
  truthy.set("show", mustache::Data::boolean(true));
  expect(renderSource("A\n  {{#show}}\nvalue\n\t{{/show}}\nB\n", truthy) == "A\nvalue\nB\n",
      "standalone section lines were not removed");

  mustache::Data falsey = mustache::Data::object();
  falsey.set("show", mustache::Data::boolean(false));
  expect(renderSource("A\n  {{^show}}\nvalue\n\t{{/show}}\nB\n", falsey) == "A\nvalue\nB\n",
      "standalone inverted-section lines were not removed");

  expect(renderSource("A\r\n\t{{=<% %>=}}\r\n<%#show%>\r\nvalue\r\n"
                      "<%/show%>\r\nB",
             truthy) == "A\r\nvalue\r\nB",
      "standalone stripping failed with changed delimiters");

  const char nulTemplate[] = "\0{{! comment }}\n";
  std::string nulSource(nulTemplate, sizeof(nulTemplate) - 1);
  std::string nulExpected("\0\n", 2);
  expect(renderSource(nulSource, empty) == nulExpected, "an embedded NUL incorrectly made a comment standalone");

  mustache::Tokenizer::Limits limits;
  limits.maxNodes = 3;
  mustache::Tokenizer tokenizer;
  mustache::Node root;
  tokenizer.tokenize(" \t{{! comment }}\n", &root, limits);
  expect(root.children.size() == 3 && root.children[0]->type == mustache::Node::TypeOutput &&
          root.children[0]->flags == mustache::Node::FlagLambdaOnly &&
          root.children[1]->type == mustache::Node::TypeComment &&
          root.children[2]->type == mustache::Node::TypeOutput &&
          root.children[2]->flags == mustache::Node::FlagLambdaOnly,
      "standalone source boundaries were not retained for lambdas");
  limits.maxNodes = 2;
  expectLimitFailure(" \t{{! comment }}\n", limits, "node count limit",
      "lambda-only standalone source nodes bypassed the parser node limit");

  limits.maxNodes = 3;
  tokenizer.tokenize("  {{>partial}}\n", &root, limits);
  expect(root.children.size() == 3 && root.children[0]->type == mustache::Node::TypeOutput &&
          root.children[0]->flags == (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent) &&
          root.children[0]->data.has_value() && *root.children[0]->data == "  " &&
          root.children[1]->type == mustache::Node::TypePartial && root.children[1]->data.has_value() &&
          *root.children[1]->data == "partial" && root.children[2]->type == mustache::Node::TypeOutput &&
          root.children[2]->flags == mustache::Node::FlagLambdaOnly,
      "standalone partial indentation was not retained as bounded metadata");
  limits.maxNodes = 2;
  expectLimitFailure(
      "{{>partial}}\n", limits, "node count limit", "zero-byte standalone partial metadata bypassed the node limit");
}

void testRepeatedInlineTagsUseBoundedForwardWork()
{
  const std::string delimiterTags = "{{=<% %>=}}<%={{ }}=%>";
  const std::size_t pairCount = 10000;
  std::string source("x");
  source.reserve(source.size() + delimiterTags.size() * pairCount);
  for (std::size_t i = 0; i < pairCount; ++i) {
    source.append(delimiterTags);
  }

  mustache::Tokenizer::Limits limits;
  limits.maxNodes = 1;
  mustache::Tokenizer tokenizer;
  mustache::Node root;
  tokenizer.tokenize(source, &root, limits);
  expect(root.children.size() == 1 && root.children[0]->type == mustache::Node::TypeOutput &&
          root.children[0]->data.has_value() && *root.children[0]->data == "x",
      "repeated inline delimiter tags changed the parsed output");
}

} // namespace

int main()
{
  testFailedTokenizePreservesExistingTree();
  testSuccessfulTokenizeReplacesExistingTree();
  testSetDataReplacesOwnedState();
  testParserLimits();
  testSectionClosureValidation();
  testStandaloneTagStripping();
  testRepeatedInlineTagsUseBoundedForwardWork();
  return failures == 0 ? 0 : 1;
}
