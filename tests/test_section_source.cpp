#include "mustache_config.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "compiled_template.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "lambda.hpp"
#include "mustache.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, std::size_t size);

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

class Capture final : public mustache::Lambda {
  public:
    explicit Capture(std::vector<std::string>& calls) :
        calls_(calls)
    {}
    std::string invoke() override
    {
      return "";
    }
    std::string invoke(std::string_view text, mustache::LambdaRenderContext) override
    {
      calls_.emplace_back(text);
      return "OK";
    }

  private:
    std::vector<std::string>& calls_;
};

void testExactCallbackBytes()
{
  struct Fixture {
      std::string source;
      std::string body;
  };
  const Fixture fixtures[] = {
      {"{{#call}}A {{ name }} B {{{name}}} C{{/call}}", "A {{ name }} B {{{name}}} C"},
      {"{{#call}}{{! spaced comment }}{{> piece }}{{/call}}", "{{! spaced comment }}{{> piece }}"},
      {"{{#call}}a{{=<% %>=}}<% name %><%/call%>", "a{{=<% %>=}}<% name %>"},
      {"{{=<% %>=}}<%#call%><% name %><%/call%>", "<% name %>"},
      {"  {{#call}} \r\nbody\r\n\t{{/call}}\r\n", " \r\nbody\r\n\t"},
      {"{{#call}}\n{{#inner}}x{{/inner}}\n{{/call}}", "\n{{#inner}}x{{/inner}}\n"},
      {"{{#call}}{{/call}}", ""},
      {"{{{#call}}}{{/call}}", ""},
      {"{{{#call}}}x{{/call}}", "x"},
      {"{{{#call}}}}x{{/call}}", "}x"},
      {"{{={{ %>=}}{{{#call%>}x{{/call%>", "}x"},
      {std::string("{{#call}}A") + '\0' + "{{ name }}{{/call}}", std::string("A") + '\0' + "{{ name }}"},
  };
  for (const auto& fixture : fixtures) {
    mustache::Mustache engine;
    mustache::Node root;
    engine.tokenize(fixture.source, &root);
    const auto compiled = mustache::compile(fixture.source);
    std::vector<std::string> calls;
    auto data = mustache::Data::object();
    data.set("call", mustache::Data::lambda(std::make_unique<Capture>(calls)));
    std::string output;
    engine.render(&root, &data, nullptr, &output);
    expect(output == "OK", "owned callback result changed");
    expect(calls == std::vector<std::string>{fixture.body}, "owned callback did not receive exact section bytes");
    calls.clear();
    expect(mustache::render(compiled, data) == "OK", "compiled callback result changed");
    expect(calls == std::vector<std::string>{fixture.body}, "compiled callback did not receive exact section bytes");
  }
}

void testLegacyWriteGuard()
{
  mustache::Tokenizer tokenizer;
  const char * lossy[] = {"{{#call}}{{ name }}{{/call}}", "{{#call}}{{{name}}}{{/call}}",
      "{{#outer}}{{#call}}{{! spaced }}{{/call}}{{/outer}}", "{{#call}}{{=<% %>=}}text<%/call%>"};
  for (const auto * source : lossy) {
    mustache::Node root;
    tokenizer.tokenize(source, &root);
    for (int api = 0; api < 4; ++api) {
      bool rejected = false;
      std::vector<uint8_t> destination{42};
      try {
        if (api == 0) {
          destination = root.serializeValue();
        } else if (api == 1) {
          destination = root.serializeValue(mustache::Node::SerializationLimits());
        } else if (api == 2) {
          std::unique_ptr<std::vector<uint8_t>> result(root.serialize());
        } else {
          std::unique_ptr<std::vector<uint8_t>> result(root.serialize(mustache::Node::SerializationLimits()));
        }
      } catch (const mustache::Exception& error) {
        rejected = std::string_view(error.what()) == "Legacy serialization cannot preserve original section text";
      }
      expect(rejected, "legacy writer silently discarded original section text");
      expect(destination == std::vector<uint8_t>{42}, "rejected legacy write changed the caller's destination");
    }
  }

  // Exact reconstruction remains representable, including empty sections.
  for (const char * source : {"{{#call}}{{name}}{{&name}}{{/call}}", "{{#call}}\nbody\n{{/call}}\n",
           "{{#call}}{{/call}}", "{{{#call}}}{{/call}}", "{{{#call}}}x{{/call}}"}) {
    mustache::Node root;
    tokenizer.tokenize(source, &root);
    const auto bytes = root.serializeValue();
    const auto decoded =
        mustache::Node::unserializeOwned(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
    expect(decoded->serializeValue() == bytes, "representable legacy source no longer round-trips");
    expect(!decoded->children.back()->originalSectionText(), "legacy decoder claimed to recover original source");
  }
}

void testDeepLegacyWrite()
{
  std::string source;
  for (int depth = 0; depth < 256; ++depth) {
    source += "{{#s}}";
  }
  for (int depth = 0; depth < 256; ++depth) {
    source += "{{/s}}";
  }
  mustache::Tokenizer tokenizer;
  mustache::Tokenizer::Limits parseLimits;
  parseLimits.maxNestingDepth = 256;
  mustache::Node root;
  tokenizer.tokenize(source, &root, parseLimits);

  mustache::Node::SerializationLimits limits;
  limits.maxNestingDepth = 258; // Root, 256 sections, and the innermost stop.
  try {
    const auto bytes = root.serializeValue(limits);
    const auto decoded = mustache::Node::unserializeOwned(
        std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()), limits);
    expect(decoded->serializeValue(limits) == bytes, "deep representable legacy source no longer round-trips");
    const std::unique_ptr<std::vector<uint8_t>> pointerBytes(root.serialize(limits));
    expect(*pointerBytes == bytes, "pointer legacy writer rejected deep representable source");
  } catch (const mustache::Exception& error) {
    std::fprintf(stderr, "deep legacy write rejected within serialization limits: %s\n", error.what());
    ++failures;
  }

  limits.maxNestingDepth = 257;
  try {
    static_cast<void>(root.serializeValue(limits));
    expect(false, "legacy writer bypassed its explicit nesting limit");
  } catch (const mustache::Exception& error) {
    expect(std::string_view(error.what()) == "Serial node nesting limit exceeded",
        "deep legacy write was rejected for an unexpected reason");
  }
}

void testSourceOwnershipAndDiscard()
{
  mustache::Node section;
  mustache::Mustache engine;
  {
    std::string source = "{{#outer}}before{{#call}}{{ name }}{{/call}}after{{/outer}}";
    mustache::Node root;
    engine.tokenize(source, &root);
    section = std::move(*root.children.at(0)->children.at(1));
    root.discardSource();
    expect(!root.children.front()->originalSectionText(), "discardSource left source on the former parent");
    source.assign("changed");
  }
  mustache::Node moved(std::move(section));
  expect(!section.originalSectionText(), "moved-from node retained section metadata");
  expect(moved.originalSectionText() == std::optional<std::string_view>("{{ name }}"),
      "detached section lost its source ownership");
  std::vector<std::string> calls;
  auto data = mustache::Data::object();
  data.set("call", mustache::Data::lambda(std::make_unique<Capture>(calls)));
  moved.children.at(0)->setData("changed");
  std::string output;
  engine.render(&moved, &data, nullptr, &output);
  expect(calls == std::vector<std::string>{"{{ name }}"}, "an AST edit changed the original callback text");
  moved.discardSource();
  calls.clear();
  output.clear();
  engine.render(&moved, &data, nullptr, &output);
  expect(calls == std::vector<std::string>{"{{changed}}"}, "discarding source did not enable reconstruction");
  static_cast<void>(moved.serializeValue());

  mustache::Node root;
  engine.tokenize("{{#call}}{{/call}}", &root);
  expect(root.children.front()->originalSectionText().has_value(), "empty section source was treated as absent");
  root.children.front()->children.insert(
      root.children.front()->children.begin(), std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "edited"));
  calls.clear();
  output.clear();
  engine.render(&root, &data, nullptr, &output);
  expect(calls == std::vector<std::string>{""}, "empty original source fell back to the edited AST");

  const auto oldBody = root.children.front()->originalSectionText();
  try {
    engine.tokenize("{{#unclosed}}", &root);
    expect(false, "unclosed section unexpectedly parsed");
  } catch (const mustache::TokenizerException&) {
    expect(root.children.front()->originalSectionText() == oldBody, "failed tokenization replaced section source");
  }
  root.discardSource();
  calls.clear();
  output.clear();
  engine.render(&root, &data, nullptr, &output);
  expect(calls == std::vector<std::string>{"edited"}, "discardSource did not reach a child section");
}

void testPartialsAndBudgets()
{
  std::vector<std::string> calls;
  auto data = mustache::Data::object();
  data.set("call", mustache::Data::lambda(std::make_unique<Capture>(calls)));
  mustache::PartialMap partials;
  {
    const std::string source = "{{#call}}{{ name }}{{/call}}";
    partials.emplace("piece", mustache::compile(source));
  }
  const auto compiled = mustache::compile("{{>piece}}{{>piece}}");
  mustache::RenderLimits limits;
  limits.maxLambdaTemplateBytes = 24; // Two 10-byte inputs and two "OK" results.
  expect(mustache::render(compiled, data, partials, limits) == "OKOK", "exact callback byte budget was rejected");
  expect(calls == std::vector<std::string>({"{{ name }}", "{{ name }}"}), "compiled partial lost exact source");
  calls.clear();
  limits.maxLambdaTemplateBytes = 21; // First call consumes 12; second input does not fit.
  try {
    static_cast<void>(mustache::render(compiled, data, partials, limits));
    expect(false, "original section text bypassed the lambda byte budget");
  } catch (const mustache::Exception& error) {
    expect(std::string_view(error.what()) == "Render lambda template byte limit exceeded",
        "original text was rejected for an unexpected reason");
  }
  expect(calls.size() == 1, "callback ran before its original input passed the byte budget");

  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{>piece}}", &root);
  auto partial = std::make_unique<mustache::Node>();
  engine.tokenize("{{#call}}{{ name }}{{/call}}", partial.get());
  root.partials.emplace("piece", std::move(partial));
  calls.clear();
  std::string output;
  engine.render(&root, &data, nullptr, &output);
  expect(calls == std::vector<std::string>{"{{ name }}"}, "owned partial lost exact source");
  root.discardSource();
  calls.clear();
  output.clear();
  engine.render(&root, &data, nullptr, &output);
  expect(calls == std::vector<std::string>{"{{name}}"}, "discardSource did not reach owned partials");
}

void testTokenizerHarness()
{
  constexpr std::string_view source = "{{#call}}{{ name }}{{/call}}";
  expect(LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t *>(source.data()), source.size()) == 0,
      "tokenizer harness did not handle original-text rejection");
}

} // namespace

int main()
{
  try {
    testExactCallbackBytes();
    testDeepLegacyWrite();
    testLegacyWriteGuard();
    testSourceOwnershipAndDiscard();
    testPartialsAndBudgets();
    testTokenizerHarness();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "section source test failed: %s\n", error.what());
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
