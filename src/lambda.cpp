#include "lambda.hpp"

#include <mutex>
#include <utility>

#include "exception.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "tokenizer.hpp"

namespace mustache {

LambdaResult::LambdaResult(Kind kind, std::string text) :
    kind_(kind),
    text_(std::move(text))
{}

LambdaResult LambdaResult::fromString(std::string text)
{
  return LambdaResult(Kind::Inherit, std::move(text));
}

LambdaResult LambdaResult::literal(std::string text)
{
  return LambdaResult(Kind::Literal, std::move(text));
}

LambdaResult LambdaResult::templateSource(std::string text)
{
  return LambdaResult(Kind::Template, std::move(text));
}

LambdaResult::Kind LambdaResult::kind() const noexcept
{
  return kind_;
}

const std::string& LambdaResult::text() const noexcept
{
  return text_;
}

struct LambdaRenderContext::State {
    State(Renderer * renderer, std::string_view start, std::string_view stop, bool escapeOutput) :
        renderer(renderer),
        start(start),
        stop(stop),
        escapeOutput(escapeOutput)
    {}

    mutable std::recursive_mutex mutex;
    Renderer * renderer;
    const std::string start;
    const std::string stop;
    const bool escapeOutput;
};

LambdaRenderContext::LambdaRenderContext(
    Renderer * renderer, std::string_view start, std::string_view stop, bool escapeOutput) :
    state(std::make_shared<State>(renderer, start, stop, escapeOutput))
{}

bool LambdaRenderContext::active() const
{
  if (!state) {
    return false;
  }
  const std::lock_guard<std::recursive_mutex> lock(state->mutex);
  return state->renderer != NULL;
}

void LambdaRenderContext::render(const Node& node, std::string& output) const
{
  if (!state) {
    throw Exception("Lambda render context is no longer active");
  }
  const std::lock_guard<std::recursive_mutex> lock(state->mutex);
  if (state->renderer == NULL) {
    throw Exception("Lambda render context is no longer active");
  }
  state->renderer->renderForLambda(&node, &output);
}

std::string LambdaRenderContext::render(const Node& node) const
{
  std::string output;
  render(node, output);
  return output;
}

LambdaResult LambdaRenderContext::renderResult(const Node& node) const
{
  return LambdaResult::literal(render(node));
}

LambdaResult LambdaRenderContext::renderTemplate(std::string_view source) const
{
  if (!state) {
    throw Exception("Lambda render context is no longer active");
  }
  const std::lock_guard<std::recursive_mutex> lock(state->mutex);
  if (state->renderer == NULL) {
    throw Exception("Lambda render context is no longer active");
  }
  state->renderer->_consumeLambdaTemplate(source.size());
  Tokenizer tokenizer;
  tokenizer.setStartSequence(state->start);
  tokenizer.setStopSequence(state->stop);
  Node node;
  state->renderer->_tokenizeLambda(&tokenizer, source, &node, state->escapeOutput);
  return renderResult(node);
}

Renderer * LambdaRenderContext::legacyRenderer() const
{
  if (!state) {
    throw Exception("Lambda render context is no longer active");
  }
  const std::lock_guard<std::recursive_mutex> lock(state->mutex);
  if (state->renderer == NULL) {
    throw Exception("Lambda render context is no longer active");
  }
  return state->renderer;
}

void LambdaRenderContext::invalidate() noexcept
{
  if (!state) {
    return;
  }
  const std::lock_guard<std::recursive_mutex> lock(state->mutex);
  state->renderer = NULL;
}

std::string Lambda::invoke(std::string *, Renderer *)
{
  throw Exception("Legacy section lambda callback is not implemented");
}

std::string Lambda::invoke()
{
  throw Exception("Interpolation lambda callback is not implemented");
}

LambdaResult Lambda::invokeResult()
{
  return LambdaResult::fromString(invoke());
}

LambdaResult Lambda::invokeResult(std::string_view text, LambdaRenderContext context)
{
  return LambdaResult::fromString(invoke(text, std::move(context)));
}

// Passing the context by value gives each callback its own retainable capability handle.
std::string Lambda::invoke(
    std::string_view text, LambdaRenderContext context) // NOLINT(performance-unnecessary-value-param)
{
  std::string ownedText(text);
  return invoke(&ownedText, context.legacyRenderer());
}

} // namespace mustache
