
#include "renderer.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

#include "tokenizer.hpp"

namespace mustache {

namespace {

const std::size_t renderNestingCeiling = 256;

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

bool isPartialIndentation(std::string_view value)
{
  return std::all_of(value.begin(), value.end(), [](char character) {
    return character == ' ' || character == '\t';
  });
}

} // namespace

/*! Immutable view of one owned renderable node.

    This adapter is intentionally Node-specific. It establishes the operations
    used by the owned path; a later compile-time shared engine can pair those
    operations with an archive-specific view and partial source without
    exposing either representation publicly.
*/
class Renderer::NodeView {
  public:
    NodeView() noexcept :
        node_(NULL)
    {}

    static NodeView fromNode(const Node * node) noexcept
    {
      return NodeView(node);
    }

    explicit operator bool() const noexcept
    {
      return node_ != NULL;
    }

    Node::Type type() const noexcept
    {
      assert(node_ != NULL);
      return node_->type;
    }

    int flags() const noexcept
    {
      assert(node_ != NULL);
      return node_->flags;
    }

    const std::string * data() const noexcept
    {
      assert(node_ != NULL);
      return node_->data.has_value() ? &*node_->data : NULL;
    }

    std::size_t dataPartCount() const noexcept
    {
      assert(node_ != NULL);
      return node_->dataParts.size();
    }

    const std::string& dataPart(std::size_t index) const noexcept
    {
      assert(node_ != NULL);
      assert(index < node_->dataParts.size());
      return node_->dataParts[index];
    }

    std::size_t childCount() const noexcept
    {
      assert(node_ != NULL);
      return node_->children.size();
    }

    NodeView child(std::size_t index) const noexcept
    {
      assert(node_ != NULL);
      assert(index < node_->children.size());
      return NodeView(node_->children[index].get());
    }

    NodeView containerChild() const noexcept
    {
      assert(node_ != NULL);
      return NodeView(node_->child.get());
    }

    const std::string * startSequence() const noexcept
    {
      assert(node_ != NULL);
      return node_->startSequence.has_value() ? &*node_->startSequence : NULL;
    }

    const std::string * stopSequence() const noexcept
    {
      assert(node_ != NULL);
      return node_->stopSequence.has_value() ? &*node_->stopSequence : NULL;
    }

    NodeView findPartial(const std::string& name) const
    {
      assert(node_ != NULL);
      const Node::Partials::const_iterator partial = node_->partials.find(name);
      return partial == node_->partials.end() ? NodeView() : NodeView(partial->second.get());
    }

  private:
    explicit NodeView(const Node * node) noexcept :
        node_(node)
    {}

    const Node * node_;
};

RenderLimits::RenderLimits() :
    maxOutputBytes(std::size_t{64} * 1024 * 1024),
    maxNestingDepth(renderNestingCeiling),
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
  _renderNode(NodeView::fromNode(_node), 0);
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
  _renderNode(NodeView::fromNode(node), _activeDepth + 1);
}

std::string_view Renderer::_requireNodeData(NodeView node)
{
  const std::string * data = node.data();
  if (data == NULL) {
    throw Exception("Invalid node without data");
  }
  return *data;
}

const std::string& Renderer::_requireOwnedNodeData(NodeView node)
{
  const std::string * data = node.data();
  if (data == NULL) {
    throw Exception("Invalid node without data");
  }
  return *data;
}

const std::string * Renderer::_validatePartialIndentationAt(NodeView parent, std::size_t index)
{
  const NodeView child = parent.child(index);
  if (!child || (child.flags() & Node::FlagPartialIndent) == 0) {
    return NULL;
  }
  const std::string * data = child.data();
  if (child.type() != Node::TypeOutput || child.flags() != (Node::FlagLambdaOnly | Node::FlagPartialIndent) ||
      data == NULL || !isPartialIndentation(*data) || index + 1 >= parent.childCount() || !parent.child(index + 1) ||
      parent.child(index + 1).type() != Node::TypePartial) {
    throw Exception("Invalid standalone partial indentation metadata");
  }
  return data;
}

void Renderer::_renderChildren(NodeView node, std::size_t depth)
{
  for (std::size_t index = 0; index < node.childCount(); ++index) {
    const NodeView child = node.child(index);
    const std::string * partialIndentation = _validatePartialIndentationAt(node, index);
    if (partialIndentation != NULL) {
      _renderNode(child, depth + 1, NULL, true);
      _renderNode(node.child(index + 1), depth + 1, partialIndentation);
      ++index;
      continue;
    }
    _renderNode(child, depth + 1);
  }
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

void Renderer::_appendLambdaNodeTemplate(
    NodeView node, std::string_view start, std::string_view stop, std::string * output, std::size_t depth)
{
  if (!node) {
    throw Exception("Invalid null child node");
  }
  if (depth >= _limits.maxNestingDepth || depth >= renderNestingCeiling) {
    throw Exception("Render nesting limit exceeded");
  }
  _consumeNodeVisit();
  if (!(node.type() & Node::TypeHasNoString) && node.data() == NULL) {
    throw Exception("Invalid node without data");
  }

  bool appendChildren = false;
  switch (node.type()) {
    case Node::TypeComment:
      _appendLambdaTemplate(output, start);
      _appendLambdaTemplate(output, "!");
      _appendLambdaTemplate(output, _requireNodeData(node));
      _appendLambdaTemplate(output, stop);
      break;
    case Node::TypeOutput:
      _appendLambdaTemplate(output, _requireNodeData(node));
      break;
    case Node::TypePartial:
      _appendLambdaTemplate(output, start);
      _appendLambdaTemplate(output, ">");
      _appendLambdaTemplate(output, _requireNodeData(node));
      _appendLambdaTemplate(output, stop);
      break;
    case Node::TypeNegate:
    case Node::TypeSection:
    case Node::TypeStop:
    case Node::TypeVariable:
      _appendLambdaTemplate(output, start);
      if (node.type() == Node::TypeVariable && !(node.flags() & Node::FlagEscape)) {
        _appendLambdaTemplate(output, "&");
      }
      if (node.type() == Node::TypeNegate) {
        _appendLambdaTemplate(output, "^");
      } else if (node.type() == Node::TypeSection) {
        _appendLambdaTemplate(output, "#");
      } else if (node.type() == Node::TypeStop) {
        _appendLambdaTemplate(output, "/");
      }
      _appendLambdaTemplate(output, _requireNodeData(node));
      _appendLambdaTemplate(output, stop);
      appendChildren = true;
      break;
    case Node::TypeRoot:
      appendChildren = true;
      break;
    case Node::TypeNone:
    case Node::TypeTag:
    case Node::TypeContainer:
    case Node::TypeInlinePartial:
      break;
    default:
      throw Exception("Unknown tree node type");
  }

  if (appendChildren) {
    for (std::size_t index = 0; index < node.childCount(); ++index) {
      _validatePartialIndentationAt(node, index);
      _appendLambdaNodeTemplate(node.child(index), start, stop, output, depth + 1);
    }
  }
}

std::string Renderer::_lambdaSectionText(
    NodeView node, std::string_view start, std::string_view stop, std::size_t depth)
{
  std::string output;
  for (std::size_t index = 0; index < node.childCount(); ++index) {
    _validatePartialIndentationAt(node, index);
    const NodeView child = node.child(index);
    if (!child) {
      throw Exception("Invalid null child node");
    }
    if (child.type() == Node::TypeStop) {
      if (depth + 1 >= _limits.maxNestingDepth || depth + 1 >= renderNestingCeiling) {
        throw Exception("Render nesting limit exceeded");
      }
      _consumeNodeVisit();
    } else {
      _appendLambdaNodeTemplate(child, start, stop, &output, depth + 1);
    }
  }
  return output;
}

void Renderer::_renderNode(
    NodeView node, std::size_t depth, const std::string * partialIndentation, bool partialIndentationMetadata)
{
  if (!node) {
    throw Exception("Empty tree node");
  }
  if (depth >= _limits.maxNestingDepth || depth >= renderNestingCeiling) {
    throw Exception("Render nesting limit exceeded");
  }
  _consumeNodeVisit();

  if ((node.flags() & Node::FlagPartialIndent) != 0) {
    const std::string * metadata = node.data();
    if (!partialIndentationMetadata || node.type() != Node::TypeOutput ||
        node.flags() != (Node::FlagLambdaOnly | Node::FlagPartialIndent) || metadata == NULL ||
        !isPartialIndentation(*metadata)) {
      throw Exception("Invalid standalone partial indentation metadata");
    }
  }

  const std::size_t parentDepth = _activeDepth;
  _activeDepth = depth;
  const auto depthGuard = onScopeExit([this, parentDepth]() {
    _activeDepth = parentDepth;
  });

  if (_stack.empty()) {
    throw Exception("Whoops, empty data");
  }
  if (!(node.type() & Node::TypeHasNoString) && node.data() == NULL) {
    throw Exception("Whoops, empty tag");
  }

  bool valIsEmpty = true;
  const Data * val = NULL;
  if (node.type() & Node::TypeHasData) {
    val = _lookup(node);
  }
  if (val != NULL && !val->isEmpty()) {
    valIsEmpty = false;
  }

  const auto renderWithContext = [this, node, depth](const Data * context) {
    _stack.push_back(context);
    const auto stackGuard = onScopeExit([this]() {
      _stack.pop_back();
    });
    _renderChildren(node, depth);
  };

  switch (node.type()) {
    case Node::TypeNone:
      break;

    case Node::TypeComment:
    case Node::TypeStop:
    case Node::TypeInlinePartial:
      _beginTemplateTag();
      break;

    case Node::TypeRoot:
      _renderChildren(node, depth);
      break;

    case Node::TypeOutput:
      if (const std::string * output = node.data()) {
        if (node.flags() & Node::FlagLambdaOnly) {
          _consumeTemplateSource(*output);
        } else {
          _appendTemplateOutput(*output);
        }
      }
      break;

    case Node::TypeContainer:
      if (!node.containerChild()) {
        throw Exception("Empty container node");
      }
      _renderNode(node.containerChild(), depth + 1);
      break;

    case Node::TypeTag:
    case Node::TypeVariable:
      _beginTemplateTag();
      if (!valIsEmpty) {
        switch (val->type()) {
          case Data::TypeString:
            if (node.flags() & Node::FlagEscape) {
              _appendEscaped(val->stringValue());
            } else {
              _append(val->stringValue());
            }
            break;
          case Data::TypeBoolean:
          case Data::TypeInteger:
          case Data::TypeDouble: {
            const std::string rendered = val->toString();
            if (node.flags() & Node::FlagEscape) {
              _appendEscaped(rendered);
            } else {
              _append(rendered);
            }
            break;
          }
          case Data::TypeLambda: {
            std::string invoked = val->lambdaValue()->invoke();
            _consumeLambdaTemplate(invoked.size());

            Tokenizer tokenizer;
            Node nodeFromLambda;
            _tokenizeLambda(&tokenizer, invoked, &nodeFromLambda, node.flags() & Node::FlagEscape);
            _indentationStack.emplace_back();
            const auto indentationGuard = onScopeExit([this]() {
              _indentationStack.pop_back();
            });
            _renderNode(NodeView::fromNode(&nodeFromLambda), depth + 1);
            break;
          }
          case Data::TypeNone:
          case Data::TypeList:
          case Data::TypeMap:
          case Data::TypeArray:
            break;
        }
      }
      break;

    case Node::TypeNegate:
      _beginTemplateTag();
      if (valIsEmpty) {
        _renderChildren(node, depth);
      }
      break;

    case Node::TypeSection:
      _beginTemplateTag();
      if (!valIsEmpty) {
        switch (val->type()) {
          case Data::TypeString:
          case Data::TypeBoolean:
          case Data::TypeInteger:
          case Data::TypeDouble:
          case Data::TypeMap:
            renderWithContext(val);
            break;
          case Data::TypeList:
            for (const Data& child : val->listItems()) {
              renderWithContext(&child);
            }
            break;
          case Data::TypeArray:
            for (const Data& child : val->arrayItems()) {
              renderWithContext(&child);
            }
            break;
          case Data::TypeLambda: {
            const std::string * startSequence = node.startSequence();
            const std::string * stopSequence = node.stopSequence();
            if (startSequence == NULL || stopSequence == NULL) {
              throw Exception("Missing section delimiters");
            }
            const std::string_view start = *startSequence;
            const std::string_view stop = *stopSequence;
            const std::string text = _lambdaSectionText(node, start, stop, depth);
            std::string invoked;
            {
              LambdaRenderContext context(this);
              ++_lambdaCallbackDepth;
              const auto callbackGuard = onScopeExit([this, &context]() {
                context.invalidate();
                --_lambdaCallbackDepth;
              });
              invoked = val->lambdaValue()->invoke(std::string_view(text), context);
            }
            _consumeLambdaTemplate(invoked.size());

            Tokenizer tokenizer;
            tokenizer.setStartSequence(start);
            tokenizer.setStopSequence(stop);
            Node nodeFromLambda;
            _tokenizeLambda(&tokenizer, invoked, &nodeFromLambda, node.flags() & Node::FlagEscape);
            _indentationStack.emplace_back();
            const auto indentationGuard = onScopeExit([this]() {
              _indentationStack.pop_back();
            });
            _renderNode(NodeView::fromNode(&nodeFromLambda), depth + 1);
            break;
          }
          case Data::TypeNone:
            break;
        }
      }
      break;

    case Node::TypePartial: {
      _beginTemplateTag();
      const std::string& name = _requireOwnedNodeData(node);
      std::vector<std::string_view> indentation;
      if (partialIndentation != NULL) {
        if (!_indentationStack.empty()) {
          indentation = _indentationStack.back().components;
        }
        if (!partialIndentation->empty()) {
          indentation.push_back(*partialIndentation);
        }
      }
      const auto renderPartial = [this, depth, &indentation](NodeView partial) {
        _indentationStack.emplace_back(std::move(indentation));
        const auto indentationGuard = onScopeExit([this]() {
          _indentationStack.pop_back();
        });
        _renderNode(partial, depth + 1);
      };
      bool partialFound = false;
      if (_partialResolver) {
        const Node * partial = _partialResolver(name);
        if (partial != NULL) {
          partialFound = true;
          renderPartial(NodeView::fromNode(partial));
        }
      }
      if (!partialFound && _partials != NULL) {
        const Node::Partials::const_iterator partial = _partials->find(name);
        if (partial != _partials->end() && partial->second != NULL) {
          partialFound = true;
          renderPartial(NodeView::fromNode(partial->second.get()));
        }
      }
      if (!partialFound) {
        const NodeView partial = NodeView::fromNode(_node).findPartial(name);
        if (partial) {
          renderPartial(partial);
        }
      }
      break;
    }

    default:
      throw Exception("Unknown tree node type");
  }
}

const Data * Renderer::_lookup(NodeView node)
{
  const Data * data = _stack.back();
  const std::string& name = _requireOwnedNodeData(node);

  if (name == ".") {
    return data;
  }

  if (data->type() == Data::TypeMap) {
    // Check top level
    const Data * found = data->find(name);
    if (found != NULL) {
      return found;
    }
  }

  // Stop here for strict paths
  if (this->_strictPaths) {
    return NULL;
  }

  // Get initial segment for dot notation
  const std::string * initial;
  if (node.dataPartCount() != 0) {
    initial = &node.dataPart(0);
  } else {
    initial = &name;
  }

  // Resolve up the data stack
  const Data * ref = NULL;
  for (std::vector<const Data *>::const_reverse_iterator stackPos = _stack.rbegin(); stackPos != _stack.rend();
      ++stackPos) {
    if (*stackPos != NULL && (*stackPos)->type() == Data::TypeMap) {
      ref = (*stackPos)->find(*initial);
      if (ref != NULL) {
        break;
      }
    }
  }

  // Resolve or dot notation
  if (ref != NULL && node.dataPartCount() > 1) {
    // Dot notation
    for (std::size_t index = 1; index < node.dataPartCount(); ++index) {
      if (ref == NULL) {
        break;
      } else if (ref->type() != Data::TypeMap) {
        ref = NULL; // Not sure about this
        break;
      } else {
        ref = ref->find(node.dataPart(index));
        if (ref == NULL) {
          ref = NULL; // Not sure about this
          break;
        }
      }
    }
  }

  return ref;
}

} // namespace mustache
