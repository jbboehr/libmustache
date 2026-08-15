#ifndef MUSTACHE_COMPILED_TEMPLATE_HPP
#define MUSTACHE_COMPILED_TEMPLATE_HPP

#include "mustache_export.hpp"

#include <map>
#include <memory>
#include <string>

namespace mustache {

class Mustache;

/*! \class CompiledTemplate
    \brief Opaque, immutable compiled template handle.

    Compiled templates own their parsed representation through immutable
    shared state. Copies are inexpensive and remain valid independently of the
    source string and the Mustache instance that compiled them.
*/
class CompiledTemplate {
  public:
    MUSTACHE_API CompiledTemplate() noexcept;
    MUSTACHE_API CompiledTemplate(
        const CompiledTemplate& other) noexcept;
    MUSTACHE_API CompiledTemplate& operator=(
        const CompiledTemplate& other) noexcept;
    MUSTACHE_API CompiledTemplate(CompiledTemplate&& other) noexcept;
    MUSTACHE_API CompiledTemplate& operator=(
        CompiledTemplate&& other) noexcept;
    MUSTACHE_API ~CompiledTemplate();

    //! Returns true when this handle contains no compiled template.
    MUSTACHE_API bool empty() const noexcept;

    //! Tests whether this handle contains a compiled template.
    explicit MUSTACHE_API operator bool() const noexcept;

  private:
    struct State;

    explicit CompiledTemplate(std::shared_ptr<const State> state) noexcept;

    std::shared_ptr<const State> state;

    friend class Mustache;
};

//! A map of named, independently owned compiled partial templates.
typedef std::map<std::string, CompiledTemplate> PartialMap;

} // namespace mustache

#endif
