#include "mustache_config.h"

#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "data.hpp"
#include "node.hpp"
#include "renderer.hpp"

static_assert(std::is_copy_constructible<mustache::Data>::value,
    "mustache::Data must be safely copy constructible");
static_assert(std::is_copy_assignable<mustache::Data>::value,
    "mustache::Data must be safely copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::Data>::value,
    "mustache::Data must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::Data>::value,
    "mustache::Data must be nothrow move assignable");
static_assert(!std::is_copy_constructible<mustache::Node>::value,
    "mustache::Node must not be copy constructible");
static_assert(!std::is_copy_assignable<mustache::Node>::value,
    "mustache::Node must not be copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::Node>::value,
    "mustache::Node must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::Node>::value,
    "mustache::Node must be nothrow move assignable");
static_assert(std::is_same<mustache::Node::Children::value_type,
        std::unique_ptr<mustache::Node> >::value,
    "mustache::Node children must have explicit ownership");
static_assert(std::is_same<mustache::Node::Partials::mapped_type,
        std::unique_ptr<mustache::Node> >::value,
    "mustache::Node partials must have explicit ownership");

namespace {

int failures = 0;

class CountingLambda : public mustache::Lambda {
  public:
    explicit CountingLambda(int * destructions) : destructions(destructions) {}

    ~CountingLambda() override
    {
      ++*destructions;
    }

    std::string invoke() override
    {
      return "lambda";
    }

    std::string invoke(std::string *, mustache::Renderer *) override
    {
      return "lambda";
    }

  private:
    int * destructions;
};

void expect(bool condition, const char * message)
{
  if( !condition ) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

void testDataCopyAndMoveConstruction()
{
  mustache::Data source(mustache::Data::TypeMap, 0);
  source.set("name", mustache::Data::string("libmustache"));
  mustache::Data values = mustache::Data::array();
  values.push_back(mustache::Data::string("rock hard"));
  source.set("values", std::move(values));

  mustache::Data copy(source);
  source.set("name", mustache::Data::string("changed"));
  const mustache::Data * copiedName = copy.find("name");
  expect(copiedName != NULL && copiedName->stringValue() == "libmustache",
      "Data copy construction did not deeply copy object values");

  mustache::Data moved(std::move(source));

  expect(source.type() == mustache::Data::TypeNone,
      "Data move construction did not reset the source type");
  expect(moved.type() == mustache::Data::TypeMap,
      "Data move construction did not transfer the type");
  expect(moved.objectItems().size() == 2,
      "Data move construction did not transfer map entries");
  const mustache::Data * name = moved.find("name");
  expect(name != NULL && name->stringValue() == "changed",
      "Data move construction did not preserve a string value");
  const mustache::Data * nested = moved.find("values");
  expect(nested != NULL && nested->arrayItems().size() == 1 &&
          nested->arrayItems().front().stringValue() == "rock hard",
      "Data move construction did not preserve nested ownership");
}

void testDataMoveAssignment()
{
  mustache::Data source(mustache::Data::TypeList, 0);
  source.push_back(mustache::Data::string("new"));

  mustache::Data destination = mustache::Data::string("old");
  destination = std::move(source);

  expect(source.type() == mustache::Data::TypeNone,
      "Data move assignment did not reset the source type");
  expect(destination.type() == mustache::Data::TypeList,
      "Data move assignment did not transfer the type");
  expect(destination.listItems().size() == 1 &&
          destination.listItems().front().stringValue() == "new",
      "Data move assignment did not transfer list ownership");
}

void testDataLambdaOwnership()
{
  int destructions = 0;
  {
    mustache::Data source = mustache::Data::lambda(
        std::make_unique<CountingLambda>(&destructions));
    mustache::Data shared(source);
    expect(source.lambdaValue() == shared.lambdaValue(),
        "Data copying did not explicitly share lambda ownership");
    expect(shared.lambdaValue()->invoke() == "lambda",
        "a copied Data lambda could not be invoked");

    mustache::Data destination = mustache::Data::lambda(
        std::make_unique<CountingLambda>(&destructions));

    destination = std::move(source);

    expect(destructions == 1,
        "Data move assignment did not destroy the destination lambda");
    expect(source.type() == mustache::Data::TypeNone &&
            source.lambdaValue() == NULL,
        "Data lambda move assignment did not reset the source");
    expect(destination.type() == mustache::Data::TypeLambda &&
            destination.lambdaValue() != NULL &&
            destination.lambdaValue()->invoke() == "lambda",
        "Data lambda move assignment did not transfer ownership");
  }
  expect(destructions == 2,
      "Data lambda sharing did not destroy each callback exactly once");
}

void testNodeMoveConstruction()
{
  mustache::Node source(
      mustache::Node::TypeSection, "person.name", mustache::Node::FlagEscape);
  source.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeVariable, "value"));
  source.child = std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, "container child");
  source.partials.emplace("card",
      std::make_unique<mustache::Node>(
          mustache::Node::TypeVariable, "title"));
  source.startSequence = "<%";
  source.stopSequence = "%>";

  mustache::Node moved(std::move(source));

  expect(source.type == mustache::Node::TypeNone,
      "Node move construction did not reset the source type");
  expect(!source.data.has_value() && source.dataParts.empty() &&
          source.children.empty() && source.child == NULL &&
          source.partials.empty() && !source.startSequence.has_value() &&
          !source.stopSequence.has_value(),
      "Node move construction left owned state in the source");
  expect(moved.type == mustache::Node::TypeSection &&
          moved.flags == mustache::Node::FlagEscape,
      "Node move construction did not transfer scalar state");
  expect(moved.data.has_value() && *moved.data == "person.name" &&
          moved.dataParts.size() == 2,
      "Node move construction did not transfer data state");
  expect(moved.children.size() == 1 && moved.child != NULL &&
          moved.child->data.has_value() &&
          *moved.child->data == "container child",
      "Node move construction did not transfer child ownership");
  mustache::Node::Partials::iterator partial = moved.partials.find("card");
  expect(partial != moved.partials.end() && partial->second != NULL &&
          partial->second->data.has_value() &&
          *partial->second->data == "title",
      "Node move construction did not transfer internal partials");
  expect(moved.startSequence.has_value() &&
          moved.stopSequence.has_value() &&
          *moved.startSequence == "<%" && *moved.stopSequence == "%>",
      "Node move construction did not transfer delimiters");
}

void testNodeMoveAssignment()
{
  mustache::Node source(mustache::Node::TypeVariable, "new.value");
  source.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeComment, "child"));

  mustache::Node destination(mustache::Node::TypeVariable, "old.value");
  destination.children.push_back(std::make_unique<mustache::Node>(
      mustache::Node::TypeComment, "old child"));
  destination.partials.emplace("old", std::make_unique<mustache::Node>(
      mustache::Node::TypeVariable, "old partial"));
  destination = std::move(source);

  expect(source.type == mustache::Node::TypeNone &&
          !source.data.has_value() && source.dataParts.empty() &&
          source.children.empty(),
      "Node move assignment did not reset the source");
  expect(destination.type == mustache::Node::TypeVariable &&
          destination.data.has_value() && *destination.data == "new.value",
      "Node move assignment did not transfer data");
  expect(destination.children.size() == 1 &&
          *destination.children.front()->data == "child",
      "Node move assignment did not replace owned children");
  expect(destination.partials.empty(),
      "Node move assignment retained the destination's old partials");
}

void testOwnedNodeRendering()
{
  mustache::Data data(mustache::Data::TypeString, 0);
  mustache::Renderer renderer;
  std::string output;

  mustache::Node container;
  container.type = mustache::Node::TypeContainer;
  container.child = std::make_unique<mustache::Node>(
      mustache::Node::TypeOutput, "owned child");
  renderer.init(&container, &data, NULL, &output);
  renderer.render();
  expect(output == "owned child",
      "Renderer did not follow the owned container child");

  mustache::Node invalidRoot;
  invalidRoot.type = mustache::Node::TypeRoot;
  invalidRoot.children.push_back(std::unique_ptr<mustache::Node>());
  output.clear();
  renderer.init(&invalidRoot, &data, NULL, &output);
  bool rejected = false;
  try {
    renderer.render();
  } catch( const mustache::Exception& exception ) {
    rejected = std::string(exception.what()).find("Empty tree node") !=
        std::string::npos;
  }
  expect(rejected, "Renderer did not reject a null owned child safely");
}

} // namespace

int main()
{
  testDataCopyAndMoveConstruction();
  testDataMoveAssignment();
  testDataLambdaOwnership();
  testNodeMoveConstruction();
  testNodeMoveAssignment();
  testOwnedNodeRendering();
  return failures == 0 ? 0 : 1;
}
