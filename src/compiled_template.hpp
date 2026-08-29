#ifndef MUSTACHE_COMPILED_TEMPLATE_HPP
#define MUSTACHE_COMPILED_TEMPLATE_HPP

#include "mustache_config.h"
#include "mustache_export.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mustache {

class Mustache;
#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
struct ArchivedTemplateLimits;
#endif

/*! \class CompiledTemplate
    \brief Opaque, immutable compiled template handle.

    Compiled templates own their parsed representation through immutable
    shared state. Copies are inexpensive and remain valid independently of the
    source string and the Mustache instance that compiled them.
*/
class CompiledTemplate {
  public:
    MUSTACHE_API CompiledTemplate() noexcept;
    MUSTACHE_API CompiledTemplate(const CompiledTemplate& other) noexcept;
    MUSTACHE_API CompiledTemplate& operator=(const CompiledTemplate& other) noexcept;
    MUSTACHE_API CompiledTemplate(CompiledTemplate&& other) noexcept;
    MUSTACHE_API CompiledTemplate& operator=(CompiledTemplate&& other) noexcept;
    MUSTACHE_API ~CompiledTemplate();

    //! Returns true when this handle contains no compiled template.
    MUSTACHE_API bool empty() const noexcept;

    //! Tests whether this handle contains a compiled template.
    explicit MUSTACHE_API operator bool() const noexcept;

  private:
    struct State;

    explicit CompiledTemplate(std::shared_ptr<const State> state) noexcept;

    std::shared_ptr<const State> state;

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
#if defined(__GNUC__) && !defined(_WIN32)
    __attribute__((visibility("hidden")))
#endif
    const void * archivedTemplateRoot() const noexcept;

    friend MUSTACHE_API std::vector<std::uint8_t> serializeArchivedTemplate(const CompiledTemplate& compiled,
        const std::map<std::string, CompiledTemplate>& partials, const ArchivedTemplateLimits& limits);
#endif
    friend class Mustache;
};

//! A map of named, independently owned compiled partial templates.
typedef std::map<std::string, CompiledTemplate> PartialMap;

} // namespace mustache

#endif
