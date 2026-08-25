#ifndef MUSTACHE_BENCHMARK_CISTA_ARCHIVE_HPP
#define MUSTACHE_BENCHMARK_CISTA_ARCHIVE_HPP

#include "mustache.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mustache_benchmark {

struct CistaArchiveLimits {
    std::size_t maxInputBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxNestingDepth = 64;
    std::size_t maxNodes = 100000;
    std::size_t maxStringBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxDataPartsPerNode = 256;
    std::size_t maxDataParts = 100000;
};

std::vector<std::uint8_t> serializeCistaArchive(const mustache::Node& root,
    const mustache::Node::Partials& partials = mustache::Node::Partials(),
    const CistaArchiveLimits& archiveLimits = CistaArchiveLimits());

std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data,
    const CistaArchiveLimits& archiveLimits = CistaArchiveLimits(),
    const mustache::RenderLimits& renderLimits = mustache::RenderLimits());

} // namespace mustache_benchmark

#endif
