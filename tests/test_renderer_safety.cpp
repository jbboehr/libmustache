#include "mustache_config.h"

#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "data.hpp"
#include "exception.hpp"
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

template <typename Callable>
void expectException(const char * label, const char * expected,
    Callable&& callable)
{
  bool rejected = false;
  try {
    std::forward<Callable>(callable)();
  } catch( const mustache::Exception& exception ) {
    rejected = true;
    if( std::string(exception.what()) != expected ) {
      std::fprintf(stderr, "%s failed\n  expected: %s\n  actual:   %s\n",
          label, expected, exception.what());
      ++failures;
    }
  }
  if( !rejected ) {
    std::fprintf(stderr, "%s failed: no exception was thrown\n", label);
    ++failures;
  }
}

class FixedLambda : public mustache::Lambda {
  public:
    explicit FixedLambda(std::string value) : value(std::move(value)) {}

    std::string invoke() override
    {
      return value;
    }

    std::string invoke(
        std::string *, mustache::Renderer *) override
    {
      return value;
    }

  private:
    std::string value;
};

class RecoveringLambda : public mustache::Lambda {
  public:
    explicit RecoveringLambda(mustache::Renderer ** retained) :
        retained(retained) {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(
        std::string *, mustache::Renderer * renderer) override
    {
      *retained = renderer;

      bool mutationRejected = false;
      try {
        renderer->clear();
      } catch( const mustache::Exception& exception ) {
        mutationRejected =
            std::string(exception.what()) == "Renderer is already rendering";
      }
      if( !mutationRejected ) {
        throw mustache::Exception(
            "Active renderer mutation was not rejected");
      }

      bool reentryRejected = false;
      try {
        renderer->render();
      } catch( const mustache::Exception& exception ) {
        reentryRejected =
            std::string(exception.what()) == "Renderer is already rendering";
      }
      if( !reentryRejected ) {
        throw mustache::Exception(
            "Active renderer re-entry was not rejected");
      }

      mustache::Node failing(mustache::Node::TypeSection, "inner");
      failing.children.push_back(std::unique_ptr<mustache::Node>());
      std::string discarded;
      try {
        renderer->renderForLambda(&failing, &discarded);
      } catch( const mustache::Exception& ) {
      }

      mustache::Node name(mustache::Node::TypeVariable, "name");
      std::string recovered;
      renderer->renderForLambda(&name, &recovered);
      return recovered;
    }

  private:
    mustache::Renderer ** retained;
};

class CallbackRenderingLambda : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(
        std::string *, mustache::Renderer * renderer) override
    {
      mustache::Node literal(mustache::Node::TypeOutput, "abc");
      std::string nestedOutput;
      renderer->renderForLambda(&literal, &nestedOutput);
      return nestedOutput;
    }
};

class ScopedRenderingLambda : public mustache::Lambda {
  public:
    ScopedRenderingLambda(mustache::LambdaRenderContext * retained,
        std::string * observed) : retained(retained), observed(observed) {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(std::string_view text,
        mustache::LambdaRenderContext context) override
    {
      if( !context.active() ) {
        throw mustache::Exception(
            "Scoped lambda received an inactive context");
      }
      *retained = context;
      observed->assign(text.data(), text.size());

      mustache::Node name(mustache::Node::TypeVariable, "name");
      std::string buffered;
      context.render(name, buffered);
      if( context.render(name) != buffered ) {
        throw mustache::Exception(
            "Scoped lambda render overloads disagreed");
      }
      return buffered;
    }

  private:
    mustache::LambdaRenderContext * retained;
    std::string * observed;
};

class ThrowingScopedLambda : public mustache::Lambda {
  public:
    explicit ThrowingScopedLambda(
        mustache::LambdaRenderContext * retained) : retained(retained) {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(std::string_view,
        mustache::LambdaRenderContext context) override
    {
      *retained = context;
      throw mustache::Exception("Scoped lambda failed");
    }

  private:
    mustache::LambdaRenderContext * retained;
};

class InnerScopedLambda : public mustache::Lambda {
  public:
    explicit InnerScopedLambda(
        mustache::LambdaRenderContext * retained) : retained(retained) {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(std::string_view,
        mustache::LambdaRenderContext context) override
    {
      *retained = context;
      return std::string();
    }

  private:
    mustache::LambdaRenderContext * retained;
};

class OuterScopedLambda : public mustache::Lambda {
  public:
    OuterScopedLambda(mustache::LambdaRenderContext * retained,
        mustache::LambdaRenderContext * innerRetained) :
        retained(retained), innerRetained(innerRetained) {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(std::string_view,
        mustache::LambdaRenderContext context) override
    {
      *retained = context;

      mustache::Node inner(mustache::Node::TypeSection, "inner");
      inner.startSequence = "{{";
      inner.stopSequence = "}}";
      inner.children.push_back(std::make_unique<mustache::Node>(
          mustache::Node::TypeStop, "inner"));
      if( !context.render(inner).empty() ) {
        throw mustache::Exception(
            "Nested scoped lambda produced unexpected output");
      }
      if( innerRetained->active() ) {
        throw mustache::Exception(
            "Nested lambda context remained active after its callback");
      }

      mustache::Node name(mustache::Node::TypeVariable, "name");
      bool innerRejected = false;
      try {
        static_cast<void>(innerRetained->render(name));
      } catch( const mustache::Exception& exception ) {
        innerRejected = std::string(exception.what()) ==
            "Lambda render context is no longer active";
      }
      if( !innerRejected || !context.active() ) {
        throw mustache::Exception(
            "Nested lambda contexts were not isolated by callback frame");
      }
      return context.render(name);
    }

  private:
    mustache::LambdaRenderContext * retained;
    mustache::LambdaRenderContext * innerRetained;
};

std::string nestedPartial(std::string inner, std::size_t depth)
{
  std::string source;
  for( std::size_t index = 0; index < depth; ++index ) {
    source.append("{{#ok}}");
  }
  source.append(inner);
  for( std::size_t index = 0; index < depth; ++index ) {
    source.append("{{/ok}}");
  }
  return source;
}

void testLimitDefaultsAndOutputAccounting()
{
  mustache::RenderLimits defaults;
  expect(defaults.maxOutputBytes == 64 * 1024 * 1024,
      "default render output limit changed");
  expect(defaults.maxNestingDepth == 256,
      "default render nesting limit changed");
  expect(defaults.maxNodeVisits == 1000000,
      "default render node-visit limit changed");
  expect(defaults.maxLambdaTemplateBytes == 64 * 1024 * 1024,
      "default lambda-template limit changed");

  const mustache::CompiledTemplate escaped = mustache::compile("{{.}}");
  const mustache::Data lessThan = mustache::Data::string("<");
  mustache::RenderLimits limits;
  limits.maxOutputBytes = 4;
  expect(mustache::render(escaped, lessThan, limits) == "&lt;",
      "exact escaped-output limit rejected valid output");

  limits.maxOutputBytes = 3;
  expectException("escaped output limit",
      "Render output byte limit exceeded", [&]() {
        static_cast<void>(mustache::render(escaped, lessThan, limits));
      });

  const mustache::CompiledTemplate plain = mustache::compile("abcd");
  limits = mustache::RenderLimits();
  limits.maxOutputBytes = 4;
  expect(mustache::render(plain, mustache::Data::null(), limits) == "abcd",
      "exact literal-output limit rejected valid output");
  limits.maxOutputBytes = 3;
  expectException("literal output limit",
      "Render output byte limit exceeded", [&]() {
        static_cast<void>(
            mustache::render(plain, mustache::Data::null(), limits));
      });

  mustache::Data callbackData = mustache::Data::object({
      {"value", mustache::Data::lambda(
          std::make_unique<CallbackRenderingLambda>())}
  });
  const mustache::CompiledTemplate callback =
      mustache::compile("{{#value}}{{/value}}");
  limits = mustache::RenderLimits();
  limits.maxOutputBytes = 6;
  expect(mustache::render(callback, callbackData, limits) == "abc",
      "exact aggregate callback-output limit rejected valid output");
  limits.maxOutputBytes = 5;
  expectException("aggregate callback-output limit",
      "Render output byte limit exceeded", [&]() {
        static_cast<void>(mustache::render(callback, callbackData, limits));
      });
}

void testDepthAndWorkLimits()
{
  const mustache::CompiledTemplate plain = mustache::compile("x");
  const mustache::Data data = mustache::Data::null();
  mustache::RenderLimits limits;

  limits.maxNestingDepth = 2;
  expect(mustache::render(plain, data, limits) == "x",
      "exact render nesting limit rejected a root and child");
  limits.maxNestingDepth = 1;
  expectException("render nesting limit", "Render nesting limit exceeded",
      [&]() { static_cast<void>(mustache::render(plain, data, limits)); });

  limits = mustache::RenderLimits();
  limits.maxNodeVisits = 2;
  expect(mustache::render(plain, data, limits) == "x",
      "exact node-visit limit rejected a root and child");
  limits.maxNodeVisits = 1;
  expectException("render node-visit limit",
      "Render node visit limit exceeded", [&]() {
        static_cast<void>(mustache::render(plain, data, limits));
      });

  const mustache::CompiledTemplate cycle = mustache::compile("{{>cycle}}");
  mustache::PartialMap partials;
  partials.emplace("cycle", cycle);
  limits = mustache::RenderLimits();
  limits.maxNestingDepth = 8;
  expectException("recursive partial nesting",
      "Render nesting limit exceeded", [&]() {
        static_cast<void>(mustache::render(cycle, data, partials, limits));
      });

  const mustache::CompiledTemplate layout = mustache::compile("{{>one}}");
  mustache::PartialMap layoutPartials;
  layoutPartials.emplace("one",
      mustache::compile(nestedPartial("{{>two}}", 42)));
  layoutPartials.emplace("two",
      mustache::compile(nestedPartial("{{>three}}", 42)));
  layoutPartials.emplace("three",
      mustache::compile(nestedPartial("x", 42)));
  const mustache::Data layoutData = mustache::Data::object({
      {"ok", mustache::Data::boolean(true)}
  });
  expect(mustache::render(layout, layoutData, layoutPartials) == "x",
      "default nesting limit rejected a parser-bounded partial chain");
}

void testLambdaTemplateBudget()
{
  mustache::Data data = mustache::Data::object({
      {"value", mustache::Data::lambda(
          std::make_unique<FixedLambda>("x"))}
  });
  const mustache::CompiledTemplate twice =
      mustache::compile("{{value}}{{value}}");

  mustache::RenderLimits limits;
  limits.maxLambdaTemplateBytes = 2;
  expect(mustache::render(twice, data, limits) == "xx",
      "exact aggregate lambda-template limit rejected valid output");
  limits.maxLambdaTemplateBytes = 1;
  expectException("aggregate lambda-template limit",
      "Render lambda template byte limit exceeded", [&]() {
        static_cast<void>(mustache::render(twice, data, limits));
      });

  mustache::Data sectionData = mustache::Data::object({
      {"value", mustache::Data::lambda(
          std::make_unique<FixedLambda>(""))}
  });
  const mustache::CompiledTemplate section =
      mustache::compile("{{#value}}abcd{{/value}}");
  limits = mustache::RenderLimits();
  limits.maxLambdaTemplateBytes = 4;
  expect(mustache::render(section, sectionData, limits).empty(),
      "exact section-lambda input limit rejected valid callback text");
  limits.maxLambdaTemplateBytes = 3;
  expectException("section-lambda input limit",
      "Render lambda template byte limit exceeded", [&]() {
        static_cast<void>(mustache::render(section, sectionData, limits));
      });
}

void testLambdaNodeAccounting()
{
  mustache::Data data = mustache::Data::object({
      {"value", mustache::Data::lambda(std::make_unique<FixedLambda>(
          "{{#missing}}{{!a}}{{!b}}{{/missing}}"))}
  });
  const mustache::CompiledTemplate source = mustache::compile("{{value}}");

  mustache::RenderLimits limits;
  limits.maxNodeVisits = 9;
  expect(mustache::render(source, data, limits).empty(),
      "exact parsed-lambda node budget rejected valid output");
  limits.maxNodeVisits = 8;
  expectException("parsed lambda node budget",
      "Render node visit limit exceeded", [&]() {
        static_cast<void>(mustache::render(source, data, limits));
      });

  limits.maxNodeVisits = 3;
  expectException("lambda tokenizer aggregate node bound",
      "Render node visit limit exceeded", [&]() {
        static_cast<void>(mustache::render(source, data, limits));
      });
}

void testScopedLambdaContext()
{
  mustache::LambdaRenderContext inactive;
  expect(!inactive.active(), "a default lambda context was active");
  mustache::Node literal(mustache::Node::TypeOutput, "late");
  expectException("default lambda context",
      "Lambda render context is no longer active", [&]() {
        static_cast<void>(inactive.render(literal));
      });

  mustache::LambdaRenderContext retained;
  std::string observed;
  mustache::Data data = mustache::Data::object({
      {"name", mustache::Data::string("safe")},
      {"scoped", mustache::Data::lambda(
          std::make_unique<ScopedRenderingLambda>(&retained, &observed))}
  });
  const mustache::CompiledTemplate scoped =
      mustache::compile("{{#scoped}}original{{/scoped}}");
  expect(mustache::render(scoped, data) == "safe",
      "scoped lambda rendering changed callback output");
  expect(observed == "original",
      "scoped lambda did not receive exact section text");
  expect(!retained.active(),
      "retained lambda context remained active after callback completion");
  expectException("retained scoped lambda context",
      "Lambda render context is no longer active", [&]() {
        static_cast<void>(retained.render(literal));
      });

  mustache::LambdaRenderContext failed;
  mustache::Data failureData = mustache::Data::object({
      {"fail", mustache::Data::lambda(
          std::make_unique<ThrowingScopedLambda>(&failed))}
  });
  const mustache::CompiledTemplate failure =
      mustache::compile("{{#fail}}{{/fail}}");
  expectException("throwing scoped lambda", "Scoped lambda failed", [&]() {
    static_cast<void>(mustache::render(failure, failureData));
  });
  expect(!failed.active(),
      "throwing lambda left its retained context active");
  expectException("retained context after callback exception",
      "Lambda render context is no longer active", [&]() {
        static_cast<void>(failed.render(literal));
      });

  mustache::LambdaRenderContext outerRetained;
  mustache::LambdaRenderContext innerRetained;
  mustache::Data nestedData = mustache::Data::object({
      {"name", mustache::Data::string("outer")},
      {"inner", mustache::Data::lambda(
          std::make_unique<InnerScopedLambda>(&innerRetained))},
      {"outer", mustache::Data::lambda(
          std::make_unique<OuterScopedLambda>(
              &outerRetained, &innerRetained))}
  });
  const mustache::CompiledTemplate nested =
      mustache::compile("{{#outer}}{{/outer}}");
  expect(mustache::render(nested, nestedData) == "outer",
      "nested scoped lambda rendering failed");
  expect(!outerRetained.active() && !innerRetained.active(),
      "nested lambda contexts survived their callback frames");
}

void testFailureStateAndCallbackWindow()
{
  mustache::Renderer * retained = NULL;
  mustache::Data data = mustache::Data::object({
      {"name", mustache::Data::string("outer")},
      {"inner", mustache::Data::object({
          {"name", mustache::Data::string("inner")}
      })},
      {"recover", mustache::Data::lambda(
          std::make_unique<RecoveringLambda>(&retained))}
  });
  std::string source = "{{#recover}}ignored{{/recover}}!";
  mustache::Node root;
  mustache::Mustache engine;
  engine.tokenize(&source, &root);

  std::string output;
  engine.renderer.init(&root, &data, NULL, &output);
  engine.renderer.render();
  expect(output == "outer!",
      "a caught callback failure left output or data-stack state corrupted");
  expect(retained == &engine.renderer,
      "the section callback did not receive its renderer");

  mustache::Node literal(mustache::Node::TypeOutput, "late");
  std::string lateOutput;
  expectException("retained lambda renderer",
      "Lambda renderer is no longer active", [&]() {
        retained->renderForLambda(&literal, &lateOutput);
      });

  mustache::RenderLimits limits;
  limits.maxOutputBytes = 0;
  output.clear();
  engine.renderer.init(&root, &data, NULL, &output, limits);
  expectException("failed renderer operation",
      "Render output byte limit exceeded",
      [&]() { engine.renderer.render(); });

  output.clear();
  engine.renderer.init(&root, &data, NULL, &output);
  engine.renderer.render();
  expect(output == "outer!",
      "a renderer could not be reused after limit exhaustion");

  engine.renderer.init(&root, &data, NULL, NULL);
  expectException("null output buffer", "Missing output buffer",
      [&]() { engine.renderer.render(); });

  mustache::Node none(mustache::Node::TypeNone, "ignored");
  output.clear();
  engine.renderer.init(&none, &data, NULL, &output);
  engine.renderer.render();
  expect(output.empty(), "a compatibility TypeNone node was not skipped");

  std::string failingSource = "AAAA{{name}}BBBB";
  mustache::Node failingRoot;
  engine.tokenize(&failingSource, &failingRoot);
  limits = mustache::RenderLimits();
  limits.maxOutputBytes = 6;
  output.clear();
  engine.renderer.init(&failingRoot, &data, NULL, &output, limits);
  expectException("partial caller output on failure",
      "Render output byte limit exceeded",
      [&]() { engine.renderer.render(); });
  expect(output == "AAAA",
      "a render failure did not retain the documented output prefix");
}

} // namespace

static_assert(!std::is_copy_constructible<mustache::Renderer>::value,
    "a renderer must not duplicate borrowed operation state");
static_assert(!std::is_copy_assignable<mustache::Renderer>::value,
    "a renderer must not copy borrowed operation state");
static_assert(!std::is_move_constructible<mustache::Renderer>::value,
    "an active renderer must not be movable");
static_assert(!std::is_move_assignable<mustache::Renderer>::value,
    "an active renderer must not be move assigned");
static_assert(!std::is_copy_constructible<mustache::Mustache>::value,
    "Mustache must not copy its renderer's borrowed operation state");
static_assert(!std::is_move_constructible<mustache::Mustache>::value,
    "Mustache must not move its renderer's borrowed operation state");
static_assert(
    std::is_copy_constructible<mustache::LambdaRenderContext>::value,
    "lambda render contexts must be safely retainable by value");
static_assert(
    std::is_nothrow_move_constructible<mustache::LambdaRenderContext>::value,
    "lambda render contexts must be nothrow movable");
static_assert(!std::is_abstract<ScopedRenderingLambda>::value,
    "new lambdas must not need to implement the legacy renderer hook");

int main()
{
  testLimitDefaultsAndOutputAccounting();
  testDepthAndWorkLimits();
  testLambdaTemplateBudget();
  testLambdaNodeAccounting();
  testScopedLambdaContext();
  testFailureStateAndCallbackWindow();
  return failures == 0 ? 0 : 1;
}
