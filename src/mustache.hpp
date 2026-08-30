
#ifndef MUSTACHE_HPP
#define MUSTACHE_HPP

#include "mustache_config.h"
#include "mustache_export.hpp"

#include <stdint.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "archived_template.hpp"
#include "compiled_template.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "tokenizer.hpp"
#include "utils.hpp"

extern "C" MUSTACHE_API const char * mustache_version();
extern "C" MUSTACHE_API int mustache_version_int();

namespace mustache {

/*! \class Mustache
    \brief Container class for a tokenizer and a renderer

    This class contains the tokenizer and renderer.
*/
class Mustache {
  private:
  public:
    //! Tokenizer
    Tokenizer tokenizer;

    //! Renderer
    Renderer renderer;

    //! Utility method for Tokenizer::tokenize()
    MUSTACHE_API void tokenize(std::string * tmpl, Node * root);

    //! Utility method for Tokenizer::tokenize() with resource limits
    MUSTACHE_API void tokenize(std::string * tmpl, Node * root, const Tokenizer::Limits& limits);

    //! Utility method for Tokenizer::tokenize()
    MUSTACHE_API void tokenize(std::string_view tmpl, Node * root);

    //! Utility method for Tokenizer::tokenize() with resource limits
    MUSTACHE_API void tokenize(std::string_view tmpl, Node * root, const Tokenizer::Limits& limits);

    //! Compiles an immutable template handle
    MUSTACHE_API CompiledTemplate compile(std::string_view source);

    //! Compiles an immutable template handle with resource limits
    MUSTACHE_API CompiledTemplate compile(std::string_view source, const Tokenizer::Limits& limits);

    //! Utility method for Renderer::init() and Renderer::render()
    MUSTACHE_API void render(
        const Node * node, const Data * data, const Node::Partials * partials, std::string * output);

    //! Utility method for bounded Renderer::init() and Renderer::render()
    MUSTACHE_API void render(const Node * node, const Data * data, const Node::Partials * partials,
        std::string * output, const RenderLimits& limits);

    //! Renders an immutable compiled template
    MUSTACHE_API std::string render(const CompiledTemplate& compiled, const Data& data) const;

    //! Renders an immutable compiled template with resource limits
    MUSTACHE_API std::string render(
        const CompiledTemplate& compiled, const Data& data, const RenderLimits& limits) const;

    //! Renders an immutable compiled template with compiled partials
    MUSTACHE_API std::string render(
        const CompiledTemplate& compiled, const Data& data, const PartialMap& partials) const;

    //! Renders compiled templates and partials with resource limits
    MUSTACHE_API std::string render(const CompiledTemplate& compiled, const Data& data, const PartialMap& partials,
        const RenderLimits& limits) const;

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
    //! Renders a previously validated archived template
    MUSTACHE_API std::string render(const ArchivedTemplate& archived, const Data& data) const;

    //! Renders a previously validated archived template with resource limits
    MUSTACHE_API std::string render(
        const ArchivedTemplate& archived, const Data& data, const RenderLimits& limits) const;
#endif

    //! Utility method for Tokenizer::setStartSequence()
    void setStartSequence(const std::string& start)
    {
      return tokenizer.setStartSequence(start);
    };

    //! Utility method for Tokenizer::setStartSequence()
    void setStartSequence(std::string_view start)
    {
      tokenizer.setStartSequence(start);
    };

    //! Utility method for Tokenizer::setStartSequence()
    void setStartSequence(const char * start, long len = 0)
    {
      if (start == NULL) {
        throw Exception("Missing start delimiter");
      }
      if (len < 0) {
        throw Exception("Invalid start delimiter length");
      }
      if (len == 0) {
        tokenizer.setStartSequence(start);
      } else {
        tokenizer.setStartSequence(std::string_view(start, static_cast<size_t>(len)));
      }
    };

    //! Utility method for Tokenizer::setStopSequence()
    void setStopSequence(const std::string& stop)
    {
      tokenizer.setStopSequence(stop);
    };

    //! Utility method for Tokenizer::setStopSequence()
    void setStopSequence(std::string_view stop)
    {
      tokenizer.setStopSequence(stop);
    };

    //! Utility method for Tokenizer::setStopSequence()
    void setStopSequence(const char * stop, long len = 0)
    {
      if (stop == NULL) {
        throw Exception("Missing stop delimiter");
      }
      if (len < 0) {
        throw Exception("Invalid stop delimiter length");
      }
      if (len == 0) {
        tokenizer.setStopSequence(stop);
      } else {
        tokenizer.setStopSequence(std::string_view(stop, static_cast<size_t>(len)));
      }
    };

    //! Utility method for Tokenizer::setEscapeByDefault()
    void setEscapeByDefault(bool flag)
    {
      tokenizer.setEscapeByDefault(flag);
    };

    //! Utility method for Tokenizer::getStartSequence()
    const std::string& getStartSequence()
    {
      return tokenizer.getStartSequence();
    }

    //! Utility method for Tokenizer::getStopSequence()
    const std::string& getStopSequence()
    {
      return tokenizer.getStopSequence();
    };

    //! Utility method for Tokenizer::getEscapeByDefault()
    bool getEscapeByDefault()
    {
      return tokenizer.getEscapeByDefault();
    };
};

//! Compiles a template with default tokenizer settings
MUSTACHE_API CompiledTemplate compile(std::string_view source);

//! Compiles a template with explicit resource limits
MUSTACHE_API CompiledTemplate compile(std::string_view source, const Tokenizer::Limits& limits);

//! Renders a compiled template with no partials
MUSTACHE_API std::string render(const CompiledTemplate& compiled, const Data& data);

//! Renders a compiled template with explicit resource limits
MUSTACHE_API std::string render(const CompiledTemplate& compiled, const Data& data, const RenderLimits& limits);

//! Renders a compiled template with compiled partials
MUSTACHE_API std::string render(const CompiledTemplate& compiled, const Data& data, const PartialMap& partials);

//! Renders compiled templates and partials with explicit resource limits
MUSTACHE_API std::string render(
    const CompiledTemplate& compiled, const Data& data, const PartialMap& partials, const RenderLimits& limits);

} // namespace mustache

#endif
