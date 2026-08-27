
#include "renderer.hpp"

#include <algorithm>
#include <utility>

#include "render_engine.hpp"
#include "tokenizer.hpp"

namespace mustache {

std::mutex Renderer::ActiveEngineScope::registryMutex_;
Renderer::ActiveEngineScope * Renderer::ActiveEngineScope::current_ = NULL;

namespace {

template <typename Callable> class ScopeExit {
  public:
    explicit ScopeExit(Callable callable) :
        callable(std::move(callable))
    {}

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;

    ~ScopeExit() noexcept
    {
      callable();
    }

  private:
    Callable callable;
};

template <typename Callable> ScopeExit<Callable> onScopeExit(Callable callable)
{
  return ScopeExit<Callable>(std::move(callable));
}

std::string_view escapedValue(char value)
{
  switch (value) {
    case '&':
      return "&amp;";
    case '"':
      return "&quot;";
    case '\'':
      return "&#039;";
    case '<':
      return "&lt;";
    case '>':
      return "&gt;";
    default:
      return std::string_view();
  }
}

} // namespace

RenderLimits::RenderLimits() :
    maxOutputBytes(std::size_t{64} * 1024 * 1024),
    maxNestingDepth(detail::renderNestingCeiling),
    maxNodeVisits(1000000),
    maxLambdaTemplateBytes(std::size_t{64} * 1024 * 1024)
{}

Renderer::Renderer() :
    _node(NULL),
    _data(NULL),
    _partials(NULL),
    _output(NULL),
    _limits(),
    _outputBytes(0),
    _nodeVisits(0),
    _lambdaTemplateBytes(0),
    _activeDepth(0),
    _rendering(false),
    _lambdaCallbackDepth(0),
    _strictPaths(false)
{}

Renderer::~Renderer()
{}

void Renderer::clear()
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  _node = NULL;
  _data = NULL;
  _stack.clear();
  _partials = NULL;
  _partialResolver = PartialResolver();
  _output = NULL;
  _outputBytes = 0;
  _nodeVisits = 0;
  _lambdaTemplateBytes = 0;
  _indentationStack.clear();
  _activeDepth = 0;
  _rendering = false;
  _lambdaCallbackDepth = 0;
}

void Renderer::init(const Node * node, const Data * data, const Node::Partials * partials, std::string * output)
{
  init(node, data, partials, output, RenderLimits());
}

void Renderer::init(const Node * node, const Data * data, const Node::Partials * partials, std::string * output,
    const RenderLimits& limits)
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  clear();
  _node = node;
  _data = data;
  if (partials != NULL && partials->size() > 0) {
    // Don't add if no partials so we can check if it's null
    _partials = partials;
  }
  _output = output;
  _limits = limits;
}

void Renderer::setNode(const Node * node)
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  _node = node;
}

void Renderer::setData(const Data * data)
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  _data = data;
}

void Renderer::setPartials(const Node::Partials * partials)
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  _partials = partials;
}

void Renderer::setOutput(std::string * output)
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  _output = output;
}

void Renderer::setPartialResolver(PartialResolver resolver)
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  _partialResolver = std::move(resolver);
}

void Renderer::render()
{
  if (_rendering) {
    throw Exception("Renderer is already rendering");
  }
  if (_node == NULL) {
    throw Exception("Empty tree");
  } else if (_data == NULL) {
    throw Exception("Empty data");
  } else if (_output == NULL) {
    throw Exception("Missing output buffer");
  } else if (_output->size() > _limits.maxOutputBytes) {
    throw Exception("Render output byte limit exceeded");
  }

  _rendering = true;
  _outputBytes = _output->size();
  _nodeVisits = 0;
  _lambdaTemplateBytes = 0;
  _indentationStack.clear();
  _activeDepth = 0;
  _lambdaCallbackDepth = 0;
  _stack.clear();
  detail::OwnedPartialSource partialSource(&_partialResolver, _partials, _node);
  detail::RenderEngine<detail::OwnedPartialSource> engine(*this, partialSource);
  const auto renderGuard = onScopeExit([this]() {
    _stack.clear();
    _indentationStack.clear();
    _activeDepth = 0;
    _lambdaCallbackDepth = 0;
    _rendering = false;
  });

  if (_output->empty() && _output->capacity() == 0) {
    const std::size_t reserveBytes =
        std::min(static_cast<std::size_t>(Renderer::outputBufferLength), _limits.maxOutputBytes);
    _output->reserve(reserveBytes);
  }

  _stack.push_back(_data);
  engine.renderOwnedNode(_node, 0);
}

void Renderer::renderForLambda(const Node * node, std::string * output)
{
  if (!_rendering || _lambdaCallbackDepth == 0) {
    throw Exception("Lambda renderer is no longer active");
  }
  if (node == NULL) {
    throw Exception("Empty tree node");
  }
  if (output == NULL) {
    throw Exception("Missing output buffer");
  }

  std::string * parentOutput = _output;
  _output = output;
  const auto outputGuard = onScopeExit([this, parentOutput]() {
    _output = parentOutput;
  });

  if (_output->size() > _limits.maxOutputBytes) {
    throw Exception("Render output byte limit exceeded");
  }
  _indentationStack.emplace_back();
  const auto indentationGuard = onScopeExit([this]() {
    _indentationStack.pop_back();
  });
  ActiveRenderEngine * activeEngine = ActiveEngineScope::find(this);
  if (activeEngine == NULL) {
    throw Exception("Missing active render engine");
  }
  activeEngine->renderOwnedNode(node, _activeDepth + 1);
}

void Renderer::_append(std::string_view value)
{
  if (_output == NULL) {
    throw Exception("Missing output buffer");
  }
  const std::size_t maximum = std::min(_limits.maxOutputBytes, _output->max_size());
  if (_output->size() > maximum || value.size() > maximum - _output->size()) {
    throw Exception("Render output byte limit exceeded");
  }
  _consumeOutputBytes(value.size());
  if (!value.empty()) {
    _output->append(value.data(), value.size());
  }
}

void Renderer::_appendEscaped(std::string_view value)
{
  if (_output == NULL) {
    throw Exception("Missing output buffer");
  }
  const std::size_t maximum = std::min(_limits.maxOutputBytes, _output->max_size());
  if (_output->size() > maximum) {
    throw Exception("Render output byte limit exceeded");
  }

  const std::size_t available = maximum - _output->size();
  std::size_t addition = 0;
  for (const char character : value) {
    const std::string_view escaped = escapedValue(character);
    const std::size_t bytes = escaped.empty() ? 1 : escaped.size();
    if (bytes > available - addition) {
      throw Exception("Render output byte limit exceeded");
    }
    addition += bytes;
  }
  _consumeOutputBytes(addition);

  for (const char character : value) {
    const std::string_view escaped = escapedValue(character);
    if (escaped.empty()) {
      _output->push_back(character);
    } else {
      _output->append(escaped.data(), escaped.size());
    }
  }
}

void Renderer::_appendTemplateOutput(std::string_view value)
{
  if (_indentationStack.empty()) {
    _append(value);
    return;
  }

  IndentationFrame& frame = _indentationStack.back();
  std::size_t offset = 0;
  while (offset < value.size()) {
    if (frame.atLineStart) {
      _appendIndentation(frame);
      frame.atLineStart = false;
    }

    const std::size_t newline = value.find('\n', offset);
    if (newline == std::string_view::npos) {
      _append(value.substr(offset));
      return;
    }
    _append(value.substr(offset, newline - offset + 1));
    frame.atLineStart = true;
    offset = newline + 1;
  }
}

void Renderer::_appendIndentation(const IndentationFrame& frame)
{
  for (const std::string_view component : frame.components) {
    _append(component);
  }
}

void Renderer::_consumeTemplateSource(std::string_view value)
{
  if (_indentationStack.empty()) {
    return;
  }

  IndentationFrame& frame = _indentationStack.back();
  if (value.empty()) {
    // Empty lambda-only nodes are explicit standalone-prefix markers.
    frame.atLineStart = false;
    return;
  }
  for (const char character : value) {
    frame.atLineStart = character == '\n';
  }
}

void Renderer::_beginTemplateTag()
{
  if (_indentationStack.empty()) {
    return;
  }

  IndentationFrame& frame = _indentationStack.back();
  if (frame.atLineStart) {
    _appendIndentation(frame);
    frame.atLineStart = false;
  }
}

void Renderer::_consumeOutputBytes(std::size_t bytes)
{
  if (_outputBytes > _limits.maxOutputBytes || bytes > _limits.maxOutputBytes - _outputBytes) {
    throw Exception("Render output byte limit exceeded");
  }
  _outputBytes += bytes;
}

void Renderer::_consumeLambdaTemplate(std::size_t bytes)
{
  if (_lambdaTemplateBytes > _limits.maxLambdaTemplateBytes ||
      bytes > _limits.maxLambdaTemplateBytes - _lambdaTemplateBytes) {
    throw Exception("Render lambda template byte limit exceeded");
  }
  _lambdaTemplateBytes += bytes;
}

void Renderer::_consumeNodeVisit()
{
  if (_nodeVisits >= _limits.maxNodeVisits) {
    throw Exception("Render node visit limit exceeded");
  }
  ++_nodeVisits;
}

void Renderer::_tokenizeLambda(Tokenizer * tokenizer, std::string_view source, Node * root, bool escapeOutput)
{
  if (tokenizer == NULL || root == NULL) {
    throw Exception("Missing lambda tokenizer state");
  }
  if (_nodeVisits >= _limits.maxNodeVisits) {
    throw Exception("Render node visit limit exceeded");
  }

  Tokenizer::Limits tokenizerLimits;
  tokenizerLimits.maxInputBytes = _limits.maxLambdaTemplateBytes;
  // Tokenizer::Limits::maxNodes excludes the root, which is also charged.
  const std::size_t aggregateNodeLimit = _limits.maxNodeVisits - _nodeVisits - 1;
  const bool limitedByRenderBudget = aggregateNodeLimit <= tokenizerLimits.maxNodes;
  tokenizerLimits.maxNodes = std::min(tokenizerLimits.maxNodes, aggregateNodeLimit);
  try {
    tokenizer->tokenize(source, root, tokenizerLimits, escapeOutput);
  } catch (const TokenizerException& exception) {
    if (limitedByRenderBudget && std::string_view(exception.what()) == "Template node count limit exceeded") {
      throw Exception("Render node visit limit exceeded");
    }
    throw;
  }
  _consumeLambdaNodes(root);
}

void Renderer::_consumeLambdaNodes(const Node * node)
{
  if (node == NULL) {
    throw Exception("Invalid null child node");
  }
  _consumeNodeVisit();
  if (node->child != NULL) {
    _consumeLambdaNodes(node->child.get());
  }
  for (const std::unique_ptr<Node>& child : node->children) {
    _consumeLambdaNodes(child.get());
  }
}

void Renderer::_appendLambdaTemplate(std::string * output, std::string_view value)
{
  if (output == NULL) {
    throw Exception("Missing lambda template buffer");
  }
  if (output->size() > output->max_size() || value.size() > output->max_size() - output->size()) {
    throw Exception("Render lambda template byte limit exceeded");
  }
  _consumeLambdaTemplate(value.size());
  if (!value.empty()) {
    output->append(value.data(), value.size());
  }
}

std::string Renderer::_invokeSectionLambda(
    Lambda * lambda, std::string_view text, ActiveRenderEngine * activeRenderEngine)
{
  if (lambda == NULL) {
    throw Exception("Missing section lambda");
  }

  ActiveEngineScope activeEngineScope(this, activeRenderEngine);
  LambdaRenderContext context(this);
  ++_lambdaCallbackDepth;
  const auto callbackGuard = onScopeExit([this, &context]() {
    context.invalidate();
    --_lambdaCallbackDepth;
  });
  return lambda->invoke(text, context);
}

} // namespace mustache
