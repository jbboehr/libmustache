#include "mustache_config.h"

#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

#include "data.hpp"
#include "node.hpp"

static_assert(!std::is_copy_constructible<mustache::Data>::value,
    "mustache::Data must not be copy constructible");
static_assert(!std::is_copy_assignable<mustache::Data>::value,
    "mustache::Data must not be copy assignable");
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

mustache::Data * makeStringData(const std::string& value)
{
  mustache::Data * data = new mustache::Data(
      mustache::Data::TypeString, static_cast<int>(value.size()));
  data->val->assign(value);
  return data;
}

void testDataMoveConstruction()
{
  mustache::Data source(mustache::Data::TypeMap, 0);
  source.data["name"] = makeStringData("libmustache");
  mustache::Data * values = new mustache::Data(mustache::Data::TypeArray, 1);
  values->array.push_back(makeStringData("rock hard"));
  source.data["values"] = values;

  mustache::Data moved(std::move(source));

  expect(source.type == mustache::Data::TypeNone,
      "Data move construction did not reset the source type");
  expect(source.data.empty(),
      "Data move construction left owned map entries in the source");
  expect(moved.type == mustache::Data::TypeMap,
      "Data move construction did not transfer the type");
  expect(moved.data.size() == 2,
      "Data move construction did not transfer map entries");
  mustache::Data::Map::iterator name = moved.data.find("name");
  expect(name != moved.data.end() && name->second != NULL &&
          name->second->val != NULL && *name->second->val == "libmustache",
      "Data move construction did not preserve a string value");
  mustache::Data::Map::iterator nested = moved.data.find("values");
  expect(nested != moved.data.end() && nested->second != NULL &&
          nested->second->array.size() == 1 &&
          nested->second->array.front() != NULL &&
          nested->second->array.front()->val != NULL &&
          *nested->second->array.front()->val == "rock hard",
      "Data move construction did not preserve nested ownership");
}

void testDataMoveAssignment()
{
  mustache::Data source(mustache::Data::TypeList, 0);
  source.children.push_back(makeStringData("new"));

  mustache::Data destination(mustache::Data::TypeString, 3);
  destination.val->assign("old");
  destination = std::move(source);

  expect(source.type == mustache::Data::TypeNone,
      "Data move assignment did not reset the source type");
  expect(source.children.empty(),
      "Data move assignment left owned list entries in the source");
  expect(destination.type == mustache::Data::TypeList,
      "Data move assignment did not transfer the type");
  expect(destination.children.size() == 1 &&
          *destination.children.front()->val == "new",
      "Data move assignment did not transfer list ownership");
}

void testDataLambdaMoveAssignment()
{
  int destructions = 0;
  {
    mustache::Data source(mustache::Data::TypeLambda, 0);
    source.lambda = new CountingLambda(&destructions);
    mustache::Data destination(mustache::Data::TypeLambda, 0);
    destination.lambda = new CountingLambda(&destructions);

    destination = std::move(source);

    expect(destructions == 1,
        "Data move assignment did not destroy the destination lambda");
    expect(source.type == mustache::Data::TypeNone && source.lambda == NULL,
        "Data lambda move assignment did not reset the source");
    expect(destination.type == mustache::Data::TypeLambda &&
            destination.lambda != NULL &&
            destination.lambda->invoke() == "lambda",
        "Data lambda move assignment did not transfer ownership");
  }
  expect(destructions == 2,
      "Data lambda move assignment did not destroy the transferred lambda");
}

void testNodeMoveConstruction()
{
  mustache::Node source(
      mustache::Node::TypeSection, "person.name", mustache::Node::FlagEscape);
  source.children.push_back(
      new mustache::Node(mustache::Node::TypeVariable, "value"));
  source.child = source.children.front();
  source.partials.emplace("card",
      mustache::Node(mustache::Node::TypeVariable, "title"));
  source.startSequence = new std::string("<%");
  source.stopSequence = new std::string("%>");

  mustache::Node moved(std::move(source));

  expect(source.type == mustache::Node::TypeNone,
      "Node move construction did not reset the source type");
  expect(source.data == NULL && source.dataParts == NULL &&
          source.children.empty() && source.child == NULL &&
          source.partials.empty() && source.startSequence == NULL &&
          source.stopSequence == NULL,
      "Node move construction left owned or borrowed state in the source");
  expect(moved.type == mustache::Node::TypeSection &&
          moved.flags == mustache::Node::FlagEscape,
      "Node move construction did not transfer scalar state");
  expect(moved.data != NULL && *moved.data == "person.name" &&
          moved.dataParts != NULL && moved.dataParts->size() == 2,
      "Node move construction did not transfer data state");
  expect(moved.children.size() == 1 && moved.child == moved.children.front(),
      "Node move construction did not preserve child relationships");
  mustache::Node::Partials::iterator partial = moved.partials.find("card");
  expect(partial != moved.partials.end() && partial->second.data != NULL &&
          *partial->second.data == "title",
      "Node move construction did not transfer internal partials");
  expect(moved.startSequence != NULL && moved.stopSequence != NULL &&
          *moved.startSequence == "<%" && *moved.stopSequence == "%>",
      "Node move construction did not transfer delimiters");
}

void testNodeMoveAssignment()
{
  mustache::Node source(mustache::Node::TypeVariable, "new.value");
  source.children.push_back(
      new mustache::Node(mustache::Node::TypeComment, "child"));

  mustache::Node destination(mustache::Node::TypeVariable, "old.value");
  destination.children.push_back(
      new mustache::Node(mustache::Node::TypeComment, "old child"));
  destination.partials.emplace("old", mustache::Node(
      mustache::Node::TypeVariable, "old partial"));
  destination = std::move(source);

  expect(source.type == mustache::Node::TypeNone && source.data == NULL &&
          source.dataParts == NULL && source.children.empty(),
      "Node move assignment did not reset the source");
  expect(destination.type == mustache::Node::TypeVariable &&
          destination.data != NULL && *destination.data == "new.value",
      "Node move assignment did not transfer data");
  expect(destination.children.size() == 1 &&
          *destination.children.front()->data == "child",
      "Node move assignment did not replace owned children");
  expect(destination.partials.empty(),
      "Node move assignment retained the destination's old partials");
}

} // namespace

int main()
{
  testDataMoveConstruction();
  testDataMoveAssignment();
  testDataLambdaMoveAssignment();
  testNodeMoveConstruction();
  testNodeMoveAssignment();
  return failures == 0 ? 0 : 1;
}
