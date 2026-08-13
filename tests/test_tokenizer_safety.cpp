#include "mustache_config.h"

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "exception.hpp"
#include "node.hpp"
#include "tokenizer.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if( !condition ) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

void populateExistingTree(mustache::Node * root)
{
  root->type = mustache::Node::TypeSection;
  root->flags = mustache::Node::FlagEscape;
  root->setData("original.path");
  root->children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, "original child"));
  root->child = std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, "container child");
  root->partials.emplace("original partial",
      std::make_unique<mustache::Node>(
          mustache::Node::TypeOutput, "partial body"));
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
  } catch( const mustache::TokenizerException& ) {
    rejected = true;
  }

  expect(rejected, "an unfinished template was accepted");
  expect(root.type == mustache::Node::TypeSection &&
          root.flags == mustache::Node::FlagEscape,
      "failed tokenization changed the existing root state");
  expect(root.data.has_value() && *root.data == "original.path" &&
          root.dataParts.size() == 2,
      "failed tokenization changed the existing root data");
  expect(root.children.size() == 1 &&
          root.children.front().get() == originalChild &&
          root.child.get() == originalContainerChild,
      "failed tokenization changed the existing child ownership");
  expect(root.partials.size() == 1 &&
          root.startSequence.has_value() && *root.startSequence == "<%" &&
          root.stopSequence.has_value() && *root.stopSequence == "%>",
      "failed tokenization changed existing auxiliary AST state");
}

void testSuccessfulTokenizeReplacesExistingTree()
{
  mustache::Node root;
  populateExistingTree(&root);

  mustache::Tokenizer tokenizer;
  tokenizer.tokenize("new {{value}}", &root);

  expect(root.type == mustache::Node::TypeRoot &&
          root.flags == mustache::Node::FlagNone,
      "successful tokenization did not publish a new root");
  expect(!root.data.has_value() && root.dataParts.empty() &&
          root.child == NULL && !root.startSequence.has_value() &&
          !root.stopSequence.has_value(),
      "successful tokenization retained state from the previous tree");
  expect(root.partials.size() == 1 &&
          root.partials.find("original partial") != root.partials.end(),
      "successful tokenization discarded caller-managed partials");
  expect(root.children.size() == 2 &&
          root.children[0]->type == mustache::Node::TypeOutput &&
          *root.children[0]->data == "new " &&
          root.children[1]->type == mustache::Node::TypeVariable &&
          *root.children[1]->data == "value",
      "successful tokenization published an unexpected tree");
}

void testSetDataReplacesOwnedState()
{
  mustache::Node node(mustache::Node::TypeVariable, "one.two");
  expect(node.dataParts.size() == 2,
      "the initial dotted name was not split");

  node.setData("plain");
  expect(node.data.has_value() && *node.data == "plain" &&
          node.dataParts.empty(),
      "setData retained stale dotted-name state");

  node.setData("three.parts.here");
  expect(node.data.has_value() && *node.data == "three.parts.here" &&
          node.dataParts.size() == 3,
      "setData did not publish its replacement state");
}

void expectLimitFailure(std::string_view source,
    const mustache::Tokenizer::Limits& limits, const char * expectedMessage,
    const char * failureMessage)
{
  mustache::Node root;
  populateExistingTree(&root);
  mustache::Node * originalChild = root.children.front().get();
  mustache::Node * originalContainerChild = root.child.get();
  mustache::Tokenizer tokenizer;

  bool rejected = false;
  try {
    tokenizer.tokenize(source, &root, limits);
  } catch( const mustache::TokenizerException& exception ) {
    rejected = std::string(exception.what()).find(expectedMessage) !=
        std::string::npos;
  }

  expect(rejected, failureMessage);
  expect(root.type == mustache::Node::TypeSection &&
          root.children.size() == 1 &&
          root.children.front().get() == originalChild &&
          root.child.get() == originalContainerChild,
      "a parser limit failure changed the destination tree");
}

std::string nestedSections(std::size_t depth)
{
  std::string source;
  source.reserve(depth * 12);
  for( std::size_t i = 0; i < depth; ++i ) {
    source.append("{{#s}}");
  }
  for( std::size_t i = 0; i < depth; ++i ) {
    source.append("{{/s}}");
  }
  return source;
}

void testParserLimits()
{
  mustache::Tokenizer::Limits limits;
  expect(limits.maxNestingDepth == 62,
      "default parser nesting limit changed");

  mustache::Node defaultDepthRoot;
  mustache::Tokenizer tokenizer;
  tokenizer.tokenize(
      nestedSections(limits.maxNestingDepth), &defaultDepthRoot, limits);
  bool defaultDepthSerializable = false;
  try {
    std::unique_ptr<std::vector<uint8_t> > serial(
        defaultDepthRoot.serialize());
    defaultDepthSerializable = !serial->empty();
  } catch( const mustache::Exception& ) {
  }
  expect(defaultDepthSerializable,
      "the default parser depth produced an unserializable AST");
  expectLimitFailure(nestedSections(limits.maxNestingDepth + 1), limits,
      "nesting limit", "the default parser accepted a depth that default "
      "serialization rejects");

  limits.maxInputBytes = 2;
  mustache::Node root;
  tokenizer.tokenize("ab", &root, limits);
  expect(root.children.size() == 1 && *root.children[0]->data == "ab",
      "the exact template input limit was rejected");
  limits.maxInputBytes = 1;
  expectLimitFailure("ab", limits, "input size limit",
      "an oversized template input was accepted");

  limits = mustache::Tokenizer::Limits();
  limits.maxNodes = 2;
  tokenizer.tokenize("a{{b}}", &root, limits);
  expect(root.children.size() == 2,
      "the exact parser node limit was rejected");
  limits.maxNodes = 1;
  expectLimitFailure("a{{b}}", limits, "node count limit",
      "an oversized parser node count was accepted");

  limits = mustache::Tokenizer::Limits();
  limits.maxNestingDepth = 2;
  tokenizer.tokenize("{{#a}}{{#b}}{{/b}}{{/a}}", &root, limits);
  expect(root.children.size() == 1 &&
          root.children[0]->children.size() == 2,
      "the exact parser nesting limit was rejected");
  limits.maxNestingDepth = 1;
  expectLimitFailure("{{#a}}{{#b}}{{/b}}{{/a}}", limits,
      "nesting limit", "an oversized parser nesting depth was accepted");
  limits.maxNestingDepth = 0;
  expectLimitFailure("{{#a}}{{/a}}", limits, "nesting limit",
      "a zero parser nesting limit was treated as unlimited");

  limits = mustache::Tokenizer::Limits();
  limits.maxTagBytes = 3;
  tokenizer.tokenize("{{abc}}", &root, limits);
  expect(root.children.size() == 1 && *root.children[0]->data == "abc",
      "the exact template tag limit was rejected");
  limits.maxTagBytes = 2;
  expectLimitFailure("{{abc}}", limits, "tag size limit",
      "an oversized template tag was accepted");
  limits.maxTagBytes = 0;
  expectLimitFailure("{{a}}", limits, "tag size limit",
      "a zero template tag limit was treated as unlimited");

  limits = mustache::Tokenizer::Limits();
  limits.maxDelimiterBytes = 2;
  tokenizer.tokenize("{{=<% %>=}}<%value%>", &root, limits);
  expect(root.children.size() == 1 && *root.children[0]->data == "value",
      "the exact changed-delimiter limit was rejected");
  expectLimitFailure("{{=<<< >>>=}}", limits, "delimiter size limit",
      "an oversized changed delimiter was accepted");
  limits.maxDelimiterBytes = 0;
  expectLimitFailure("", limits, "delimiter size limit",
      "a zero delimiter limit was treated as unlimited");

  limits.maxDelimiterBytes = 2;

  mustache::Tokenizer longDelimiterTokenizer;
  longDelimiterTokenizer.setStartSequence("<<<");
  bool rejected = false;
  try {
    longDelimiterTokenizer.tokenize("", &root, limits);
  } catch( const mustache::TokenizerException& exception ) {
    rejected = std::string(exception.what()).find("delimiter size limit") !=
        std::string::npos;
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
  } catch( const mustache::TokenizerException& exception ) {
    rejected = std::string(exception.what()).find(
        "Extra closing section 'orphan'") != std::string::npos;
  }
  expect(rejected, "an orphan closing section was accepted");

  rejected = false;
  try {
    tokenizer.tokenize("{{#open}}body{{/different}}", &root);
  } catch( const mustache::TokenizerException& exception ) {
    const std::string message(exception.what());
    rejected = message.find("Mismatched closing section 'different'") !=
            std::string::npos &&
        message.find("expected 'open'") != std::string::npos &&
        message.find("opened at") != std::string::npos;
  }
  expect(rejected, "a mismatched closing section was accepted");

  tokenizer.tokenize(
      "{{#outer}}{{=<% %>=}}<%#inner%><%/inner%><%/outer%>", &root);
  expect(root.children.size() == 1 &&
          root.children[0]->children.size() == 2 &&
          root.children[0]->children[0]->type ==
              mustache::Node::TypeSection,
      "valid nested sections with changed delimiters were rejected");
}

} // namespace

int main()
{
  testFailedTokenizePreservesExistingTree();
  testSuccessfulTokenizeReplacesExistingTree();
  testSetDataReplacesOwnedState();
  testParserLimits();
  testSectionClosureValidation();
  return failures == 0 ? 0 : 1;
}
