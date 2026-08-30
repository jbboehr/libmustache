#ifndef MUSTACHE_ARCHIVED_TEMPLATE_HPP
#define MUSTACHE_ARCHIVED_TEMPLATE_HPP

#include "mustache_config.h"
#include "mustache_export.hpp"

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)

#include "compiled_template.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mustache {

class Data;
class Mustache;
class ArchivedTemplate;

//! Stable categories for failures while loading archived-template bytes.
enum class ArchivedTemplateError {
  InvalidArchive,
  UnsupportedFormat,
  LimitExceeded,
};

/*! A categorized failure raised only while loading an archived template.

    Existing callers may continue catching mustache::Exception. New callers
    can catch this derived type and inspect reason() without parsing what().
    Callers should retain a default branch when switching on the reason so
    future categories can be added compatibly.
*/
class MUSTACHE_API ArchivedTemplateException : public Exception {
  public:
    ArchivedTemplateException(ArchivedTemplateError reason, const std::string& desc);

    //! Returns the stable category without parsing the diagnostic message.
    ArchivedTemplateError reason() const noexcept;

  private:
    ArchivedTemplateError reason_;
};

/*! Resource limits applied while creating or validating an archived template.

    Every field is an enforced maximum. A zero value therefore rejects any
    archive that consumes that resource; zero never means unlimited.
    maxArchiveBytes applies to the complete framed archive on both writer and
    loader paths. The maxTotalStringBytes and maxTotalDataParts budgets are
    aggregate limits across the complete archived template and its partials.
*/
struct ArchivedTemplateLimits {
    std::size_t maxArchiveBytes;
    std::size_t maxNestingDepth;
    std::size_t maxNodes;
    std::size_t maxTotalStringBytes;
    std::size_t maxDataPartsPerNode;
    std::size_t maxTotalDataParts;

    MUSTACHE_API ArchivedTemplateLimits();
};

//! Returns an opaque identifier suitable for archived-template cache keys.
MUSTACHE_API std::string_view archivedTemplateCompatibilityTag() noexcept;

/*! Copies archive bytes into owned storage and validates them once.

    \throws ArchivedTemplateException with a machine-readable loading failure
    category. Allocation failures continue to propagate as std::bad_alloc.
*/
MUSTACHE_API ArchivedTemplate loadArchivedTemplate(
    const std::vector<std::uint8_t>& bytes, const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

/*! Copies archive bytes into owned storage and validates them once.

    \throws ArchivedTemplateException with a machine-readable loading failure
    category. Allocation failures continue to propagate as std::bad_alloc.
*/
MUSTACHE_API ArchivedTemplate loadArchivedTemplate(
    std::string_view bytes, const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

//! Renders a previously validated archived template.
MUSTACHE_API std::string render(const ArchivedTemplate& archived, const Data& data);

//! Renders a previously validated archived template with resource limits.
MUSTACHE_API std::string render(const ArchivedTemplate& archived, const Data& data, const RenderLimits& limits);

/*! \class ArchivedTemplate
    \brief Opaque, immutable handle over one validated archived template.

    The handle owns its archive backing through immutable shared state. Loading
    defensively copies the supplied bytes into suitably aligned storage and
    validates them once. Copies are inexpensive and remain valid independently
    of the input buffer used to load the archive.
*/
class ArchivedTemplate {
  public:
    MUSTACHE_API ArchivedTemplate() noexcept;
    MUSTACHE_API ArchivedTemplate(const ArchivedTemplate& other) noexcept;
    MUSTACHE_API ArchivedTemplate& operator=(const ArchivedTemplate& other) noexcept;
    MUSTACHE_API ArchivedTemplate(ArchivedTemplate&& other) noexcept;
    MUSTACHE_API ArchivedTemplate& operator=(ArchivedTemplate&& other) noexcept;
    MUSTACHE_API ~ArchivedTemplate();

    //! Returns true when this handle contains no archived template.
    MUSTACHE_API bool empty() const noexcept;

    //! Tests whether this handle contains an archived template.
    explicit MUSTACHE_API operator bool() const noexcept;

  private:
    struct State;

    explicit ArchivedTemplate(std::shared_ptr<const State> state) noexcept;

    std::shared_ptr<const State> state;

    friend MUSTACHE_API ArchivedTemplate loadArchivedTemplate(
        const std::vector<std::uint8_t>& bytes, const ArchivedTemplateLimits& limits);
    friend MUSTACHE_API ArchivedTemplate loadArchivedTemplate(
        std::string_view bytes, const ArchivedTemplateLimits& limits);
    friend MUSTACHE_API std::string render(
        const ArchivedTemplate& archived, const Data& data, const RenderLimits& limits);
    friend class Mustache;
};

//! Serializes a Node graph using the protected archived-template format.
MUSTACHE_API std::vector<std::uint8_t> serializeArchivedTemplate(const Node& root,
    const Node::Partials& partials = Node::Partials(), const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

//! Serializes an opaque compiled template and compiled partials using the protected archived-template format.
MUSTACHE_API std::vector<std::uint8_t> serializeArchivedTemplate(const CompiledTemplate& compiled,
    const PartialMap& partials = PartialMap(), const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

} // namespace mustache

#endif

#endif
