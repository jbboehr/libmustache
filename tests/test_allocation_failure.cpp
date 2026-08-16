#include "mustache_config.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "compiled_template.hpp"
#include "data.hpp"
#include "lambda.hpp"
#include "mustache.hpp"
#include "node.hpp"
#include "tokenizer.hpp"

namespace allocation_failure_test {

thread_local bool enabled = false;
thread_local bool failed = false;
thread_local std::size_t allocationsBeforeFailure = 0;

void arm(std::size_t allocations) noexcept
{
  enabled = true;
  failed = false;
  allocationsBeforeFailure = allocations;
}

bool shouldFail() noexcept
{
  if (!enabled) {
    return false;
  }
  if (allocationsBeforeFailure != 0) {
    --allocationsBeforeFailure;
    return false;
  }
  enabled = false;
  failed = true;
  return true;
}

bool disarm() noexcept
{
  const bool result = failed;
  enabled = false;
  failed = false;
  return result;
}

} // namespace allocation_failure_test

// GCC -Wmismatched-new-delete does not treat a replacement operator new that
// calls malloc as pairing with operator delete that calls free. After inlining
// into std::make_unique it reports a false positive under -Werror.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void * operator new(std::size_t size)
{
  if (allocation_failure_test::shouldFail()) {
    throw std::bad_alloc();
  }
  if (void * memory = std::malloc(size == 0 ? 1 : size)) {
    return memory;
  }
  throw std::bad_alloc();
}

void * operator new[](std::size_t size)
{
  return ::operator new(size);
}

void operator delete(void * memory) noexcept
{
  std::free(memory);
}

void operator delete[](void * memory) noexcept
{
  std::free(memory);
}

void operator delete(void * memory, std::size_t) noexcept
{
  std::free(memory);
}

void operator delete[](void * memory, std::size_t) noexcept
{
  std::free(memory);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

constexpr std::size_t maxAllocationAttempts = 4096;
constexpr std::string_view tokenizationSource = "head\n{{#show}}\n{{name}}\n{{/show}}\n{{>card}}\n{{=<% %>=}}<%tail%>";
constexpr std::string_view jsonInput = R"({"name":"Ada","enabled":true,"items":[1,{"nested":"value"},3.50]})";
constexpr std::string_view yamlInput = "name: Ada\nenabled: true\nitems:\n  - 1\n  - nested: value\n  - 3.50\n";
int failures = 0;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

template <typename Setup, typename Operation, typename FailureCheck, typename SuccessCheck>
void exerciseAllocationFailures(
    const char * name, Setup setup, Operation operation, FailureCheck failureCheck, SuccessCheck successCheck)
{
  bool observedFailure = false;

  for (std::size_t failAt = 0; failAt < maxAllocationAttempts; ++failAt) {
    auto state = setup();
    allocation_failure_test::arm(failAt);

    try {
      operation(state);
    } catch (const std::exception& exception) {
      const bool injected = allocation_failure_test::disarm();
      if (!injected) {
        std::fprintf(stderr, "%s threw without an injected allocation failure at attempt %zu: %s\n", name, failAt,
            exception.what());
        ++failures;
        return;
      }
      observedFailure = true;
      failureCheck(state);
      continue;
    } catch (...) {
      const bool injected = allocation_failure_test::disarm();
      std::fprintf(stderr, "%s threw a non-standard exception at attempt %zu%s\n", name, failAt,
          injected ? " after an injected allocation failure" : "");
      ++failures;
      return;
    }

    const bool injected = allocation_failure_test::disarm();
    if (injected) {
      std::fprintf(stderr, "%s ignored an injected allocation failure at attempt %zu\n", name, failAt);
      ++failures;
      return;
    }

    expect(observedFailure, "an allocation-failure sweep reached success without exercising a failure");
    successCheck(state);
    return;
  }

  std::fprintf(stderr, "%s did not complete within %zu allocation attempts\n", name, maxAllocationAttempts);
  ++failures;
}

struct TokenizationCase {
    TokenizationCase()
    {
      root.type = mustache::Node::TypeSection;
      root.flags = mustache::Node::FlagEscape;
      root.setData("original.path");
      root.children.push_back(std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "original child"));
      originalChild = root.children.front().get();
      root.child = std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "container child");
      originalContainerChild = root.child.get();
      root.partials.emplace("preserved", std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "partial body"));
      root.startSequence = "<%";
      root.stopSequence = "%>";
    }

    mustache::Node root;
    mustache::Node * originalChild = nullptr;
    mustache::Node * originalContainerChild = nullptr;
};

bool tokenizationDestinationPreserved(const TokenizationCase& state)
{
  return state.root.type == mustache::Node::TypeSection && state.root.flags == mustache::Node::FlagEscape &&
      state.root.data.has_value() && *state.root.data == "original.path" && state.root.dataParts.size() == 2 &&
      state.root.children.size() == 1 && state.root.children.front().get() == state.originalChild &&
      state.root.child.get() == state.originalContainerChild && state.root.partials.size() == 1 &&
      state.root.startSequence.has_value() && *state.root.startSequence == "<%" &&
      state.root.stopSequence.has_value() && *state.root.stopSequence == "%>";
}

void testTokenizationFailureIsTransactional()
{
  exerciseAllocationFailures(
      "tokenization",
      []() {
        return TokenizationCase();
      },
      [](TokenizationCase& state) {
        mustache::Tokenizer tokenizer;
        tokenizer.tokenize(tokenizationSource, &state.root);
      },
      [](const TokenizationCase& state) {
        expect(tokenizationDestinationPreserved(state), "allocation failure changed the tokenization destination");
      },
      [](const TokenizationCase& state) {
        expect(state.root.type == mustache::Node::TypeRoot && !state.root.children.empty() &&
                state.root.partials.find("preserved") != state.root.partials.end(),
            "tokenization did not complete after the allocation-failure sweep");
      });
}

struct DataCase {
    DataCase()
    {
      destination.set("sentinel", mustache::Data::string("preserved"));
    }

    mustache::Data destination = mustache::Data::object();
};

bool dataDestinationPreserved(const DataCase& state)
{
  const mustache::Data * sentinel = state.destination.find("sentinel");
  return sentinel != nullptr && sentinel->type() == mustache::Data::TypeString &&
      sentinel->stringValue() == "preserved" && state.destination.objectItems().size() == 1;
}

bool parsedDataIsComplete(const DataCase& state)
{
  const mustache::Data * name = state.destination.find("name");
  const mustache::Data * enabled = state.destination.find("enabled");
  const mustache::Data * items = state.destination.find("items");
  return name != nullptr && name->type() == mustache::Data::TypeString && name->stringValue() == "Ada" &&
      enabled != nullptr && enabled->type() == mustache::Data::TypeBoolean && enabled->booleanValue() &&
      items != nullptr && items->type() == mustache::Data::TypeArray && items->arrayItems().size() == 3;
}

void testJSONFailureDoesNotPublishPartialData()
{
  exerciseAllocationFailures(
      "JSON parsing",
      []() {
        return DataCase();
      },
      [](DataCase& state) {
        mustache::Data parsed = mustache::Data::fromJSON(jsonInput);
        state.destination = std::move(parsed);
      },
      [](const DataCase& state) {
        expect(dataDestinationPreserved(state), "allocation failure published partial JSON data");
      },
      [](const DataCase& state) {
        expect(parsedDataIsComplete(state), "JSON parsing did not complete after the allocation-failure sweep");
      });
}

void testYAMLFailureDoesNotPublishPartialData()
{
  exerciseAllocationFailures(
      "YAML parsing",
      []() {
        return DataCase();
      },
      [](DataCase& state) {
        mustache::Data parsed = mustache::Data::fromYAML(yamlInput);
        state.destination = std::move(parsed);
      },
      [](const DataCase& state) {
        expect(dataDestinationPreserved(state), "allocation failure published partial YAML data");
      },
      [](const DataCase& state) {
        const mustache::Data * name = state.destination.find("name");
        const mustache::Data * items = state.destination.find("items");
        expect(name != nullptr && name->stringValue() == "Ada" && items != nullptr &&
                items->type() == mustache::Data::TypeArray && items->arrayItems().size() == 3,
            "YAML parsing did not complete after the allocation-failure sweep");
      });
}

mustache::Node makeSerializableTree()
{
  mustache::Node root;
  mustache::Tokenizer tokenizer;
  tokenizer.tokenize("before {{#items}}{{name}}={{value}};{{/items}} after", &root);
  return root;
}

struct SerializationCase {
    mustache::Node root = makeSerializableTree();
    std::vector<uint8_t> output = {0xde, 0xad, 0xbe, 0xef};
};

bool serializationDestinationPreserved(const SerializationCase& state)
{
  return state.output.size() == 4 && state.output[0] == 0xde && state.output[1] == 0xad && state.output[2] == 0xbe &&
      state.output[3] == 0xef;
}

void testSerializationFailureDoesNotPublishPartialBytes()
{
  exerciseAllocationFailures(
      "AST serialization",
      []() {
        return SerializationCase();
      },
      [](SerializationCase& state) {
        std::vector<uint8_t> serialized = state.root.serializeValue();
        state.output = std::move(serialized);
      },
      [](const SerializationCase& state) {
        expect(serializationDestinationPreserved(state), "allocation failure published partial serialized bytes");
      },
      [](const SerializationCase& state) {
        expect(state.output.size() > 2 && state.output[0] == 'M' && state.output[1] == 'U',
            "AST serialization did not complete after the allocation-failure sweep");
      });
}

struct DeserializationCase {
    DeserializationCase() :
        serial(makeSerializableTree().serializeValue()),
        destination(std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "preserved")),
        original(destination.get())
    {}

    std::vector<uint8_t> serial;
    std::unique_ptr<mustache::Node> destination;
    mustache::Node * original;
};

void testDeserializationFailureDoesNotPublishPartialTree()
{
  exerciseAllocationFailures(
      "AST deserialization",
      []() {
        return DeserializationCase();
      },
      [](DeserializationCase& state) {
        const std::string_view bytes(reinterpret_cast<const char *>(state.serial.data()), state.serial.size());
        std::unique_ptr<mustache::Node> decoded = mustache::Node::unserializeOwned(bytes);
        state.destination = std::move(decoded);
      },
      [](const DeserializationCase& state) {
        expect(state.destination.get() == state.original && state.destination->type == mustache::Node::TypeOutput &&
                state.destination->data.has_value() && *state.destination->data == "preserved",
            "allocation failure published a partial deserialized tree");
      },
      [](const DeserializationCase& state) {
        expect(state.destination.get() != state.original && state.destination->type == mustache::Node::TypeRoot &&
                !state.destination->children.empty(),
            "AST deserialization did not complete after the allocation-failure sweep");
      });
}

class AllocationLambda : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "{{name}}";
    }

    std::string invoke(std::string_view, mustache::LambdaRenderContext) override
    {
      return "{{name}}";
    }
};

struct RenderingCase {
    RenderingCase() :
        compiled(mustache::compile("Hello {{name}} {{#callback}}ignored{{/callback}} {{>card}}"))
    {
      partials.emplace("card", mustache::compile("[{{name}}]"));
      data.set("name", mustache::Data::string("Ada"));
      data.set("callback", mustache::Data::lambda(std::make_unique<AllocationLambda>()));
    }

    mustache::CompiledTemplate compiled;
    mustache::PartialMap partials;
    mustache::Data data = mustache::Data::object();
    std::string output = "preserved";
};

constexpr std::string_view expectedRender = "Hello Ada Ada [Ada]";

void testRenderingFailureDoesNotPublishPartialOutput()
{
  exerciseAllocationFailures(
      "compiled rendering",
      []() {
        return RenderingCase();
      },
      [](RenderingCase& state) {
        std::string rendered = mustache::render(state.compiled, state.data, state.partials);
        state.output = std::move(rendered);
      },
      [](const RenderingCase& state) {
        expect(state.output == "preserved", "allocation failure published partial compiled render output");
        expect(std::string_view(mustache::render(state.compiled, state.data, state.partials)) == expectedRender,
            "compiled templates were not reusable after allocation failure");
      },
      [](const RenderingCase& state) {
        expect(std::string_view(state.output) == expectedRender,
            "compiled rendering did not complete after the allocation-failure sweep");
      });
}

} // namespace

int main()
{
  testTokenizationFailureIsTransactional();
  testJSONFailureDoesNotPublishPartialData();
  testYAMLFailureDoesNotPublishPartialData();
  testSerializationFailureDoesNotPublishPartialBytes();
  testDeserializationFailureDoesNotPublishPartialTree();
  testRenderingFailureDoesNotPublishPartialOutput();
  return failures == 0 ? 0 : 1;
}
