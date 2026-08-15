
#ifndef MUSTACHE_LAMBDA_HPP
#define MUSTACHE_LAMBDA_HPP

#include "mustache_export.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace mustache {

class Node;
class Renderer;

/*! \class LambdaRenderContext
    \brief Callback-scoped access to the active renderer.

    Copies share one callback frame. Once that callback returns or throws,
    every retained copy becomes inactive and render() throws a library
    exception without accessing the former Renderer object. Contexts are not
    safe for concurrent rendering.
*/
class LambdaRenderContext {
  private:
    struct State;
    std::shared_ptr<State> state;

    explicit LambdaRenderContext(Renderer * renderer);
    Renderer * legacyRenderer() const;
    void invalidate() noexcept;

    friend class Lambda;
    friend class Renderer;

  public:
    //! Constructs an inactive context.
    LambdaRenderContext() noexcept = default;

    LambdaRenderContext(const LambdaRenderContext&) noexcept = default;
    LambdaRenderContext& operator=(
        const LambdaRenderContext&) noexcept = default;
    LambdaRenderContext(LambdaRenderContext&&) noexcept = default;
    LambdaRenderContext& operator=(
        LambdaRenderContext&&) noexcept = default;
    ~LambdaRenderContext() = default;

    //! Returns whether this callback frame remains active.
    MUSTACHE_API bool active() const;

    //! Renders a node through the active callback frame.
    MUSTACHE_API void render(
        const Node& node, std::string& output) const;

    //! Renders a node and returns owned output.
    MUSTACHE_API std::string render(const Node& node) const;
};

/*! \class Lambda
    \brief A callable for use within mustache::Data.
*/
class MUSTACHE_API Lambda {
  public:
    //! Destructor
    virtual ~Lambda() {};

    //! Invoke this lambda if it's being used as a variable
    virtual std::string invoke() = 0;

    /*! Invoke this lambda through the legacy section callback.

        Existing implementations may continue overriding this method. New
        implementations should override the LambdaRenderContext overload
        below instead. The default implementation throws.
    */
    virtual std::string invoke(std::string * text, Renderer * renderer);

    /*! Invoke this lambda with explicitly sized section text.

        Existing implementations remain compatible through the owning-string
        adapter. New callers should prefer this overload so embedded NULs and
        non-NUL-terminated inputs retain their explicit length.
    */
    std::string invoke(std::string_view text, Renderer * renderer) {
      std::string ownedText(text);
      return invoke(&ownedText, renderer);
    }

    /*! Invoke this lambda with a lifetime-safe render capability.

        The default implementation adapts to the legacy virtual method. New
        implementations may override only this overload for section use.
    */
    virtual std::string invoke(
        std::string_view text, LambdaRenderContext context);
};


} // namespace Mustache

#endif
