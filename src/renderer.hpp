
#ifndef MUSTACHE_RENDERER_HPP
#define MUSTACHE_RENDERER_HPP

#include "mustache_export.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"

namespace mustache {

class Mustache;
class Tokenizer;

/*! \struct RenderLimits
    \brief Resource limits for one render operation.

    Every field is an enforced maximum. A zero value therefore rejects any
    render that consumes that resource; zero never means unlimited. The root
    template node counts as the first nesting level and the first node visit.
    Section text sent to lambdas and template text returned by lambdas share
    the byte budget across the complete render operation. Lambda-generated
    AST nodes are charged once when parsed and again when traversed. Nesting
    is always capped at 256 active nodes to protect the C++ call stack.
*/
struct RenderLimits {
    std::size_t maxOutputBytes;
    std::size_t maxNestingDepth;
    std::size_t maxNodeVisits;
    std::size_t maxLambdaTemplateBytes;

    MUSTACHE_API RenderLimits();
};

/*! \class Renderer
    \brief Renders a token tree

    This class renders a token tree. The Data tree must not be structurally
    modified while a render is in progress; doing so can invalidate borrowed
    entries in the renderer's lookup stack.
*/
class Renderer {
  private:
    class NodeView;

    typedef std::function<const Node *(const std::string&)> PartialResolver;

    struct IndentationFrame {
        explicit IndentationFrame(std::vector<std::string_view> components = {}) :
            components(std::move(components)),
            atLineStart(true)
        {}

        std::vector<std::string_view> components;
        bool atLineStart;
    };

    //! The root token node
    const Node * _node;

    //! The root data node
    const Data * _data;

    //! The data stack
    std::vector<const Data *> _stack;

    //! Partials
    const Node::Partials * _partials;

    //! Optional lookup for opaque compiled partials
    PartialResolver _partialResolver;

    //! Current output buffer
    std::string * _output;

    //! Resource policy for the current render
    RenderLimits _limits;

    //! Aggregate work consumed by the current render
    std::size_t _outputBytes;
    std::size_t _nodeVisits;
    std::size_t _lambdaTemplateBytes;

    //! Source-line state scoped to the currently rendered partial
    std::vector<IndentationFrame> _indentationStack;

    //! Active recursion level for callback-scoped rendering
    std::size_t _activeDepth;

    //! Whether a render operation is active
    bool _rendering;

    //! Number of active section-lambda callback frames
    std::size_t _lambdaCallbackDepth;

    //! Renders a single node
    void _renderNode(NodeView node, std::size_t depth, const std::string * partialIndentation = NULL,
        bool partialIndentationMetadata = false);

    //! Renders all children one level below their parent
    void _renderChildren(NodeView node, std::size_t depth);

    //! Appends bounded output
    void _append(std::string_view value);
    void _appendEscaped(std::string_view value);

    //! Applies indentation to literal partial source, never dynamic values
    void _appendTemplateOutput(std::string_view value);
    void _appendIndentation(const IndentationFrame& frame);
    void _consumeTemplateSource(std::string_view value);
    void _beginTemplateTag();

    //! Accounts for output across every buffer used by this render
    void _consumeOutputBytes(std::size_t bytes);

    //! Accounts for a lambda-generated template before parsing it
    void _consumeLambdaTemplate(std::size_t bytes);

    //! Accounts for one renderer or lambda-source node traversal
    void _consumeNodeVisit();

    //! Parses and charges every node in a lambda-generated AST
    void _tokenizeLambda(Tokenizer * tokenizer, std::string_view source, Node * root, bool escapeOutput);
    void _consumeLambdaNodes(const Node * node);

    //! Reconstructs bounded section text for a lambda callback
    std::string _lambdaSectionText(NodeView node, std::string_view start, std::string_view stop, std::size_t depth);
    void _appendLambdaNodeTemplate(
        NodeView node, std::string_view start, std::string_view stop, std::string * output, std::size_t depth);
    void _appendLambdaTemplate(std::string * output, std::string_view value);

    const Data * _lookup(NodeView node);

    static std::string_view _requireNodeData(NodeView node);
    static const std::string& _requireOwnedNodeData(NodeView node);
    static const std::string * _validatePartialIndentationAt(NodeView parent, std::size_t index);

    void setPartialResolver(PartialResolver resolver);

    friend class Mustache;

    bool _strictPaths;

  public:
    //! The default output buffer length
    static const int outputBufferLength = 1000;

    //! Constructor
    MUSTACHE_API Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    //! Destructor
    MUSTACHE_API ~Renderer();

    //! Clears any assigned values
    MUSTACHE_API void clear();

    //! Initializes the renderer
    MUSTACHE_API void init(const Node * node, const Data * data, const Node::Partials * partials, std::string * output);

    //! Initializes the renderer with an explicit resource policy
    MUSTACHE_API void init(const Node * node, const Data * data, const Node::Partials * partials, std::string * output,
        const RenderLimits& limits);

    //! Sets the current root token node
    MUSTACHE_API void setNode(const Node * node);

    //! Sets the current root data node
    MUSTACHE_API void setData(const Data * data);

    //! Sets the current partials
    MUSTACHE_API void setPartials(const Node::Partials * partials);

    //! Sets the current output buffer
    MUSTACHE_API void setOutput(std::string * output);

    //! Renders using the stored variables
    MUSTACHE_API void render();

    /*! Renders the given node to the given output during a lambda callback.

        This operation is valid only while a section-lambda callback is active
        on this renderer. A retained renderer pointer is rejected when no such
        callback is active.
    */
    MUSTACHE_API void renderForLambda(const Node * node, std::string * output);
};

} // namespace mustache

#endif
