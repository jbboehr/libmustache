#include "mustache_config.h"

#include <cstdio>
#include <string>

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
  root->children.push_back(
      new mustache::Node(mustache::Node::TypeOutput, "original child"));
  root->child = root->children.front();
  root->partials.emplace("original partial", mustache::Node(
      mustache::Node::TypeOutput, "partial body"));
  root->startSequence = new std::string("<%");
  root->stopSequence = new std::string("%>");
}

void testFailedTokenizePreservesExistingTree()
{
  mustache::Node root;
  populateExistingTree(&root);
  mustache::Node * originalChild = root.children.front();

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
  expect(root.data != NULL && *root.data == "original.path" &&
          root.dataParts != NULL && root.dataParts->size() == 2,
      "failed tokenization changed the existing root data");
  expect(root.children.size() == 1 &&
          root.children.front() == originalChild &&
          root.child == originalChild,
      "failed tokenization changed the existing child ownership");
  expect(root.partials.size() == 1 &&
          root.startSequence != NULL && *root.startSequence == "<%" &&
          root.stopSequence != NULL && *root.stopSequence == "%>",
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
  expect(root.data == NULL && root.dataParts == NULL && root.child == NULL &&
          root.startSequence == NULL && root.stopSequence == NULL,
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
  expect(node.dataParts != NULL && node.dataParts->size() == 2,
      "the initial dotted name was not split");

  node.setData("plain");
  expect(node.data != NULL && *node.data == "plain" &&
          node.dataParts == NULL,
      "setData retained stale dotted-name state");

  node.setData("three.parts.here");
  expect(node.data != NULL && *node.data == "three.parts.here" &&
          node.dataParts != NULL && node.dataParts->size() == 3,
      "setData did not publish its replacement state");
}

} // namespace

int main()
{
  testFailedTokenizePreservesExistingTree();
  testSuccessfulTokenizeReplacesExistingTree();
  testSetDataReplacesOwnedState();
  return failures == 0 ? 0 : 1;
}
