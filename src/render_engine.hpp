#ifndef MUSTACHE_RENDER_ENGINE_HPP
#define MUSTACHE_RENDER_ENGINE_HPP

#include "renderer.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tokenizer.hpp"

namespace mustache {

/*! Callback-only bridge to the currently specialized render engine.

    Ordinary traversal remains statically dispatched. This bridge exists so a
    LambdaRenderContext can re-enter the same partial source with an owned Node
    while an archive-backed engine is active.
*/
class Renderer::ActiveRenderEngine {
  public:
    virtual void renderOwnedNode(const Node * node, std::size_t depth) = 0;

  protected:
    virtual ~ActiveRenderEngine() = default;
};

/*! Callback-scoped engine registry that does not change Renderer object layout.

    The registry is process-wide rather than thread-local because a callback
    may hand its scoped rendering capability to a worker and wait for it. The
    callback frame keeps the engine alive while a serialized worker lookup is
    in progress.
*/
class Renderer::ActiveEngineScope {
  public:
    ActiveEngineScope(Renderer * renderer, ActiveRenderEngine * engine) :
        renderer_(renderer),
        engine_(engine),
        previous_(NULL)
    {
      assert(renderer_ != NULL);
      assert(engine_ != NULL);
      const std::lock_guard<std::mutex> lock(registryMutex_);
      previous_ = current_;
      current_ = this;
    }

    ActiveEngineScope(const ActiveEngineScope&) = delete;
    ActiveEngineScope& operator=(const ActiveEngineScope&) = delete;

    ~ActiveEngineScope() noexcept
    {
      const std::lock_guard<std::mutex> lock(registryMutex_);
      ActiveEngineScope ** link = &current_;
      while (*link != NULL && *link != this) {
        link = &(*link)->previous_;
      }
      assert(*link == this);
      if (*link == this) {
        *link = previous_;
      }
    }

    static ActiveRenderEngine * find(const Renderer * renderer)
    {
      const std::lock_guard<std::mutex> lock(registryMutex_);
      for (ActiveEngineScope * scope = current_; scope != NULL; scope = scope->previous_) {
        if (scope->renderer_ == renderer) {
          return scope->engine_;
        }
      }
      return NULL;
    }

  private:
    Renderer * renderer_;
    ActiveRenderEngine * engine_;
    ActiveEngineScope * previous_;
    static std::mutex registryMutex_;
    static ActiveEngineScope * current_;
};

namespace detail {

inline constexpr std::size_t renderNestingCeiling = 256;

template <typename Callable> class RenderScopeExit {
  public:
    explicit RenderScopeExit(Callable callable) :
        callable_(std::move(callable))
    {}

    RenderScopeExit(const RenderScopeExit&) = delete;
    RenderScopeExit& operator=(const RenderScopeExit&) = delete;

    ~RenderScopeExit() noexcept
    {
      callable_();
    }

  private:
    Callable callable_;
};

template <typename Callable> RenderScopeExit<Callable> onRenderScopeExit(Callable callable)
{
  return RenderScopeExit<Callable>(std::move(callable));
}

/*! Optional immutable string borrowed from a render backend.

    Owned nodes retain their std::string identity so current Data and partial
    lookups do not allocate. Archive adapters can provide a plain string_view;
    the shared engine materializes only at legacy std::string boundaries.
*/
class RenderString {
  public:
    RenderString() noexcept :
        value_(),
        owned_(NULL),
        present_(false)
    {}

    static RenderString fromOwned(const std::string * value) noexcept
    {
      return value == NULL ? RenderString() : RenderString(*value, value);
    }

    static RenderString fromView(std::string_view value) noexcept
    {
      return RenderString(value, NULL);
    }

    explicit operator bool() const noexcept
    {
      return present_;
    }

    std::string_view value() const noexcept
    {
      assert(present_);
      return value_;
    }

    const std::string * ownedString() const noexcept
    {
      assert(present_);
      return owned_;
    }

  private:
    RenderString(std::string_view value, const std::string * owned) noexcept :
        value_(value),
        owned_(owned),
        present_(true)
    {}

    std::string_view value_;
    const std::string * owned_;
    bool present_;
};

/*! Read-only adapter for the existing owned Node representation. */
class OwnedNodeView {
  public:
    class DataPartCursor {
      public:
        DataPartCursor() noexcept :
            parts_(NULL),
            index_(0)
        {}

        explicit operator bool() const noexcept
        {
          return parts_ != NULL && index_ < parts_->size();
        }

        RenderString value() const noexcept
        {
          assert(static_cast<bool>(*this));
          return RenderString::fromOwned(&(*parts_)[index_]);
        }

        void advance() noexcept
        {
          assert(static_cast<bool>(*this));
          ++index_;
        }

      private:
        friend class OwnedNodeView;

        explicit DataPartCursor(const std::vector<std::string> * parts) noexcept :
            parts_(parts),
            index_(0)
        {}

        const std::vector<std::string> * parts_;
        std::size_t index_;
    };

    class ChildCursor {
      public:
        ChildCursor() noexcept :
            children_(NULL),
            index_(0)
        {}

        explicit operator bool() const noexcept
        {
          return children_ != NULL && index_ < children_->size();
        }

        OwnedNodeView value() const noexcept
        {
          assert(static_cast<bool>(*this));
          return OwnedNodeView::fromNode((*children_)[index_].get());
        }

        void advance() noexcept
        {
          assert(static_cast<bool>(*this));
          ++index_;
        }

      private:
        friend class OwnedNodeView;

        explicit ChildCursor(const Node::Children * children) noexcept :
            children_(children),
            index_(0)
        {}

        const Node::Children * children_;
        std::size_t index_;
    };

    OwnedNodeView() noexcept :
        node_(NULL)
    {}

    static OwnedNodeView fromNode(const Node * node) noexcept
    {
      return OwnedNodeView(node);
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

    RenderString data() const noexcept
    {
      assert(node_ != NULL);
      return RenderString::fromOwned(node_->data.has_value() ? &*node_->data : NULL);
    }

    DataPartCursor dataParts() const noexcept
    {
      assert(node_ != NULL);
      return DataPartCursor(&node_->dataParts);
    }

    ChildCursor children() const noexcept
    {
      assert(node_ != NULL);
      return ChildCursor(&node_->children);
    }

    OwnedNodeView containerChild() const noexcept
    {
      assert(node_ != NULL);
      return OwnedNodeView(node_->child.get());
    }

    RenderString startSequence() const noexcept
    {
      assert(node_ != NULL);
      return RenderString::fromOwned(node_->startSequence.has_value() ? &*node_->startSequence : NULL);
    }

    RenderString stopSequence() const noexcept
    {
      assert(node_ != NULL);
      return RenderString::fromOwned(node_->stopSequence.has_value() ? &*node_->stopSequence : NULL);
    }

  private:
    explicit OwnedNodeView(const Node * node) noexcept :
        node_(node)
    {}

    const Node * node_;
};

/*! Partial lookup policy for the owned renderer entry points.

    The order is the established compatibility contract: opaque compiled
    resolver, caller-supplied partial map, then partials owned by the root.
*/
class OwnedPartialSource {
  public:
    using Resolver = std::function<const Node *(const std::string&)>;

    OwnedPartialSource(const Resolver * resolver, const Node::Partials * external, const Node * root) noexcept :
        resolver_(resolver),
        external_(external),
        root_(root)
    {}

    template <typename Callback> bool withPartial(RenderString name, Callback&& callback) const
    {
      assert(name);
      std::string materialized;
      const std::string * key = name.ownedString();
      if (key == NULL) {
        const std::string_view view = name.value();
        if (!view.empty()) {
          materialized.assign(view.data(), view.size());
        }
        key = &materialized;
      }

      if (resolver_ != NULL && *resolver_) {
        const Node * partial = (*resolver_)(*key);
        if (partial != NULL) {
          std::forward<Callback>(callback)(OwnedNodeView::fromNode(partial));
          return true;
        }
      }
      if (external_ != NULL) {
        const Node::Partials::const_iterator partial = external_->find(*key);
        if (partial != external_->end() && partial->second != NULL) {
          std::forward<Callback>(callback)(OwnedNodeView::fromNode(partial->second.get()));
          return true;
        }
      }
      if (root_ != NULL) {
        const Node::Partials::const_iterator partial = root_->partials.find(*key);
        if (partial != root_->partials.end() && partial->second != NULL) {
          std::forward<Callback>(callback)(OwnedNodeView::fromNode(partial->second.get()));
          return true;
        }
      }
      return false;
    }

  private:
    const Resolver * resolver_;
    const Node::Partials * external_;
    const Node * root_;
};

inline bool isPartialIndentation(std::string_view value)
{
  return std::all_of(value.begin(), value.end(), [](char character) {
    return character == ' ' || character == '\t';
  });
}

/*! Rendering algorithm shared by compile-time node and partial adapters. */
template <typename PartialSource> class RenderEngine final : public Renderer::ActiveRenderEngine {
  public:
    RenderEngine(Renderer& renderer, PartialSource& partialSource) noexcept :
        renderer_(renderer),
        partialSource_(partialSource)
    {}

    void renderOwnedNode(const Node * node, std::size_t depth) override
    {
      renderNode(OwnedNodeView::fromNode(node), depth);
    }

    template <typename NodeView> void renderRoot(NodeView root)
    {
      if (renderer_._rendering) {
        throw Exception("Renderer is already rendering");
      }
      if (!root) {
        throw Exception("Empty tree");
      }
      if (renderer_._data == NULL) {
        throw Exception("Empty data");
      }
      if (renderer_._output == NULL) {
        throw Exception("Missing output buffer");
      }
      if (renderer_._output->size() > renderer_._limits.maxOutputBytes) {
        throw Exception("Render output byte limit exceeded");
      }

      renderer_._rendering = true;
      renderer_._outputBytes = renderer_._output->size();
      renderer_._nodeVisits = 0;
      renderer_._lambdaTemplateBytes = 0;
      renderer_._indentationStack.clear();
      renderer_._activeDepth = 0;
      renderer_._lambdaCallbackDepth = 0;
      renderer_._stack.clear();
      const auto renderGuard = onRenderScopeExit([this]() {
        renderer_._stack.clear();
        renderer_._indentationStack.clear();
        renderer_._activeDepth = 0;
        renderer_._lambdaCallbackDepth = 0;
        renderer_._rendering = false;
      });

      if (renderer_._output->empty() && renderer_._output->capacity() == 0) {
        const std::size_t reserveBytes =
            std::min(static_cast<std::size_t>(Renderer::outputBufferLength), renderer_._limits.maxOutputBytes);
        renderer_._output->reserve(reserveBytes);
      }

      renderer_._stack.push_back(renderer_._data);
      renderNode(root, 0);
    }

    template <typename NodeView>
    void renderNode(NodeView node, std::size_t depth, RenderString partialIndentation = RenderString(),
        bool partialIndentationMetadata = false)
    {
      if (!node) {
        throw Exception("Empty tree node");
      }
      if (depth >= renderer_._limits.maxNestingDepth || depth >= renderNestingCeiling) {
        throw Exception("Render nesting limit exceeded");
      }
      renderer_._consumeNodeVisit();

      if ((node.flags() & Node::FlagPartialIndent) != 0) {
        const RenderString metadata = node.data();
        if (!partialIndentationMetadata || node.type() != Node::TypeOutput ||
            node.flags() != (Node::FlagLambdaOnly | Node::FlagPartialIndent) || !metadata ||
            !isPartialIndentation(metadata.value())) {
          throw Exception("Invalid standalone partial indentation metadata");
        }
      }

      const std::size_t parentDepth = renderer_._activeDepth;
      renderer_._activeDepth = depth;
      const auto depthGuard = onRenderScopeExit([this, parentDepth]() {
        renderer_._activeDepth = parentDepth;
      });

      if (renderer_._stack.empty()) {
        throw Exception("Whoops, empty data");
      }
      if (!(node.type() & Node::TypeHasNoString) && !node.data()) {
        throw Exception("Whoops, empty tag");
      }

      const Data * value = NULL;
      if (node.type() & Node::TypeHasData) {
        value = lookup(node);
      }
      const bool valueIsEmpty = value == NULL || value->isEmpty();

      const auto renderWithContext = [this, node, depth](const Data * context) {
        renderer_._stack.push_back(context);
        const auto stackGuard = onRenderScopeExit([this]() {
          renderer_._stack.pop_back();
        });
        renderChildren(node, depth);
      };

      switch (node.type()) {
        case Node::TypeNone:
          break;

        case Node::TypeComment:
        case Node::TypeStop:
        case Node::TypeInlinePartial:
          renderer_._beginTemplateTag();
          break;

        case Node::TypeRoot:
          renderChildren(node, depth);
          break;

        case Node::TypeOutput: {
          const RenderString output = node.data();
          if (output) {
            if (node.flags() & Node::FlagLambdaOnly) {
              renderer_._consumeTemplateSource(output.value());
            } else {
              renderer_._appendTemplateOutput(output.value());
            }
          }
          break;
        }

        case Node::TypeContainer:
          if (!node.containerChild()) {
            throw Exception("Empty container node");
          }
          renderNode(node.containerChild(), depth + 1);
          break;

        case Node::TypeTag:
        case Node::TypeVariable:
          renderer_._beginTemplateTag();
          if (!valueIsEmpty) {
            renderValue(node, *value, depth);
          }
          break;

        case Node::TypeNegate:
          renderer_._beginTemplateTag();
          if (valueIsEmpty) {
            renderChildren(node, depth);
          }
          break;

        case Node::TypeSection:
          renderer_._beginTemplateTag();
          if (!valueIsEmpty) {
            renderSection(node, *value, depth, renderWithContext);
          }
          break;

        case Node::TypePartial:
          renderPartial(node, depth, partialIndentation);
          break;

        default:
          throw Exception("Unknown tree node type");
      }
    }

  private:
    template <typename NodeView> static std::string_view requireNodeData(NodeView node)
    {
      const RenderString data = node.data();
      if (!data) {
        throw Exception("Invalid node without data");
      }
      return data.value();
    }

    static const Data * findInMap(const Data * data, RenderString key)
    {
      if (data == NULL || data->type() != Data::TypeMap) {
        return NULL;
      }
      if (const std::string * owned = key.ownedString()) {
        return data->find(*owned);
      }
      const std::string_view view = key.value();
      std::string materialized;
      if (!view.empty()) {
        materialized.assign(view.data(), view.size());
      }
      return data->find(materialized);
    }

    template <typename NodeView> RenderString validatePartialIndentation(NodeView child, NodeView nextChild)
    {
      if (!child || (child.flags() & Node::FlagPartialIndent) == 0) {
        return RenderString();
      }
      const RenderString data = child.data();
      if (child.type() != Node::TypeOutput || child.flags() != (Node::FlagLambdaOnly | Node::FlagPartialIndent) ||
          !data || !isPartialIndentation(data.value()) || !nextChild || nextChild.type() != Node::TypePartial) {
        throw Exception("Invalid standalone partial indentation metadata");
      }
      return data;
    }

    template <typename NodeView> void renderChildren(NodeView node, std::size_t depth)
    {
      auto children = node.children();
      while (children) {
        const NodeView child = children.value();
        children.advance();
        const NodeView nextChild = children ? children.value() : NodeView();
        const RenderString partialIndentation = validatePartialIndentation(child, nextChild);
        if (partialIndentation) {
          renderNode(child, depth + 1, RenderString(), true);
          renderNode(nextChild, depth + 1, partialIndentation);
          children.advance();
          continue;
        }
        renderNode(child, depth + 1);
      }
    }

    template <typename NodeView>
    void appendLambdaNodeTemplate(
        NodeView node, std::string_view start, std::string_view stop, std::string * output, std::size_t depth)
    {
      if (!node) {
        throw Exception("Invalid null child node");
      }
      if (depth >= renderer_._limits.maxNestingDepth || depth >= renderNestingCeiling) {
        throw Exception("Render nesting limit exceeded");
      }
      renderer_._consumeNodeVisit();
      if (!(node.type() & Node::TypeHasNoString) && !node.data()) {
        throw Exception("Invalid node without data");
      }

      bool appendChildren = false;
      switch (node.type()) {
        case Node::TypeComment:
          renderer_._appendLambdaTemplate(output, start);
          renderer_._appendLambdaTemplate(output, "!");
          renderer_._appendLambdaTemplate(output, requireNodeData(node));
          renderer_._appendLambdaTemplate(output, stop);
          break;
        case Node::TypeOutput:
          renderer_._appendLambdaTemplate(output, requireNodeData(node));
          break;
        case Node::TypePartial:
          renderer_._appendLambdaTemplate(output, start);
          renderer_._appendLambdaTemplate(output, ">");
          renderer_._appendLambdaTemplate(output, requireNodeData(node));
          renderer_._appendLambdaTemplate(output, stop);
          break;
        case Node::TypeNegate:
        case Node::TypeSection:
        case Node::TypeStop:
        case Node::TypeVariable:
          renderer_._appendLambdaTemplate(output, start);
          if (node.type() == Node::TypeVariable && !(node.flags() & Node::FlagEscape)) {
            renderer_._appendLambdaTemplate(output, "&");
          }
          if (node.type() == Node::TypeNegate) {
            renderer_._appendLambdaTemplate(output, "^");
          } else if (node.type() == Node::TypeSection) {
            renderer_._appendLambdaTemplate(output, "#");
          } else if (node.type() == Node::TypeStop) {
            renderer_._appendLambdaTemplate(output, "/");
          }
          renderer_._appendLambdaTemplate(output, requireNodeData(node));
          renderer_._appendLambdaTemplate(output, stop);
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
        auto children = node.children();
        while (children) {
          const NodeView child = children.value();
          children.advance();
          const NodeView nextChild = children ? children.value() : NodeView();
          validatePartialIndentation(child, nextChild);
          appendLambdaNodeTemplate(child, start, stop, output, depth + 1);
        }
      }
    }

    template <typename NodeView>
    std::string lambdaSectionText(NodeView node, std::string_view start, std::string_view stop, std::size_t depth)
    {
      std::string output;
      auto children = node.children();
      while (children) {
        const NodeView child = children.value();
        children.advance();
        const NodeView nextChild = children ? children.value() : NodeView();
        validatePartialIndentation(child, nextChild);
        if (!child) {
          throw Exception("Invalid null child node");
        }
        if (child.type() == Node::TypeStop) {
          if (depth + 1 >= renderer_._limits.maxNestingDepth || depth + 1 >= renderNestingCeiling) {
            throw Exception("Render nesting limit exceeded");
          }
          renderer_._consumeNodeVisit();
        } else {
          appendLambdaNodeTemplate(child, start, stop, &output, depth + 1);
        }
      }
      return output;
    }

    template <typename NodeView> const Data * lookup(NodeView node)
    {
      const Data * data = renderer_._stack.back();
      const RenderString name = node.data();
      if (!name) {
        throw Exception("Invalid node without data");
      }

      if (name.value() == ".") {
        return data;
      }

      if (const Data * found = findInMap(data, name)) {
        return found;
      }

      if (renderer_._strictPaths) {
        return NULL;
      }

      auto dataParts = node.dataParts();
      const RenderString initial = dataParts ? dataParts.value() : name;
      const Data * reference = NULL;
      for (std::vector<const Data *>::const_reverse_iterator position = renderer_._stack.rbegin();
          position != renderer_._stack.rend(); ++position) {
        reference = findInMap(*position, initial);
        if (reference != NULL) {
          break;
        }
      }

      if (dataParts) {
        dataParts.advance();
      }
      while (reference != NULL && dataParts) {
        if (reference->type() != Data::TypeMap) {
          return NULL;
        }
        reference = findInMap(reference, dataParts.value());
        dataParts.advance();
        if (reference == NULL) {
          break;
        }
      }
      return reference;
    }

    template <typename NodeView> void renderValue(NodeView node, const Data& value, std::size_t depth)
    {
      switch (value.type()) {
        case Data::TypeString:
          if (node.flags() & Node::FlagEscape) {
            renderer_._appendEscaped(value.stringValue());
          } else {
            renderer_._append(value.stringValue());
          }
          break;
        case Data::TypeBoolean:
        case Data::TypeInteger:
        case Data::TypeDouble: {
          const std::string rendered = value.toString();
          if (node.flags() & Node::FlagEscape) {
            renderer_._appendEscaped(rendered);
          } else {
            renderer_._append(rendered);
          }
          break;
        }
        case Data::TypeLambda: {
          std::string invoked = value.lambdaValue()->invoke();
          renderer_._consumeLambdaTemplate(invoked.size());

          Tokenizer tokenizer;
          Node nodeFromLambda;
          renderer_._tokenizeLambda(&tokenizer, invoked, &nodeFromLambda, node.flags() & Node::FlagEscape);
          renderer_._indentationStack.emplace_back();
          const auto indentationGuard = onRenderScopeExit([this]() {
            renderer_._indentationStack.pop_back();
          });
          renderNode(OwnedNodeView::fromNode(&nodeFromLambda), depth + 1);
          break;
        }
        case Data::TypeNone:
        case Data::TypeList:
        case Data::TypeMap:
        case Data::TypeArray:
          break;
      }
    }

    template <typename NodeView, typename RenderWithContext>
    void renderSection(NodeView node, const Data& value, std::size_t depth, RenderWithContext&& renderWithContext)
    {
      switch (value.type()) {
        case Data::TypeString:
        case Data::TypeBoolean:
        case Data::TypeInteger:
        case Data::TypeDouble:
        case Data::TypeMap:
          renderWithContext(&value);
          break;
        case Data::TypeList:
          for (const Data& child : value.listItems()) {
            renderWithContext(&child);
          }
          break;
        case Data::TypeArray:
          for (const Data& child : value.arrayItems()) {
            renderWithContext(&child);
          }
          break;
        case Data::TypeLambda: {
          const RenderString startSequence = node.startSequence();
          const RenderString stopSequence = node.stopSequence();
          if (!startSequence || !stopSequence) {
            throw Exception("Missing section delimiters");
          }
          const std::string_view start = startSequence.value();
          const std::string_view stop = stopSequence.value();
          const std::string text = lambdaSectionText(node, start, stop, depth);
          const std::string invoked = renderer_._invokeSectionLambda(value.lambdaValue(), text, this);
          renderer_._consumeLambdaTemplate(invoked.size());

          Tokenizer tokenizer;
          tokenizer.setStartSequence(start);
          tokenizer.setStopSequence(stop);
          Node nodeFromLambda;
          renderer_._tokenizeLambda(&tokenizer, invoked, &nodeFromLambda, node.flags() & Node::FlagEscape);
          renderer_._indentationStack.emplace_back();
          const auto indentationGuard = onRenderScopeExit([this]() {
            renderer_._indentationStack.pop_back();
          });
          renderNode(OwnedNodeView::fromNode(&nodeFromLambda), depth + 1);
          break;
        }
        case Data::TypeNone:
          break;
      }
    }

    template <typename NodeView> void renderPartial(NodeView node, std::size_t depth, RenderString partialIndentation)
    {
      renderer_._beginTemplateTag();
      const RenderString name = node.data();
      if (!name) {
        throw Exception("Invalid node without data");
      }
      std::vector<std::string_view> indentation;
      if (partialIndentation) {
        if (!renderer_._indentationStack.empty()) {
          indentation = renderer_._indentationStack.back().components;
        }
        if (!partialIndentation.value().empty()) {
          indentation.push_back(partialIndentation.value());
        }
      }
      const auto renderResolved = [this, depth, &indentation](auto partial) {
        renderer_._indentationStack.emplace_back(std::move(indentation));
        const auto indentationGuard = onRenderScopeExit([this]() {
          renderer_._indentationStack.pop_back();
        });
        renderNode(partial, depth + 1);
      };
      static_cast<void>(partialSource_.withPartial(name, renderResolved));
    }

    Renderer& renderer_;
    PartialSource& partialSource_;
};

} // namespace detail
} // namespace mustache

#endif
