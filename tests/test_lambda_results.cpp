#include "mustache_config.h"

#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "mustache.hpp"

namespace {

int failures = 0;

using Result = mustache::LambdaResult;
using Mode = mustache::LambdaStringMode;
using Render = std::function<std::string(const mustache::Data&, Mode, const mustache::RenderLimits&)>;

static_assert(!std::is_convertible_v<std::string, Result>);
static_assert(!std::is_convertible_v<Result, std::string>);

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

template <typename Check>
void forEachRenderer(std::string_view source, const std::map<std::string, std::string>& partialSources, Check check)
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize(source, &root);
  const auto compiled = engine.compile(source);
  mustache::Node::Partials partials;
  mustache::PartialMap compiledPartials;
  for (const auto& partial : partialSources) {
    auto node = std::make_unique<mustache::Node>();
    engine.tokenize(partial.second, node.get());
    partials.emplace(partial.first, std::move(node));
    compiledPartials.emplace(partial.first, engine.compile(partial.second));
  }
  check("node", [&](const mustache::Data& data, Mode mode, const mustache::RenderLimits& limits) {
    engine.setLambdaStringMode(mode);
    std::string output;
    engine.render(&root, &data, &partials, &output, limits);
    return output;
  });
  check("compiled member", [&](const mustache::Data& data, Mode mode, const mustache::RenderLimits& limits) {
    engine.setLambdaStringMode(mode);
    return engine.render(compiled, data, compiledPartials, limits);
  });
  check("compiled free", [&](const mustache::Data& data, Mode mode, const mustache::RenderLimits& limits) {
    return mustache::render(compiled, data, compiledPartials, limits, mode);
  });
  if (partialSources.empty()) {
    check("compiled free without partials",
        [&](const mustache::Data& data, Mode mode, const mustache::RenderLimits& limits) {
          return mustache::render(compiled, data, limits, mode);
        });
  }
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
  for (const auto& bytes : {mustache::serializeArchivedTemplate(root, partials),
           mustache::serializeArchivedTemplate(compiled, compiledPartials)}) {
    const auto archived = mustache::loadArchivedTemplate(bytes);
    check("archived member", [&](const mustache::Data& data, Mode mode, const mustache::RenderLimits& limits) {
      engine.setLambdaStringMode(mode);
      return engine.render(archived, data, limits);
    });
    check("archived free", [&](const mustache::Data& data, Mode mode, const mustache::RenderLimits& limits) {
      return mustache::render(archived, data, limits, mode);
    });
  }
#endif
}

void expectRender(std::string_view source, const mustache::Data& data, Mode mode, const std::string& expected,
    const std::map<std::string, std::string>& partials = {}, const mustache::RenderLimits& limits = {})
{
  forEachRenderer(source, partials, [&](const char * path, const Render& render) {
    try {
      const auto actual = render(data, mode, limits);
      if (actual != expected) {
        std::fprintf(stderr, "%s output mismatch for %.*s\n  expected: %s\n  actual: %s\n", path,
            static_cast<int>(source.size()), source.data(), expected.c_str(), actual.c_str());
        ++failures;
      }
    } catch (const std::exception& error) {
      std::fprintf(stderr, "%s unexpectedly threw: %s\n", path, error.what());
      ++failures;
    }
  });
}

template <typename Action> void expectException(Action action, const char * message)
{
  bool threw = false;
  try {
    action();
  } catch (const mustache::Exception&) {
    threw = true;
  }
  expect(threw, message);
}

class FixedResult final : public mustache::Lambda {
  public:
    explicit FixedResult(Result result) :
        result_(std::move(result))
    {}
    Result invokeResult() override
    {
      return result_;
    }
    Result invokeResult(std::string_view, mustache::LambdaRenderContext) override
    {
      return result_;
    }

  private:
    Result result_;
};

mustache::Data withResult(Result result)
{
  return mustache::Data::object({
      {"name", mustache::Data::string("Ada")},
      {"value", mustache::Data::lambda(std::make_unique<FixedResult>(std::move(result)))},
  });
}

void testResultMatrix()
{
  struct Case {
      Mode mode;
      Result (*result)(std::string);
      const char * escaped;
      const char * unescaped;
  };
  const Case cases[] = {
      {Mode::Template, Result::fromString, "&lt;b&gt;Ada&lt;/b&gt;", "<b>Ada</b>"},
      {Mode::Literal, Result::fromString, "&lt;b&gt;{{name}}&lt;/b&gt;", "<b>{{name}}</b>"},
      {Mode::Template, Result::literal, "&lt;b&gt;{{name}}&lt;/b&gt;", "<b>{{name}}</b>"},
      {Mode::Literal, Result::literal, "&lt;b&gt;{{name}}&lt;/b&gt;", "<b>{{name}}</b>"},
      {Mode::Template, Result::templateSource, "&lt;b&gt;Ada&lt;/b&gt;", "<b>Ada</b>"},
      {Mode::Literal, Result::templateSource, "&lt;b&gt;Ada&lt;/b&gt;", "<b>Ada</b>"},
  };
  for (const auto& test : cases) {
    auto data = withResult(test.result("<b>{{name}}</b>"));
    expectRender("{{value}}", data, test.mode, test.escaped);
    expectRender("{{{value}}}", data, test.mode, test.unescaped);
    expectRender("{{#value}}ignored{{/value}}", data, test.mode, test.unescaped);
    expectRender("{{^value}}not called{{/value}}", data, test.mode, "");
    expectRender("{{>piece}}", data, test.mode, test.escaped, {{"piece", "{{value}}"}});
  }
}

void testTemplateResultsUseCurrentContextAndPartials()
{
  auto data = mustache::Data::object({{"outer",
      mustache::Data::object({{"name", mustache::Data::string("Ada")},
          {"value",
              mustache::Data::lambda(
                  std::make_unique<FixedResult>(Result::templateSource("{{name}}|{{>piece}}")))}})}});
  const std::map<std::string, std::string> partials = {{"piece", "[{{name}}]"}};
  expectRender("{{#outer}}{{value}}{{/outer}}", data, Mode::Literal, "Ada|[Ada]", partials);
  expectRender("{{#outer}}{{#value}}ignored{{/value}}{{/outer}}", data, Mode::Literal, "Ada|[Ada]", partials);
}

void testDelimitersAndBytes()
{
  for (Mode mode : {Mode::Template, Mode::Literal}) {
    const auto evaluated = withResult(Result::templateSource("{{name}}|<%name%>"));
    expectRender("{{=<% %>=}}<%&value%>", evaluated, mode, "Ada|<%name%>");
    expectRender("{{=<% %>=}}<%#value%>body<%/value%>", evaluated, mode, "{{name}}|Ada");
    const auto literal = withResult(Result::literal("{{name}}|<%name%>"));
    expectRender("{{=<% %>=}}<%&value%>", literal, mode, "{{name}}|<%name%>");
    expectRender("{{=<% %>=}}<%#value%>body<%/value%>", literal, mode, "{{name}}|<%name%>");
    for (const std::string& bytes : {std::string(), std::string("é") + '\0' + "{{name}}"}) {
      expectRender("{{value}}", withResult(Result::literal(bytes)), mode, bytes);
      expectRender("{{#value}}body{{/value}}", withResult(Result::literal(bytes)), mode, bytes);
    }
    const std::string source = std::string("é") + '\0' + "{{name}}";
    expectRender("{{value}}", withResult(Result::templateSource(source)), mode, std::string("é") + '\0' + "Ada");
    const auto ordinary = mustache::Data::object({{"value", mustache::Data::string("{{name}}")}});
    expectRender("{{value}}", ordinary, mode, "{{name}}");
  }
}

class LegacySection final : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "{{name}}";
    }
    std::string invoke(std::string * text, mustache::Renderer *) override
    {
      return "[" + *text + "]";
    }
};

class ScopedSection final : public mustache::Lambda {
  public:
    std::string invoke(std::string_view text, mustache::LambdaRenderContext) override
    {
      return "[" + std::string(text) + "]";
    }
};

void testAdapters()
{
  for (bool scoped : {false, true}) {
    auto data = mustache::Data::object({{"name", mustache::Data::string("Ada")}});
    if (scoped) {
      data.set("value", mustache::Data::lambda(std::make_unique<ScopedSection>()));
    } else {
      data.set("value", mustache::Data::lambda(std::make_unique<LegacySection>()));
      expectRender("{{value}}", data, Mode::Template, "Ada");
      expectRender("{{value}}", data, Mode::Literal, "{{name}}");
    }
    expectRender("{{#value}}{{ name }}{{/value}}", data, Mode::Template, "[Ada]");
    expectRender("{{#value}}{{ name }}{{/value}}", data, Mode::Literal, "[{{ name }}]");
  }
  FixedResult resultOnly(Result::literal("text"));
  mustache::Lambda& base = resultOnly;
  expectException(
      [&] {
        static_cast<void>(base.invoke());
      },
      "legacy call discarded the result kind");
  std::string sectionText("body");
  expectException(
      [&] {
        static_cast<void>(base.invoke(&sectionText, nullptr));
      },
      "legacy section call discarded the result kind");
}

class Sequence final : public mustache::Lambda {
  public:
    int calls = 0;
    bool throwNext = false;
    Result invokeResult() override
    {
      if (throwNext) {
        throwNext = false;
        throw std::runtime_error("callback failure");
      }
      ++calls;
      return calls % 2 ? Result::literal("{{name}}") : Result::templateSource("{{name}}");
    }
};

void testRepeatedCallsAndRecovery()
{
  forEachRenderer("{{value}}|{{value}}", {}, [&](const char *, const Render& render) {
    auto callback = std::make_shared<Sequence>();
    auto data = mustache::Data::object(
        {{"name", mustache::Data::string("Ada")}, {"value", mustache::Data::sharedLambda(callback)}});
    for (Mode mode : {Mode::Literal, Mode::Template}) {
      callback->calls = 0;
      callback->throwNext = true;
      bool threw = false;
      try {
        static_cast<void>(render(data, mode, {}));
      } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "callback failure";
      }
      expect(threw, "ordinary callback exception did not propagate");
      expect(render(data, mode, {}) == "{{name}}|Ada", "renderer reuse changed explicit results");
      expect(callback->calls == 2, "callback results were cached or invoked too often");
    }
  });
}

class NestedResult final : public mustache::Lambda {
  public:
    mustache::LambdaRenderContext retained;
    bool throwNext = false;
    Result invokeResult(std::string_view, mustache::LambdaRenderContext context) override
    {
      retained = context;
      if (throwNext) {
        throwNext = false;
        throw std::runtime_error("section failure");
      }
      const mustache::Node partial(mustache::Node::TypePartial, "piece");
      return Result::literal(context.render(partial));
    }
};

void testNestedModeAndLifetime()
{
  forEachRenderer("{{#value}}body{{/value}}", {{"piece", "{{inner}}"}}, [&](const char *, const Render& render) {
    auto callback = std::make_shared<NestedResult>();
    auto data = mustache::Data::object(
        {{"name", mustache::Data::string("Ada")}, {"value", mustache::Data::sharedLambda(callback)},
            {"inner", mustache::Data::lambda(std::make_unique<LegacySection>())}});
    for (Mode mode : {Mode::Literal, Mode::Template}) {
      callback->throwNext = true;
      bool threw = false;
      try {
        static_cast<void>(render(data, mode, {}));
      } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "section failure";
      }
      expect(threw, "section exception did not propagate");
      expect(!callback->retained.active(), "throwing result callback retained an active context");
      expect(render(data, mode, {}) == (mode == Mode::Literal ? "{{name}}" : "Ada"),
          "nested rendering lost the configured string mode");
      expect(!callback->retained.active(), "successful result callback retained an active context");
      const mustache::Node text(mustache::Node::TypeOutput, "text");
      expectException(
          [&] {
            static_cast<void>(callback->retained.render(text));
          },
          "retained result callback context remained usable");
    }
  });
}

void testNestedRenderingSharesBudgets()
{
  forEachRenderer("{{#value}}body{{/value}}", {{"piece", "{{inner}}"}}, [&](const char *, const Render& render) {
    auto callback = std::make_shared<NestedResult>();
    auto data = mustache::Data::object(
        {{"name", mustache::Data::string("Ada")}, {"value", mustache::Data::sharedLambda(callback)},
            {"inner", mustache::Data::lambda(std::make_unique<LegacySection>())}});

    mustache::RenderLimits limits;
    limits.maxLambdaTemplateBytes = 4;
    limits.maxOutputBytes = 16;
    expect(render(data, Mode::Literal, limits) == "{{name}}", "nested literal rendering rejected exact shared budgets");
    limits.maxOutputBytes = 15;
    expectException(
        [&] {
          static_cast<void>(render(data, Mode::Literal, limits));
        },
        "nested literal rendering reset the shared output budget");

    limits.maxOutputBytes = 6;
    limits.maxLambdaTemplateBytes = 12;
    expect(render(data, Mode::Template, limits) == "Ada", "nested template rendering rejected exact shared budgets");
    limits.maxLambdaTemplateBytes = 11;
    expectException(
        [&] {
          static_cast<void>(render(data, Mode::Template, limits));
        },
        "nested template rendering reset the shared lambda byte budget");
  });
}

void testBudgets()
{
  mustache::RenderLimits limits;
  limits.maxLambdaTemplateBytes = 0;
  limits.maxOutputBytes = 3;
  expectRender("{{value}}", withResult(Result::literal("abc")), Mode::Template, "abc", {}, limits);
  expectRender("{{value}}", withResult(Result::fromString("abc")), Mode::Literal, "abc", {}, limits);
  forEachRenderer("{{value}}", {}, [&](const char *, const Render& render) {
    expectException(
        [&] {
          static_cast<void>(render(withResult(Result::templateSource("abc")), Mode::Literal, limits));
        },
        "explicit template bypassed parsing budget in literal mode");
    expectException(
        [&] {
          static_cast<void>(render(withResult(Result::literal("abcd")), Mode::Template, limits));
        },
        "literal result bypassed output budget");
    expectException(
        [&] {
          static_cast<void>(render(withResult(Result::literal("<")), Mode::Literal, limits));
        },
        "literal escape expansion bypassed output budget");
  });
  limits.maxOutputBytes = 4;
  expectRender("{{value}}", withResult(Result::literal("<")), Mode::Literal, "&lt;", {}, limits);
  limits.maxLambdaTemplateBytes = 1;
  expectRender("{{#value}}X{{/value}}", withResult(Result::literal("abc")), Mode::Template, "abc", {}, limits);
  limits.maxLambdaTemplateBytes = 0;
  forEachRenderer("{{#value}}X{{/value}}", {}, [&](const char *, const Render& render) {
    expectException(
        [&] {
          static_cast<void>(render(withResult(Result::literal("abc")), Mode::Literal, limits));
        },
        "literal mode stopped charging section source");
  });

  limits = mustache::RenderLimits();
  limits.maxNodeVisits = 2;
  expectRender("{{value}}", withResult(Result::literal("{{!comment}}")), Mode::Template, "{{!comment}}", {}, limits);
  forEachRenderer("{{value}}", {}, [&](const char *, const Render& render) {
    expectException(
        [&] {
          static_cast<void>(render(withResult(Result::templateSource("{{!comment}}")), Mode::Literal, limits));
        },
        "explicit template result bypassed the shared node budget");
  });
}

class ModeMutation final : public mustache::Lambda {
  public:
    std::string invoke(std::string *, mustache::Renderer * renderer) override
    {
      expectException(
          [&] {
            renderer->setLambdaStringMode(Mode::Literal);
          },
          "active renderer allowed mode mutation");
      return "ok";
    }
};

void testConfiguration()
{
  mustache::Mustache engine;
  expect(engine.getLambdaStringMode() == Mode::Template, "default mode is not template evaluation");
  engine.setLambdaStringMode(Mode::Literal);
  engine.renderer.clear();
  expect(engine.getLambdaStringMode() == Mode::Literal, "clear discarded configured string mode");
  expectException(
      [&] {
        engine.setLambdaStringMode(static_cast<Mode>(42));
      },
      "invalid string mode was accepted");
  expect(engine.getLambdaStringMode() == Mode::Literal, "invalid mode changed configuration");
  auto data = mustache::Data::object({{"value", mustache::Data::lambda(std::make_unique<ModeMutation>())}});
  expectRender("{{#value}}body{{/value}}", data, Mode::Template, "ok");
}

class LegacyText final : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "<b>{{name}}</b>";
    }
};

class ReconfigureOwner final : public mustache::Lambda {
  public:
    explicit ReconfigureOwner(mustache::Mustache * owner) :
        owner_(owner)
    {}
    Result invokeResult() override
    {
      owner_->setLambdaStringMode(Mode::Literal);
      return Result::fromString("{{name}}");
    }

  private:
    mustache::Mustache * owner_;
};

void testMemberRenderCapturesModeAtEntry()
{
  mustache::Mustache engine;
  const auto compiled = engine.compile("{{value}}");
  auto callback = std::make_shared<ReconfigureOwner>(&engine);
  const auto data = mustache::Data::object(
      {{"name", mustache::Data::string("Ada")}, {"value", mustache::Data::sharedLambda(callback)}});

  engine.setLambdaStringMode(Mode::Template);
  expect(engine.render(compiled, data) == "Ada", "compiled member render observed a later owner mode change");
  expect(engine.getLambdaStringMode() == Mode::Literal, "compiled callback could not reconfigure the idle owner");
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
  const auto archived = mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(compiled));
  engine.setLambdaStringMode(Mode::Template);
  expect(engine.render(archived, data) == "Ada", "archived member render observed a later owner mode change");
  expect(engine.getLambdaStringMode() == Mode::Literal, "archived callback could not reconfigure the idle owner");
#endif
}

void testLegacyLiteralMode()
{
  mustache::Mustache engine;
  engine.setLambdaStringMode(mustache::LambdaStringMode::Literal);
  auto data = mustache::Data::object({
      {"name", mustache::Data::string("Ada")},
      {"value", mustache::Data::lambda(std::make_unique<LegacyText>())},
  });
  mustache::Node root;
  engine.tokenize("{{value}}", &root);
  std::string output;
  engine.render(&root, &data, nullptr, &output);
  expect(output == "&lt;b&gt;{{name}}&lt;/b&gt;", "node renderer reparsed a literal-mode legacy result");
  const auto compiled = engine.compile("{{value}}");
  expect(engine.render(compiled, data) == "&lt;b&gt;{{name}}&lt;/b&gt;",
      "compiled renderer reparsed a literal-mode legacy result");
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
  const auto archived = mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(compiled));
  expect(engine.render(archived, data) == "&lt;b&gt;{{name}}&lt;/b&gt;",
      "archived renderer reparsed a literal-mode legacy result");
#endif
}

} // namespace

int main()
{
  testLegacyLiteralMode();
  testResultMatrix();
  testTemplateResultsUseCurrentContextAndPartials();
  testDelimitersAndBytes();
  testAdapters();
  testRepeatedCallsAndRecovery();
  testNestedModeAndLifetime();
  testNestedRenderingSharesBudgets();
  testBudgets();
  testConfiguration();
  testMemberRenderCapturesModeAtEntry();
  return failures == 0 ? 0 : 1;
}
