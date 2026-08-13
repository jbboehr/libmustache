#include "mustache_config.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "data.hpp"
#include "exception.hpp"
#include "lambda.hpp"
#include "mustache.hpp"
#include "node.hpp"
#include "renderer.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if( !condition ) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

mustache::Data * makeStringData(std::string_view value)
{
  std::unique_ptr<mustache::Data> data(new mustache::Data(
      mustache::Data::TypeString, static_cast<int>(value.size())));
  data->val->assign(value.data(), value.size());
  return data.release();
}

void testTemplateViewPreservesEmbeddedNul()
{
  const char source[] = {
      'A', '\0', 'B', '{', '{', 'n', 'a', 'm', 'e', '}', '}', 'C'};
  const char expected[] = {'A', '\0', 'B', 'o', 'k', 'C'};

  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(std::string_view(source, sizeof(source)), &root);

  mustache::Data data(mustache::Data::TypeMap, 0);
  data.data["name"] = makeStringData("ok");
  std::string output;
  mustache.render(&root, &data, NULL, &output);

  expect(output == std::string(expected, sizeof(expected)),
      "string_view template input did not preserve its explicit length");
}

void testExplicitDelimiterLengths()
{
  const char start[] = {'<', '%'};
  const char stop[] = {'%', '>'};
  const char source[] = {
      'A', '<', '%', 'n', 'a', 'm', 'e', '%', '>', 'B'};

  mustache::Mustache mustache;
  mustache.setStartSequence(start, sizeof(start));
  mustache.setStopSequence(stop, sizeof(stop));
  expect(mustache.getStartSequence() == "<%",
      "the legacy start-delimiter length was ignored");
  expect(mustache.getStopSequence() == "%>",
      "the legacy stop-delimiter length was ignored");

  mustache::Node root;
  mustache.tokenize(std::string_view(source, sizeof(source)), &root);
  mustache::Data data(mustache::Data::TypeMap, 0);
  data.data["name"] = makeStringData("ok");
  std::string output;
  mustache.render(&root, &data, NULL, &output);
  expect(output == "AokB",
      "explicitly sized delimiters did not tokenize correctly");

  bool rejected = false;
  try {
    mustache.setStartSequence(std::string_view());
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  expect(rejected, "an empty start delimiter was accepted");

  rejected = false;
  try {
    mustache.setStopSequence(static_cast<const char *>(NULL), 1);
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  expect(rejected, "a null stop delimiter was accepted");
}

class LegacyLambda : public mustache::Lambda {
  public:
    explicit LegacyLambda(std::string * observed) : observed(observed) {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(
        std::string * text, mustache::Renderer *) override
    {
      *observed = *text;
      return *text;
    }

  private:
    std::string * observed;
};

void testLengthAwareLambdaText()
{
  const char source[] = {
      '{', '{', '#', 'c', 'a', 'l', 'l', '}', '}',
      'A', '\0', 'B',
      '{', '{', '/', 'c', 'a', 'l', 'l', '}', '}'};
  const char expected[] = {'A', '\0', 'B'};
  const std::string expectedText(expected, sizeof(expected));
  std::string observed;

  mustache::Data data(mustache::Data::TypeMap, 0);
  std::unique_ptr<mustache::Data> lambdaData(
      new mustache::Data(mustache::Data::TypeLambda, 0));
  lambdaData->lambda = new LegacyLambda(&observed);
  data.data["call"] = lambdaData.release();

  mustache::Mustache mustache;
  mustache::Node root;
  mustache.tokenize(std::string_view(source, sizeof(source)), &root);
  std::string output;
  mustache.render(&root, &data, NULL, &output);

  expect(observed == expectedText,
      "string_view lambda text lost its embedded NUL");
  expect(output == expectedText,
      "lambda output lost its embedded NUL when it was reparsed");

}

void testExplicitSerializedByteLengths()
{
  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  std::unique_ptr<std::vector<uint8_t> > serial(root.serialize());

  size_t position = 0;
  std::unique_ptr<mustache::Node> fromBytes(
      mustache::Node::unserialize(
          serial->data(), serial->size(), 0, &position));
  expect(fromBytes->type == mustache::Node::TypeRoot &&
          position == serial->size(),
      "pointer-plus-length AST decoding failed");

  const std::string binary(
      reinterpret_cast<const char *>(serial->data()), serial->size());
  position = 0;
  std::unique_ptr<mustache::Node> fromView(
      mustache::Node::unserialize(
          std::string_view(binary), 0, &position));
  expect(fromView->type == mustache::Node::TypeRoot &&
          position == binary.size(),
      "string_view AST decoding failed");

  bool rejected = false;
  try {
    std::unique_ptr<mustache::Node> invalid(
        mustache::Node::unserialize(
            serial->data(), serial->size() - 1, 0, &position));
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  expect(rejected, "an explicitly truncated AST byte range was accepted");

  rejected = false;
  try {
    std::unique_ptr<mustache::Node> invalid(
        mustache::Node::unserialize(NULL, 1, 0, &position));
  } catch( const mustache::Exception& ) {
    rejected = true;
  }
  expect(rejected, "a non-empty null AST byte range was accepted");
}

} // namespace

int main()
{
  testTemplateViewPreservesEmbeddedNul();
  testExplicitDelimiterLengths();
  testLengthAwareLambdaText();
  testExplicitSerializedByteLengths();
  return failures == 0 ? 0 : 1;
}
