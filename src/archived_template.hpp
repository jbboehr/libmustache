#ifndef MUSTACHE_ARCHIVED_TEMPLATE_HPP
#define MUSTACHE_ARCHIVED_TEMPLATE_HPP

#include "mustache_config.h"
#include "mustache_export.hpp"

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)

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

//! Resource limits applied while creating or validating an archived template.
struct ArchivedTemplateLimits {
    std::size_t maxInputBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxNestingDepth = 64;
    std::size_t maxNodes = 100000;
    std::size_t maxStringBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxDataPartsPerNode = 256;
    std::size_t maxDataParts = 100000;
};

/*! \class ArchivedTemplateView
    \brief Opaque, immutable handle over one validated archived template.

    The handle owns its archive backing through immutable shared state. Loading
    defensively copies the supplied bytes into suitably aligned storage and
    validates them once. Copies are inexpensive and remain valid independently
    of the input buffer used to load the archive.
*/
class ArchivedTemplateView {
  public:
    MUSTACHE_API ArchivedTemplateView() noexcept;
    MUSTACHE_API ArchivedTemplateView(const ArchivedTemplateView& other) noexcept;
    MUSTACHE_API ArchivedTemplateView& operator=(const ArchivedTemplateView& other) noexcept;
    MUSTACHE_API ArchivedTemplateView(ArchivedTemplateView&& other) noexcept;
    MUSTACHE_API ArchivedTemplateView& operator=(ArchivedTemplateView&& other) noexcept;
    MUSTACHE_API ~ArchivedTemplateView();

    //! Returns true when this handle contains no archived template.
    MUSTACHE_API bool empty() const noexcept;

    //! Tests whether this handle contains an archived template.
    explicit MUSTACHE_API operator bool() const noexcept;

  private:
    struct State;

    explicit ArchivedTemplateView(std::shared_ptr<const State> state) noexcept;

    std::shared_ptr<const State> state;

    friend ArchivedTemplateView loadArchivedTemplate(
        const std::vector<std::uint8_t>& bytes, const ArchivedTemplateLimits& limits);
    friend ArchivedTemplateView loadArchivedTemplate(std::string_view bytes, const ArchivedTemplateLimits& limits);
    friend std::string render(const ArchivedTemplateView& archived, const Data& data, const RenderLimits& limits);
    friend class Mustache;
};

//! Serializes a Node graph using the protected archived-template format.
MUSTACHE_API std::vector<std::uint8_t> serializeArchivedTemplate(const Node& root,
    const Node::Partials& partials = Node::Partials(), const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

//! Copies archive bytes into owned storage and validates them once.
MUSTACHE_API ArchivedTemplateView loadArchivedTemplate(
    const std::vector<std::uint8_t>& bytes, const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

//! Copies archive bytes into owned storage and validates them once.
MUSTACHE_API ArchivedTemplateView loadArchivedTemplate(
    std::string_view bytes, const ArchivedTemplateLimits& limits = ArchivedTemplateLimits());

//! Renders a previously validated archived template.
MUSTACHE_API std::string render(const ArchivedTemplateView& archived, const Data& data);

//! Renders a previously validated archived template with resource limits.
MUSTACHE_API std::string render(const ArchivedTemplateView& archived, const Data& data, const RenderLimits& limits);

} // namespace mustache

#endif

#endif
