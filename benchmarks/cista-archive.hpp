#ifndef MUSTACHE_BENCHMARK_CISTA_ARCHIVE_HPP
#define MUSTACHE_BENCHMARK_CISTA_ARCHIVE_HPP

#include "mustache.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mustache_benchmark {

/*! Security checks layered on top of Cista's selected type version.

    Every mode retains the same version policy so the benchmark isolates the
    cost of deep structural checking and integrity verification.
*/
enum class CistaSecurityMode {
  Neither,
  DeepCheck,
  Integrity,
  DeepCheckAndIntegrity,
};

#if defined(_MSC_VER) && defined(_M_IX86)
namespace detail {

//! Returns the native ArchiveGraph offset of the explicit Win32 padding field.
std::size_t win32ArchiveGraphReservedMemberOffset() noexcept;

} // namespace detail
#endif

const char * cistaSecurityModeName(CistaSecurityMode mode) noexcept;

const char * cistaVersionModeName() noexcept;

const char * cistaIntegrityAlgorithmName() noexcept;

enum class CistaChecksumAlgorithm {
  None,
  Fnv1a64,
  Crc32,
  Xxh3_64,
};

const char * cistaChecksumAlgorithmName(CistaChecksumAlgorithm algorithm) noexcept;

std::uint64_t checksumCistaArchive(std::string_view bytes, CistaChecksumAlgorithm algorithm);

struct CistaArchiveLimits {
    std::size_t maxArchiveBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxNestingDepth = 64;
    std::size_t maxNodes = 100000;
    std::size_t maxTotalStringBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxDataPartsPerNode = 256;
    std::size_t maxTotalDataParts = 100000;
};

/*! Serialize an experimental archive with a libmustache format preamble.

    The fixed-width preamble is validated before Cista performs its own type,
    integrity, and structural checks. It counts toward maxArchiveBytes.
*/
std::vector<std::uint8_t> serializeCistaArchive(const mustache::Node& root,
    const mustache::Node::Partials& partials = mustache::Node::Partials(),
    const CistaArchiveLimits& archiveLimits = CistaArchiveLimits());

std::vector<std::uint8_t> serializeCistaArchive(const mustache::Node& root, const mustache::Node::Partials& partials,
    CistaSecurityMode mode, const CistaArchiveLimits& archiveLimits = CistaArchiveLimits());

/*! Validate a framed archive without rendering it. */
void validateCistaArchive(std::string_view bytes, const CistaArchiveLimits& archiveLimits = CistaArchiveLimits());

void validateCistaArchive(
    std::string_view bytes, CistaSecurityMode mode, const CistaArchiveLimits& archiveLimits = CistaArchiveLimits());

/*! Validate and render a framed archive without rebuilding owned Nodes. */
std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data,
    const CistaArchiveLimits& archiveLimits = CistaArchiveLimits(),
    const mustache::RenderLimits& renderLimits = mustache::RenderLimits());

std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data, CistaSecurityMode mode,
    const CistaArchiveLimits& archiveLimits = CistaArchiveLimits(),
    const mustache::RenderLimits& renderLimits = mustache::RenderLimits());

} // namespace mustache_benchmark

#endif
