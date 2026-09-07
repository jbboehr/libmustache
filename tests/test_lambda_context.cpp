#include "mustache_config.h"

#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "mustache.hpp"

namespace {

int failures = 0;

using Result = mustache::LambdaResult;
using Mode = mustache::LambdaStringMode;
using Render = std::function<std::string(const mustache::Data&, Mode, const mustache::RenderLimits&)>;

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

using Context = mustache::LambdaRenderContext;

class Helper final : public mustache::Lambda {
  public:
    using Callback = std::function<Result(std::string_view, Context)>;
    explicit Helper(Callback callback) :
        callback_(std::move(callback))
    {}
    Result invokeResult(std::string_view body, Context context) override
    {
      return callback_(body, std::move(context));
    }

  private:
    Callback callback_;
};

mustache::Data helper(Helper::Callback callback)
{
  return mustache::Data::lambda(std::make_unique<Helper>(std::move(callback)));
}

void testLiteralResultsAndComposition()
{
  mustache::Node name(mustache::Node::TypeVariable, "name", mustache::Node::FlagEscape);
  for (bool useNode : {false, true}) {
    for (bool compose : {false, true}) {
      auto data = mustache::Data::object(
          {{"name", mustache::Data::string("{{other}}&")}, {"other", mustache::Data::string("Ada")},
              {"wrap", helper([&](std::string_view body, Context context) {
                 auto result = useNode ? context.renderResult(name) : context.renderTemplate(body);
                 expect(result.kind() == Result::Kind::Literal, "helper must return an explicit literal result");
                 return compose ? Result::literal("<b>" + result.text() + "</b>") : result;
               })}});
      for (Mode mode : {Mode::Template, Mode::Literal}) {
        expectRender("{{#wrap}}{{name}}{{/wrap}}", data, mode, compose ? "<b>{{other}}&amp;</b>" : "{{other}}&amp;");
      }
    }
  }
}

void testCurrentContextAndPartials()
{
  mustache::Tokenizer tokenizer;
  tokenizer.setStartSequence("[[");
  tokenizer.setStopSequence("]]");
  mustache::Node node;
  tokenizer.tokenize("[[name]]|[[>piece]]|[[parent]]", &node);
  for (bool useNode : {false, true}) {
    auto data = mustache::Data::object({{"parent", mustache::Data::string("P")},
        {"person",
            mustache::Data::object({{"name", mustache::Data::string("<Ada>")},
                {"wrap", helper([&](std::string_view body, Context context) {
                   return useNode ? context.renderResult(node) : context.renderTemplate(body);
                 })}})}});
    for (Mode mode : {Mode::Template, Mode::Literal}) {
      expectRender("{{#person}}{{=<% %>=}}<%#wrap%><%name%>|<%>piece%>|<%parent%><%/wrap%><%/person%>", data, mode,
          "&lt;Ada&gt;|[<Ada>]|P", {{"piece", "[{{{name}}}]"}});
    }
  }
}

void testNestedDelimiterFrames()
{
  Context outer;
  Context inner;
  auto data = mustache::Data::object({{"name", mustache::Data::string("Ada")},
      {"outer", helper([&](std::string_view body, Context context) {
         outer = context;
         auto result = context.renderTemplate(body);
         expect(!inner.active(), "inner context must expire while outer context remains active");
         return Result::literal(result.text() + "|" + context.renderTemplate("<%name%>").text());
       })},
      {"inner", helper([&](std::string_view body, Context context) {
         inner = context;
         return Result::literal(context.renderTemplate(body).text() + ":" + outer.renderTemplate("<%name%>").text());
       })}});
  for (Mode mode : {Mode::Template, Mode::Literal}) {
    expectRender(
        "{{=<% %>=}}<%#outer%><%=[[ ]]=%>[[#inner]][[name]][[/inner]][[=<% %>=]]<%/outer%>", data, mode, "Ada:Ada|Ada");
    expect(!outer.active() && !inner.active(), "all callback frames must expire after rendering");
  }
}

void testNestedIndependentRenderers()
{
  Context outer;
  Context inner;
  const auto innerTemplate = mustache::compile("{{=[[ ]]=}}[[#inner]][[/inner]]");
  auto innerData = mustache::Data::object(
      {{"name", mustache::Data::string("inner")}, {"inner", helper([&](std::string_view, Context context) {
                                                     inner = context;
                                                     return context.renderTemplate("[[name]]");
                                                   })}});
  auto outerData = mustache::Data::object({{"name", mustache::Data::string("outer")},
      {"outer", helper([&](std::string_view, Context context) {
         outer = context;
         const std::string nested = mustache::render(innerTemplate, innerData);
         expect(nested == "inner", "nested renderer lost its callback delimiters");
         expect(!inner.active() && outer.active(), "nested renderer changed the wrong callback lifetime");
         return Result::literal(nested + "|" + outer.renderTemplate("<%name%>").text());
       })}});
  for (Mode mode : {Mode::Template, Mode::Literal}) {
    expectRender("{{=<% %>=}}<%#outer%><%/outer%>", outerData, mode, "inner|outer");
    expect(!outer.active() && !inner.active(), "independent nested callback frames remained active");
  }
}

void testOwnedBytesAndInactiveContexts()
{
  Context retained;
  Result saved = Result::literal("");
  mustache::Node empty;
  expectException(
      [&]() {
        (void)retained.renderResult(empty);
      },
      "default context accepted renderResult");
  expectException(
      [&]() {
        (void)retained.renderTemplate("");
      },
      "default context accepted renderTemplate");
  for (const auto& bytes : {std::string(), std::string("é") + '\0' + "{{other}}"}) {
    auto data = mustache::Data::object({{"name", mustache::Data::string(bytes)},
        {"other", mustache::Data::string("Ada")}, {"wrap", helper([&](std::string_view body, Context context) {
                                                     retained = context;
                                                     saved = context.renderTemplate(body);
                                                     return saved;
                                                   })}});
    expectRender("{{#wrap}}{{name}}{{/wrap}}", data, Mode::Template, bytes);
    expect(saved.kind() == Result::Kind::Literal && saved.text() == bytes, "result must own its rendered bytes");
    expect(!retained.active(), "retained helper context remained active");
    expectException(
        [&]() {
          (void)retained.renderResult(empty);
        },
        "expired context accepted renderResult");
    expectException(
        [&]() {
          (void)retained.renderTemplate("");
        },
        "expired context accepted renderTemplate");
  }
  auto data = mustache::Data::object({{"wrap", helper([](std::string_view body, Context context) {
                                         return context.renderTemplate(body);
                                       })}});
  expectRender("{{#wrap}}{{/wrap}}", data, Mode::Template, "");
  const std::string bytes = std::string("é") + '\0' + "!";
  expectRender("{{#wrap}}" + bytes + "{{/wrap}}", data, Mode::Template, bytes);
}

void testBudgets()
{
  mustache::Node output(mustache::Node::TypeOutput, "x");
  for (bool useNode : {false, true}) {
    auto data = mustache::Data::object({{"wrap", helper([&](std::string_view, Context context) {
                                           return useNode ? context.renderResult(output) : context.renderTemplate("x");
                                         })}});
    forEachRenderer("{{#wrap}}{{/wrap}}", {}, [&](const char *, const Render& render) {
      for (Mode mode : {Mode::Template, Mode::Literal}) {
        mustache::RenderLimits exact;
        exact.maxLambdaTemplateBytes = useNode ? 0 : 1;
        exact.maxOutputBytes = 2; // Intermediate x and its final append.
        exact.maxNodeVisits = useNode ? 3 : 6; // Outer root + section; helper node, or parse + visit root/output.
        expect(render(data, mode, exact) == "x", "exact helper budgets rejected valid output");
        auto shortOutput = exact;
        --shortOutput.maxOutputBytes;
        expectException(
            [&]() {
              (void)render(data, mode, shortOutput);
            },
            "helper output work escaped the budget");
        auto shortNodes = exact;
        --shortNodes.maxNodeVisits;
        expectException(
            [&]() {
              (void)render(data, mode, shortNodes);
            },
            "helper node work escaped the budget");
        if (!useNode) {
          auto shortSource = exact;
          --shortSource.maxLambdaTemplateBytes;
          expectException(
              [&]() {
                (void)render(data, mode, shortSource);
              },
              "helper parsing escaped the byte budget");
        }
        expect(render(data, mode, exact) == "x", "renderer did not recover after helper limit failure");
      }
    });
  }
  auto data = mustache::Data::object({{"wrap", helper([](std::string_view body, Context context) {
                                         return context.renderTemplate(body);
                                       })}});
  forEachRenderer("{{#wrap}}x{{/wrap}}", {}, [&](const char *, const Render& render) {
    mustache::RenderLimits limits;
    limits.maxLambdaTemplateBytes = 2; // Body delivery and explicit helper parsing are distinct work.
    expect(render(data, Mode::Template, limits) == "x", "body/helper byte budget rejected exact boundary");
    limits.maxLambdaTemplateBytes = 1;
    expectException(
        [&]() {
          (void)render(data, Mode::Template, limits);
        },
        "rendering the body must charge parsing");
  });
}

void testFailedParsingConsumesSharedBudget()
{
  auto data = mustache::Data::object({{"wrap", helper([](std::string_view, Context context) {
                                         expectException(
                                             [&]() {
                                               (void)context.renderTemplate("{{#bad}}");
                                             },
                                             "invalid helper source was accepted");
                                         return context.renderTemplate("x");
                                       })}});
  forEachRenderer("{{#wrap}}{{/wrap}}", {}, [&](const char *, const Render& render) {
    mustache::RenderLimits exact;
    exact.maxLambdaTemplateBytes = 9; // Failed parse (8) and subsequent valid parse (1).
    exact.maxOutputBytes = 2; // Intermediate x and its final append.
    exact.maxNodeVisits = 6; // Outer 2; valid helper parse 2 and traversal 2.
    expect(render(data, Mode::Template, exact) == "x", "failed helper parsing rejected the exact shared budget");
    auto shortSource = exact;
    --shortSource.maxLambdaTemplateBytes;
    expectException(
        [&]() {
          (void)render(data, Mode::Template, shortSource);
        },
        "failed helper parsing did not consume the shared source budget");
    expect(render(data, Mode::Template, exact) == "x", "failed helper parsing corrupted renderer reuse");
  });
}

void testExceptionsAndRecovery()
{
  Context retained;
  bool throwNext = true;
  auto data = mustache::Data::object(
      {{"name", mustache::Data::string("Ada")}, {"wrap", helper([&](std::string_view body, Context context) {
                                                   retained = context;
                                                   if (throwNext) {
                                                     (void)context.renderTemplate(body);
                                                     throw std::runtime_error("callback failure");
                                                   }
                                                   // A parsing failure caught inside a callback must leave its context usable.
                                                   expectException(
                                                       [&]() {
                                                         (void)context.renderTemplate("{{#broken}}");
                                                       },
                                                       "invalid helper source accepted");
                                                   return context.renderTemplate(body);
                                                 })}});
  forEachRenderer("before{{#wrap}}{{name}}{{/wrap}}after", {}, [&](const char *, const Render& render) {
    throwNext = true;
    bool threw = false;
    try {
      (void)render(data, Mode::Template, {});
    } catch (const std::runtime_error&) {
      threw = true;
    }
    expect(threw && !retained.active(), "callback failure must invalidate the helper context");
    expectException(
        [&]() {
          (void)retained.renderTemplate("x");
        },
        "exception retained an active helper context");
    throwNext = false;
    expect(render(data, Mode::Template, {}) == "beforeAdaafter", "helper exception corrupted the parent render");
  });
}

void testNestedBudgets()
{
  auto renderBody = [](std::string_view body, Context context) {
    return context.renderTemplate(body);
  };
  auto data = mustache::Data::object({{"outer", helper(renderBody)}, {"inner", helper(renderBody)}});
  forEachRenderer("{{#outer}}{{#inner}}x{{/inner}}{{/outer}}", {}, [&](const char *, const Render& render) {
    for (Mode mode : {Mode::Template, Mode::Literal}) {
      mustache::RenderLimits exact;
      exact.maxLambdaTemplateBytes = 44; // Deliver + parse: outer 21 + 21, inner 1 + 1.
      exact.maxOutputBytes = 3; // Inner helper, inner result, outer result.
      exact.maxNodeVisits = 12; // Outer 2; outer helper parse 4 (including stop) + visit 2; inner parse 2 + visit 2.
      exact.maxNestingDepth = 6; // Outer root/section, helper root/inner section, inner helper root/output.
      expect(render(data, mode, exact) == "x", "nested helpers rejected exact shared budgets");
      for (auto member : {&mustache::RenderLimits::maxLambdaTemplateBytes, &mustache::RenderLimits::maxOutputBytes,
               &mustache::RenderLimits::maxNodeVisits, &mustache::RenderLimits::maxNestingDepth}) {
        auto shortLimit = exact;
        --(shortLimit.*member);
        expectException(
            [&]() {
              (void)render(data, mode, shortLimit);
            },
            "nested helpers reset a shared budget");
      }
      expect(render(data, mode, exact) == "x", "nested helper limits corrupted the renderer");
    }
  });
}

class OrdinaryString final : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "{{name}}";
    }
};

void testHelperInheritsMode()
{
  mustache::Node node;
  mustache::Tokenizer().tokenize("{{inner}}", &node);
  for (bool useNode : {false, true}) {
    auto data = mustache::Data::object(
        {{"name", mustache::Data::string("Ada")}, {"inner", mustache::Data::lambda(std::make_unique<OrdinaryString>())},
            {"outer", helper([&](std::string_view body, Context context) {
               return useNode ? context.renderResult(node) : context.renderTemplate(body);
             })}});
    expectRender("{{#outer}}{{inner}}{{/outer}}", data, Mode::Template, "Ada");
    expectRender("{{#outer}}{{inner}}{{/outer}}", data, Mode::Literal, "{{name}}");
  }
}

void testSectionParsingSettings()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{#wrap}}{{/wrap}}", &root);
  mustache::Node output(mustache::Node::TypeOutput, "<b>");
  for (bool useNode : {false, true}) {
    auto data =
        mustache::Data::object({{"wrap", helper([&](std::string_view, Context context) {
                                   return useNode ? context.renderResult(output) : context.renderTemplate("<b>");
                                 })}});
    for (bool escape : {false, true}) {
      root.children.front()->flags = escape ? mustache::Node::FlagEscape : mustache::Node::FlagNone;
      const std::string expected = escape && !useNode ? "&lt;b&gt;" : "<b>";
      std::string actual;
      engine.render(&root, &data, nullptr, &actual);
      expect(actual == expected, "helper lost the section parsing setting or retokenized an existing node");
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
      const auto archived = mustache::loadArchivedTemplate(mustache::serializeArchivedTemplate(root));
      expect(engine.render(archived, data) == expected, "archived helper lost the section parsing setting");
#endif
    }
  }
}

} // namespace

int main()
{
  try {
    testLiteralResultsAndComposition();
    testCurrentContextAndPartials();
    testNestedDelimiterFrames();
    testNestedIndependentRenderers();
    testOwnedBytesAndInactiveContexts();
    testBudgets();
    testFailedParsingConsumesSharedBudget();
    testExceptionsAndRecovery();
    testNestedBudgets();
    testHelperInheritsMode();
    testSectionParsingSettings();
  } catch (const std::exception& error) {
    std::fprintf(stderr, "unexpected exception: %s\n", error.what());
    ++failures;
  }
  return failures == 0 ? 0 : 1;
}
