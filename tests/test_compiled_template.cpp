#include "mustache_config.h"

#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "compiled_template.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "lambda.hpp"
#include "mustache.hpp"
#include "render_engine.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

mustache::Data makeStringData(std::string_view value)
{
  return mustache::Data::string(std::string(value));
}

void testHandleOwnershipAndReuse()
{
  mustache::CompiledTemplate original;
  expect(original.empty() && !original, "a default compiled template was not empty");

  {
    std::string source("Hello {{name}}");
    original = mustache::compile(source);
    source.assign("destroyed");
  }

  mustache::CompiledTemplate copy(original);
  mustache::CompiledTemplate moved(std::move(original));
  expect(original.empty(), "a moved-from compiled template was not empty");
  expect(copy && moved, "copying or moving lost the compiled template");

  mustache::Data first(mustache::Data::TypeMap, 0);
  first.set("name", makeStringData("Ada"));
  mustache::Data second(mustache::Data::TypeMap, 0);
  second.set("name", makeStringData("Grace"));
  const mustache::Data copiedData(first);

  expect(mustache::render(copy, copiedData) == "Hello Ada", "a copied template did not own its parsed source");
  expect(mustache::render(moved, second) == "Hello Grace", "an immutable template could not be reused with new data");
}

void testMemberConfigurationAndLimits()
{
  mustache::Mustache engine;
  engine.setStartSequence("<%");
  engine.setStopSequence("%>");
  mustache::CompiledTemplate compiled = engine.compile("<%name%>");

  mustache::Data data(mustache::Data::TypeMap, 0);
  data.set("name", makeStringData("configured"));
  expect(engine.render(compiled, data) == "configured", "member compilation ignored configured delimiters");

  mustache::Tokenizer::Limits limits;
  limits.maxInputBytes = 2;
  bool rejected = false;
  try {
    engine.compile("abc", limits);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "compiled templates ignored explicit parser limits");
  expect(engine.render(compiled, data) == "configured", "failed compilation damaged an existing compiled template");
}

void testCompiledPartials()
{
  mustache::CompiledTemplate root = mustache::compile("{{>outer}}|{{>missing}}");
  mustache::PartialMap partials;
  partials.emplace("outer", mustache::compile("A{{>inner}}B"));
  partials.emplace("inner", mustache::compile("{{name}}"));

  mustache::Data data(mustache::Data::TypeMap, 0);
  data.set("name", makeStringData("rock"));
  expect(mustache::render(root, data, partials) == "ArockB|", "compiled partial lookup or nesting failed");

  partials.emplace("empty", mustache::CompiledTemplate());
  mustache::CompiledTemplate referencesEmpty = mustache::compile("{{>empty}}");
  bool rejected = false;
  try {
    mustache::render(referencesEmpty, data, partials);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "a referenced empty compiled partial was accepted");
}

void testPartialOwnershipScope()
{
  // Keep application owners alive: the assertions detect missing retention
  // without releasing memory that a borrowed node might still reference.
  const auto outer = std::make_shared<mustache::Node>();
  const auto inner = std::make_shared<mustache::Node>();
  const mustache::detail::OwnedPartialSource::Resolver resolver =
      [&](const std::string& name) -> std::shared_ptr<const mustache::Node> {
    if (name == "outer") {
      return outer;
    }
    if (name == "inner") {
      return inner;
    }
    return {};
  };
  const mustache::detail::OwnedPartialSource source(&resolver, nullptr, nullptr);
  const auto outerName = mustache::detail::RenderString::fromView("outer");
  const auto innerName = mustache::detail::RenderString::fromView("inner");

  const bool found = source.withPartial(outerName, [&](mustache::detail::OwnedNodeView) {
    expect(outer.use_count() > 1, "the resolver did not retain the active partial");
    expect(inner.use_count() == 1, "an unused partial was retained");
    const bool nested = source.withPartial(innerName, [&](mustache::detail::OwnedNodeView) {
      expect(outer.use_count() > 1, "a nested lookup released the outer partial");
      expect(inner.use_count() > 1, "a nested lookup did not retain its partial");
    });
    expect(nested, "a nested partial was not found");
    expect(inner.use_count() == 1, "a completed nested partial was retained");
  });
  expect(found, "an existing partial was not found");
  expect(outer.use_count() == 1, "a completed partial was retained");

  bool propagated = false;
  try {
    source.withPartial(outerName, [&](mustache::detail::OwnedNodeView) {
      expect(outer.use_count() > 1, "a throwing callback did not retain its partial");
      throw std::runtime_error("partial callback failed");
    });
  } catch (const std::runtime_error&) {
    propagated = true;
  }
  expect(propagated, "a partial callback exception was swallowed");
  expect(outer.use_count() == 1, "a partial was retained after a callback exception");

  const bool missing =
      source.withPartial(mustache::detail::RenderString::fromView("missing"), [](mustache::detail::OwnedNodeView) {
        expect(false, "a missing partial invoked its callback");
      });
  expect(!missing, "a missing partial was reported as found");
}

class CallbackLambda final : public mustache::Lambda {
  public:
    explicit CallbackLambda(std::function<void()> callback) :
        callback_(std::move(callback))
    {}

    std::string invoke() override
    {
      callback_();
      return "";
    }

  private:
    std::function<void()> callback_;
};

void testPartialLookupDuringCallback()
{
  const auto compiled = mustache::compile("{{add}}{{>late}}");
  mustache::PartialMap partials;
  auto add = std::make_unique<CallbackLambda>([&]() {
    partials.emplace("late", mustache::compile("added"));
  });
  const auto data = mustache::Data::object({{"add", mustache::Data::lambda(std::move(add))}});
  expect(mustache::render(compiled, data, partials) == "added",
      "a partial added by a callback was not visible to the next lookup");
}

void testPartialReplacementAndErasureDuringCallback()
{
  const auto root = mustache::compile("{{>piece}}|{{>piece}}");
  // These owners stay alive throughout each render. This checks live lookup
  // behavior through the public API, not last-owner lifetime handling.
  const auto original = mustache::compile("{{update}}original");
  const auto replacement = mustache::compile("replacement");
  for (const bool erase : {false, true}) {
    mustache::PartialMap partials{{"piece", original}};
    auto update = std::make_unique<CallbackLambda>([&]() {
      if (erase) {
        partials.erase("piece");
      } else {
        partials.at("piece") = replacement;
      }
    });
    const auto data = mustache::Data::object({{"update", mustache::Data::lambda(std::move(update))}});
    const auto output = mustache::render(root, data, partials);
    expect(output == (erase ? "original|" : "original|replacement"),
        erase ? "an erased partial was still visible to the next lookup"
              : "a replacement partial was not visible to the next lookup");
  }
}

void testStandalonePartialIndentation()
{
  mustache::Data data = mustache::Data::object({{"content", mustache::Data::string("<\n->")}});
  mustache::PartialMap partials;
  partials.emplace("partial", mustache::compile("|\n{{{content}}}\n|\n"));

  const mustache::CompiledTemplate canonical = mustache::compile("\\\n {{>partial}}\n/\n");
  expect(mustache::render(canonical, data, partials) == "\\\n |\n <\n->\n |\n/\n",
      "partial indentation was applied to dynamic value line endings");

  partials.emplace("outer", mustache::compile("A\n {{>inner}}\nB\n"));
  partials.emplace("inner", mustache::compile("X\nY\n"));
  const mustache::CompiledTemplate nested = mustache::compile("  {{>outer}}\n");
  expect(mustache::render(nested, data, partials) == "  A\n   X\n   Y\n  B\n",
      "nested standalone partial indentation did not compose");

  partials.emplace("crlf", mustache::compile(">\r\n>"));
  expect(mustache::render(mustache::compile("|\r\n\t{{>crlf}}\r\n|"), data, partials) == "|\r\n\t>\r\n\t>|",
      "standalone partial indentation changed CRLF line endings");

  partials.emplace("inline", mustache::compile(">\n>"));
  expect(mustache::render(mustache::compile("  x{{>inline}}\n"), data, partials) == "  x>\n>\n",
      "inline partials incorrectly inherited line indentation");
  expect(mustache::render(mustache::compile("before\n  {{>missing}}\nafter"), data, partials) == "before\nafter",
      "a missing standalone partial left its source line behind");
}

void testEmbeddedNulAndEmptyHandle()
{
  const char source[] = {'A', '\0', 'B', '{', '{', 'n', 'a', 'm', 'e', '}', '}'};
  const char expected[] = {'A', '\0', 'B', 'o', 'k'};
  mustache::CompiledTemplate compiled = mustache::compile(std::string_view(source, sizeof(source)));

  mustache::Data data(mustache::Data::TypeMap, 0);
  data.set("name", makeStringData("ok"));
  expect(mustache::render(compiled, data) == std::string(expected, sizeof(expected)),
      "compiled template input lost its explicit length");

  bool rejected = false;
  try {
    mustache::render(mustache::CompiledTemplate(), data);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "an empty compiled template was rendered");
}

} // namespace

static_assert(
    std::is_copy_constructible<mustache::CompiledTemplate>::value, "compiled templates must be copy constructible");
static_assert(std::is_copy_assignable<mustache::CompiledTemplate>::value, "compiled templates must be copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::CompiledTemplate>::value,
    "compiled templates must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::CompiledTemplate>::value,
    "compiled templates must be nothrow move assignable");

int main()
{
  testHandleOwnershipAndReuse();
  testMemberConfigurationAndLimits();
  testCompiledPartials();
  testPartialOwnershipScope();
  testPartialLookupDuringCallback();
  testPartialReplacementAndErasureDuringCallback();
  testStandalonePartialIndentation();
  testEmbeddedNulAndEmptyHandle();
  return failures == 0 ? 0 : 1;
}
