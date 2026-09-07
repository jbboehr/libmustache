#include "mustache_config.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "compiled_template.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "lambda.hpp"
#include "mustache.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, std::size_t size) noexcept(false);

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
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    for (const auto& bytes :
        {mustache::serializeArchivedTemplate(root), mustache::serializeArchivedTemplate(compiled)}) {
      calls.clear();
      const auto archived = mustache::loadArchivedTemplate(bytes);
      expect(mustache::render(archived, data) == "OK", "archived callback result changed");
      expect(calls == std::vector<std::string>{fixture.body}, "archived callback did not receive exact section bytes");
    }
#endif
  }
}

class ForwardSection final : public mustache::Lambda {
  public:
    using Calls = std::vector<std::pair<std::string, std::string>>;

    explicit ForwardSection(Calls& calls) :
        calls_(calls)
    {}

    std::string invoke() override
    {
      throw std::runtime_error("section callback invoked as a variable");
    }

    std::string invoke(std::string_view text, mustache::LambdaRenderContext context) override
    {
      const mustache::Node label(mustache::Node::TypePartial, "label");
      calls_.emplace_back(text, context.render(label));
      return "[" + std::string(text) + "]";
    }

  private:
    Calls& calls_;
};

void testForwardedSectionInPartial()
{
  struct Fixture {
      const char * name;
      const char * partial;
      const char * body;
      const char * expected;
  };
  const Fixture fixtures[] = {
      {"default delimiters", "{{#wrap}}{{ name }} / {{>label}}{{/wrap}};", "{{ name }} / {{>label}}",
          "ROOT|[Ada / Ada@home];[Bo / Bo@home];|ROOT"},
      {"alternate opening delimiters", "{{=<% %>=}}<%#wrap%><% name %> / <%>label%> / {{name}}<%/wrap%>;",
          "<% name %> / <%>label%> / {{name}}", "ROOT|[Ada / Ada@home / {{name}}];[Bo / Bo@home / {{name}}];|ROOT"},
      {"delimiter change in body", "{{#wrap}}{{ name }} / {{=<% %>=}}<%>label%> / {{name}}<%/wrap%>;",
          "{{ name }} / {{=<% %>=}}<%>label%> / {{name}}",
          "ROOT|[Ada / Ada@home / {{name}}];[Bo / Bo@home / {{name}}];|ROOT"},
  };
  constexpr std::string_view source = "{{name}}|{{#items}}{{>card}}{{/items}}|{{name}}";
  constexpr std::string_view labelSource = "{{name}}@{{site}}";
  const char * expectedLabels[] = {"Ada@home", "Bo@home"};

  for (const auto& fixture : fixtures) {
    ForwardSection::Calls calls;
    auto items = mustache::Data::array();
    items.push_back(mustache::Data::object({{"name", mustache::Data::string("Ada")}}));
    items.push_back(mustache::Data::object({{"name", mustache::Data::string("Bo")}}));
    const auto data =
        mustache::Data::object({{"name", mustache::Data::string("ROOT")}, {"site", mustache::Data::string("home")},
            {"items", std::move(items)}, {"wrap", mustache::Data::lambda(std::make_unique<ForwardSection>(calls))}});

    const auto check = [&](const char * representation, auto&& render) {
      calls.clear();
      const auto checkText = [&](const char * observation, std::string_view expected, const std::string& actual) {
        if (actual != expected) {
          std::fprintf(stderr, "%s (%s), %s:\n  expected: %s\n  actual:   %s\n", fixture.name, representation,
              observation, std::string(expected).c_str(), actual.c_str());
          ++failures;
        }
      };
      try {
        checkText("output", fixture.expected, render());
        if (calls.size() != 2) {
          std::fprintf(
              stderr, "%s (%s): expected 2 section callbacks, got %zu\n", fixture.name, representation, calls.size());
          ++failures;
          return;
        }
        for (std::size_t i = 0; i < calls.size(); ++i) {
          checkText(i == 0 ? "first section text" : "second section text", fixture.body, calls[i].first);
          checkText(i == 0 ? "first callback partial" : "second callback partial", expectedLabels[i], calls[i].second);
        }
      } catch (const std::exception& error) {
        std::fprintf(stderr, "%s (%s) threw: %s\n", fixture.name, representation, error.what());
        ++failures;
      }
    };

    mustache::Mustache engine;
    mustache::Node root;
    engine.tokenize(source, &root);
    mustache::Node::Partials partials;
    for (const auto& entry :
        {std::pair<std::string, std::string_view>{"card", fixture.partial}, {"label", labelSource}}) {
      auto partial = std::make_unique<mustache::Node>();
      engine.tokenize(entry.second, partial.get());
      partials.emplace(entry.first, std::move(partial));
    }
    check("owned", [&]() {
      std::string output;
      engine.render(&root, &data, &partials, &output);
      return output;
    });

    const auto compiled = mustache::compile(source);
    const mustache::PartialMap compiledPartials = {
        {"card", mustache::compile(fixture.partial)}, {"label", mustache::compile(labelSource)}};
    check("compiled", [&]() {
      return mustache::render(compiled, data, compiledPartials);
    });
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    check("archive from owned", [&]() {
      const auto archived = mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root, partials));
      return mustache::render(archived, data);
    });
    check("archive from compiled", [&]() {
      const auto archived =
          mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(compiled, compiledPartials));
      return mustache::render(archived, data);
    });
#endif
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
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
  const auto archived = mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(compiled, partials));
  calls.clear();
  expect(
      mustache::render(archived, data, limits) == "OKOK", "archived partial rejected the exact callback byte budget");
  expect(calls == std::vector<std::string>({"{{ name }}", "{{ name }}"}), "archived partial lost exact source");
#endif
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
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
  calls.clear();
  try {
    static_cast<void>(mustache::render(archived, data, limits));
    expect(false, "archived original section text bypassed the lambda byte budget");
  } catch (const mustache::Exception& error) {
    expect(std::string_view(error.what()) == "Render lambda template byte limit exceeded",
        "archived original text was rejected for an unexpected reason");
  }
  expect(calls.size() == 1, "archived callback ran before its original input passed the byte budget");
#endif

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

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
std::vector<std::uint8_t> archiveAtExactStringBudget(
    const mustache::Node& root, const mustache::Node::Partials& partials, std::size_t stringBytes)
{
  mustache::ArchivedTemplateLimits limits;
  limits.maxNestingDepth = 128;
  limits.maxTotalStringBytes = stringBytes;
  const auto bytes = mustache::serializeArchivedTemplate(root, partials, limits);
  expect(static_cast<bool>(mustache::loadArchivedTemplate(bytes, limits)), "exact archive string budget rejected");

  --limits.maxTotalStringBytes;
  bool rejected = false;
  try {
    static_cast<void>(mustache::serializeArchivedTemplate(root, partials, limits));
  } catch (const mustache::Exception& error) {
    rejected = std::string_view(error.what()) == "Cista archive string byte limit exceeded";
  }
  expect(rejected, "archive writer did not reject a string budget one byte below the stored bytes");

  rejected = false;
  try {
    static_cast<void>(mustache::loadArchivedTemplate(bytes, limits));
  } catch (const mustache::ArchivedTemplateException& error) {
    rejected = error.reason() == mustache::ArchivedTemplateError::LimitExceeded;
  }
  expect(rejected, "archive loader did not reject a string budget one byte below the stored bytes");
  return bytes;
}

void testArchivedSourceLifetimeAndFallback()
{
  std::vector<std::pair<mustache::ArchivedTemplate, std::string>> cases;
  {
    mustache::Node root;
    mustache::Tokenizer tokenizer;
    tokenizer.tokenize("{{#call}}{{ original }}{{/call}}", &root);
    root.children.front()->children.front()->setData("changed");
    cases.emplace_back(mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root)), "{{ original }}");
    root.discardSource();
    cases.emplace_back(mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root)), "{{changed}}");

    tokenizer.tokenize("{{#call}}{{/call}}", &root);
    root.children.front()->children.insert(root.children.front()->children.begin(),
        std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "edited"));
    cases.emplace_back(mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root)), "");
    root.discardSource();
    cases.emplace_back(mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root)), "edited");

    tokenizer.tokenize("{{#call}}{{name}}{{/call}}", &root);
    const auto legacyBytes = root.serializeValue();
    const auto legacy = mustache::Node::unserializeOwned(
        std::string_view(reinterpret_cast<const char *>(legacyBytes.data()), legacyBytes.size()));
    cases.emplace_back(mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(*legacy)), "{{name}}");

    auto partial = std::make_unique<mustache::Node>();
    tokenizer.tokenize("{{#call}}{{ partial }}{{/call}}", partial.get());
    tokenizer.tokenize("{{>piece}}", &root);
    root.partials.emplace("piece", std::move(partial));
    cases.emplace_back(mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root)), "{{ partial }}");
  }
  std::vector<std::string> calls;
  auto data = mustache::Data::object();
  data.set("call", mustache::Data::lambda(std::make_unique<Capture>(calls)));
  for (const auto& entry : cases) {
    calls.clear();
    expect(mustache::render(entry.first, data) == "OK", "archived callback result changed after source destruction");
    expect(calls == std::vector<std::string>{entry.second}, "archived original/absent/empty section text changed");
  }
}

void testArchivedSourceStorage()
{
  constexpr std::size_t bodySize = 64 * 1024;
  for (std::size_t depth : {1U, 16U, 64U}) {
    std::string source;
    for (std::size_t i = 0; i < depth; ++i)
      source += "{{#s}}";
    source += std::string(bodySize, 'x');
    for (std::size_t i = 0; i < depth; ++i)
      source += "{{/s}}";
    mustache::Tokenizer::Limits parseLimits;
    parseLimits.maxNestingDepth = 128;
    mustache::Node root;
    mustache::Tokenizer().tokenize(source, &root, parseLimits);
    // One source buffer (body + 12 bytes per section), one output body,
    // and six bytes of node data/delimiters per section and closing node.
    static_cast<void>(archiveAtExactStringBudget(root, {}, 2 * bodySize + 18 * depth));
  }
}

void testArchivedSharedSourceAcrossPartials()
{
  const std::string source =
      "outside-before|{{#call}}A {{ x }}{{/call}}|outside-middle|{{#call}}B {{ y }}{{/call}}|outside-after";
  mustache::Node parsed;
  mustache::Tokenizer().tokenize(source, &parsed);
  std::vector<std::unique_ptr<mustache::Node>> sections;
  for (auto& child : parsed.children) {
    if (child->type == mustache::Node::TypeSection) {
      sections.push_back(std::move(child));
    }
  }
  expect(sections.size() == 2, "shared-source fixture did not produce two sections");

  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(std::move(sections[0]));
  root.children.push_back(std::make_unique<mustache::Node>(mustache::Node::TypePartial, "piece"));
  auto partial = std::make_unique<mustache::Node>();
  partial->type = mustache::Node::TypeRoot;
  partial->children.push_back(std::move(sections[1]));
  mustache::Node::Partials partials;
  partials.emplace("piece", std::move(partial));

  // The detached sections retain the complete input once. Ordinary node
  // strings use 15 bytes per section, plus five each for the partial tag/name.
  const auto bytes = archiveAtExactStringBudget(root, partials, source.size() + 40);
  std::vector<std::string> calls;
  auto data = mustache::Data::object();
  data.set("call", mustache::Data::lambda(std::make_unique<Capture>(calls)));
  expect(mustache::render(mustache::loadArchivedTemplate(bytes), data) == "OKOK",
      "shared-source archive changed the rendered callback results");
  expect(calls == std::vector<std::string>({"A {{ x }}", "B {{ y }}"}),
      "shared-source archive lost a detached section's exact callback text");
}
#endif

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
    testForwardedSectionInPartial();
    testDeepLegacyWrite();
    testLegacyWriteGuard();
    testSourceOwnershipAndDiscard();
    testPartialsAndBudgets();
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    testArchivedSourceLifetimeAndFallback();
    testArchivedSourceStorage();
    testArchivedSharedSourceAcrossPartials();
#endif
    testTokenizerHarness();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "section source test failed: %s\n", error.what());
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
