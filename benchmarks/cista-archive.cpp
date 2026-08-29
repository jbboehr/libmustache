#include "cista-archive.hpp"

#include "render_engine.hpp"

#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3) && defined(CISTA_FNV1A)
#undef CISTA_FNV1A
#endif
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
#include "cista-xxh3/xxh3.h"
#else
#include <xxhash.h>
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
#include <cstdint>
#include <intrin.h>

namespace {

std::uint64_t mustacheCistaInterlockedOr64(std::int64_t * block, std::uint64_t mask) noexcept
{
  volatile __int64 * target = reinterpret_cast<volatile __int64 *>(block);
  __int64 observed = _InterlockedCompareExchange64(target, 0, 0);
  for (;;) {
    const __int64 desired = static_cast<__int64>(static_cast<std::uint64_t>(observed) | mask);
    const __int64 previous = _InterlockedCompareExchange64(target, desired, observed);
    if (previous == observed) {
      return static_cast<std::uint64_t>(observed);
    }
    observed = previous;
  }
}

std::uint64_t mustacheCistaInterlockedAnd64(std::int64_t * block, std::uint64_t mask) noexcept
{
  volatile __int64 * target = reinterpret_cast<volatile __int64 *>(block);
  __int64 observed = _InterlockedCompareExchange64(target, 0, 0);
  for (;;) {
    const __int64 desired = static_cast<__int64>(static_cast<std::uint64_t>(observed) & mask);
    const __int64 previous = _InterlockedCompareExchange64(target, desired, observed);
    if (previous == observed) {
      return static_cast<std::uint64_t>(observed);
    }
    observed = previous;
  }
}

} // namespace

// Cista 0.16 uses x64-only MSVC intrinsics in inline helpers even when those
// helpers are not instantiated by the archive schema. Supply equivalent CAS
// loops while parsing the dependency header so Win32 can compile it.
#define _InterlockedOr64 mustacheCistaInterlockedOr64
#define _InterlockedAnd64 mustacheCistaInterlockedAnd64
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#if defined(MUSTACHE_USE_VENDORED_CISTA)
#include <cista.h>
#else
#include <cista/serialization.h>
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
#undef _InterlockedAnd64
#undef _InterlockedOr64
#endif
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mustache_benchmark {

namespace {

namespace archive_data = cista::offset;

constexpr std::uint64_t archiveGraphMagic = UINT64_C(0x4D55535443495354);
constexpr std::uint32_t archiveSchemaVersion = 1;
constexpr std::size_t archivePreambleSize = 16;
constexpr std::array<std::uint8_t, 8> archivePreambleMagic = {'M', 'U', 'S', 'T', 'A', 'R', 'C', 0};
constexpr std::uint64_t archiveFormatGeneration = 1;
constexpr std::size_t archiveFormatGenerationOffset = archivePreambleMagic.size();
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
constexpr cista::mode archiveVersionMode = cista::mode::WITH_VERSION;
#else
constexpr cista::mode archiveVersionMode = cista::mode::WITH_STATIC_VERSION;
#endif
constexpr cista::mode archiveModeNeither = archiveVersionMode;
constexpr cista::mode archiveModeDeepCheck = archiveVersionMode | cista::mode::DEEP_CHECK;
constexpr cista::mode archiveModeIntegrity = archiveVersionMode | cista::mode::WITH_INTEGRITY;
constexpr cista::mode archiveModeDeepCheckAndIntegrity =
    archiveVersionMode | cista::mode::WITH_INTEGRITY | cista::mode::DEEP_CHECK;
constexpr std::size_t renderNestingCeiling = mustache::detail::renderNestingCeiling;
constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

template <typename Unsigned>
void writeLittleEndian(std::vector<std::uint8_t> * bytes, std::size_t offset, Unsigned value)
{
  static_assert(std::is_unsigned_v<Unsigned>, "archive preamble fields must be unsigned");
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    (*bytes)[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
  }
}

template <typename Unsigned> Unsigned readLittleEndian(std::string_view bytes, std::size_t offset)
{
  static_assert(std::is_unsigned_v<Unsigned>, "archive preamble fields must be unsigned");
  Unsigned value = 0;
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    value |= static_cast<Unsigned>(static_cast<std::uint8_t>(bytes[offset + index])) << (index * 8);
  }
  return value;
}

std::vector<std::uint8_t> frameArchive(std::vector<std::uint8_t>&& payload, const CistaArchiveLimits& limits)
{
  if (limits.maxInputBytes < archivePreambleSize || payload.size() > limits.maxInputBytes - archivePreambleSize) {
    throw mustache::Exception("Cista archive output byte limit exceeded");
  }

  std::vector<std::uint8_t> bytes;
  bytes.reserve(archivePreambleSize + payload.size());
  bytes.resize(archivePreambleSize, 0);
  std::copy(archivePreambleMagic.begin(), archivePreambleMagic.end(), bytes.begin());
  writeLittleEndian(&bytes, archiveFormatGenerationOffset, archiveFormatGeneration);
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

std::string_view readArchivePreamble(std::string_view bytes, const CistaArchiveLimits& limits)
{
  if (bytes.empty()) {
    throw mustache::Exception("Empty Cista archive");
  }
  if (bytes.size() > limits.maxInputBytes) {
    throw mustache::Exception("Cista archive input byte limit exceeded");
  }
  if (bytes.size() < archivePreambleSize) {
    throw mustache::Exception("Truncated Cista archive preamble");
  }
  if (std::memcmp(bytes.data(), archivePreambleMagic.data(), archivePreambleMagic.size()) != 0) {
    throw mustache::Exception("Invalid Cista archive preamble magic");
  }
  if (readLittleEndian<std::uint64_t>(bytes, archiveFormatGenerationOffset) != archiveFormatGeneration) {
    throw mustache::Exception("Unsupported libmustache archive format generation");
  }
  if (bytes.size() == archivePreambleSize) {
    throw mustache::Exception("Empty Cista archive payload");
  }
  return bytes.substr(archivePreambleSize);
}

enum Presence : std::uint8_t {
  HasData = 1,
  HasStartSequence = 2,
  HasStopSequence = 4,
};

struct ArchiveSlice {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct ArchiveNode {
    ArchiveSlice data;
    ArchiveSlice startSequence;
    ArchiveSlice stopSequence;
    std::uint32_t firstChild = invalidIndex;
    std::uint32_t nextSibling = invalidIndex;
    std::uint16_t type = 0;
    std::uint16_t flags = 0;
    std::uint8_t presence = 0;
    std::uint8_t reserved0 = 0;
    std::uint8_t reserved1 = 0;
    std::uint8_t reserved2 = 0;
};

struct ArchivePartial {
    ArchiveSlice name;
    std::uint32_t root = 0;
};

struct ArchiveGraph {
    std::uint64_t magic = archiveGraphMagic;
    std::uint32_t version = archiveSchemaVersion;
    std::uint32_t root = 0;
    std::uint64_t serializedSize = 0;
    archive_data::vector<ArchiveNode> nodes;
    archive_data::vector<ArchivePartial> partials;
    archive_data::vector<std::uint8_t> strings;
#if defined(_MSC_VER) && defined(_M_IX86)
    // MSVC gives this graph eight-byte alignment on x86. Make its four tail
    // padding bytes explicit so Cista cannot serialize indeterminate data.
    std::uint32_t reserved = 0;
#endif
};

#if defined(_MSC_VER) && defined(_M_IX86)
static_assert(cista_member_offset(ArchiveGraph, reserved) + sizeof(ArchiveGraph::reserved) == sizeof(ArchiveGraph),
    "Win32 archive graph must not contain tail padding");
#endif

static_assert(
    archivePreambleSize % alignof(ArchiveGraph) == 0, "archive preamble must preserve the Cista payload alignment");

template <typename Field> Field readNativeArchiveField(std::string_view payload, std::size_t offset)
{
  static_assert(std::is_trivially_copyable_v<Field>, "serialized Cista fields must be trivially copyable");
  if (offset > payload.size() || sizeof(Field) > payload.size() - offset) {
    throw mustache::Exception("Truncated Cista archive graph header");
  }
  Field value{};
  std::memcpy(&value, payload.data() + offset, sizeof(value));
  return value;
}

template <typename Element>
void validateSerializedVector(std::string_view payload, std::size_t graphOffset, std::size_t vectorOffset)
{
  using Vector = archive_data::vector<Element>;
  using Pointer = typename Vector::pointer;
  using Size = typename Vector::size_type;
  static_assert(std::is_standard_layout_v<Vector>, "Cista vector layout must be inspectable before deserialization");
  static_assert(std::is_standard_layout_v<Pointer>, "Cista offset pointer layout must be inspectable");
  static_assert(std::is_signed_v<cista::offset_t>, "Cista offsets must be signed");
  static_assert(
      std::is_unsigned_v<Size> && sizeof(Size) <= sizeof(std::size_t), "Cista vector sizes must fit in size_t");

  const std::size_t serializedVectorOffset = graphOffset + vectorOffset;
  const std::size_t pointerOffset =
      serializedVectorOffset + cista_member_offset(Vector, el_) + cista_member_offset(Pointer, offset_);
  const cista::offset_t relativeOffset = readNativeArchiveField<cista::offset_t>(payload, pointerOffset);
  const Size usedSizeField =
      readNativeArchiveField<Size>(payload, serializedVectorOffset + cista_member_offset(Vector, used_size_));
  const Size allocatedSizeField =
      readNativeArchiveField<Size>(payload, serializedVectorOffset + cista_member_offset(Vector, allocated_size_));
  const std::uint8_t selfAllocated = readNativeArchiveField<std::uint8_t>(
      payload, serializedVectorOffset + cista_member_offset(Vector, self_allocated_));

  if (selfAllocated != 0 || allocatedSizeField != usedSizeField) {
    throw mustache::Exception("Invalid Cista archive vector header");
  }
  if (relativeOffset == cista::NULLPTR_OFFSET) {
    if (usedSizeField != 0) {
      throw mustache::Exception("Invalid Cista archive null vector");
    }
    return;
  }
  if (usedSizeField == 0) {
    throw mustache::Exception("Invalid Cista archive empty vector pointer");
  }

  std::size_t dataOffset = 0;
  if (relativeOffset < 0) {
    const std::uintmax_t magnitude =
        static_cast<std::uintmax_t>(-(relativeOffset + 1)) + static_cast<std::uintmax_t>(1);
    if (magnitude > pointerOffset) {
      throw mustache::Exception("Invalid Cista archive vector pointer");
    }
    dataOffset = pointerOffset - static_cast<std::size_t>(magnitude);
  } else {
    const std::uintmax_t distance = static_cast<std::uintmax_t>(relativeOffset);
    if (distance > payload.size() - pointerOffset) {
      throw mustache::Exception("Invalid Cista archive vector pointer");
    }
    dataOffset = pointerOffset + static_cast<std::size_t>(distance);
  }

  const std::size_t usedSize = static_cast<std::size_t>(usedSizeField);
  if (usedSize > std::numeric_limits<std::size_t>::max() / sizeof(Element)) {
    throw mustache::Exception("Invalid Cista archive vector size");
  }
  const std::size_t byteSize = usedSize * sizeof(Element);
  if (dataOffset > payload.size() || byteSize > payload.size() - dataOffset) {
    throw mustache::Exception("Invalid Cista archive vector range");
  }
  if (reinterpret_cast<std::uintptr_t>(payload.data() + dataOffset) % alignof(Element) != 0) {
    throw mustache::Exception("Invalid Cista archive vector alignment");
  }
}

template <cista::mode Mode> void validateArchiveGraphLayout(std::string_view payload)
{
  constexpr cista::offset_t graphOffsetField = cista::data_start(Mode);
  static_assert(graphOffsetField >= 0, "Cista graph offset must be non-negative");
  constexpr std::size_t graphOffset = static_cast<std::size_t>(graphOffsetField);
  if (graphOffset > payload.size() || sizeof(ArchiveGraph) > payload.size() - graphOffset) {
    throw mustache::Exception("Truncated Cista archive graph header");
  }
  validateSerializedVector<ArchiveNode>(payload, graphOffset, cista_member_offset(ArchiveGraph, nodes));
  validateSerializedVector<ArchivePartial>(payload, graphOffset, cista_member_offset(ArchiveGraph, partials));
  validateSerializedVector<std::uint8_t>(payload, graphOffset, cista_member_offset(ArchiveGraph, strings));
}

std::string_view archiveSliceView(const ArchiveGraph& graph, const ArchiveSlice& value)
{
  if (value.length == 0) {
    return std::string_view();
  }
  return std::string_view(reinterpret_cast<const char *>(graph.strings.data() + value.offset), value.length);
}

using NodeTypeValue = std::underlying_type_t<mustache::Node::Type>;

NodeTypeValue nodeTypeValue(const mustache::Node& node) noexcept
{
  NodeTypeValue value;
  static_assert(sizeof(value) == sizeof(node.type), "Node type representation changed");
  std::memcpy(&value, &node.type, sizeof(value));
  return value;
}

bool isSerializableType(NodeTypeValue type)
{
  switch (type) {
    case static_cast<NodeTypeValue>(mustache::Node::TypeRoot):
    case static_cast<NodeTypeValue>(mustache::Node::TypeOutput):
    case static_cast<NodeTypeValue>(mustache::Node::TypeTag):
    case static_cast<NodeTypeValue>(mustache::Node::TypeVariable):
    case static_cast<NodeTypeValue>(mustache::Node::TypeNegate):
    case static_cast<NodeTypeValue>(mustache::Node::TypeSection):
    case static_cast<NodeTypeValue>(mustache::Node::TypeStop):
    case static_cast<NodeTypeValue>(mustache::Node::TypeComment):
    case static_cast<NodeTypeValue>(mustache::Node::TypePartial):
    case static_cast<NodeTypeValue>(mustache::Node::TypeInlinePartial):
      return true;
    default:
      return false;
  }
}

bool typeAllowsChildren(mustache::Node::Type type)
{
  return (type & mustache::Node::TypeHasChildren) != 0;
}

bool typeUsesDataParts(mustache::Node::Type type)
{
  return (type & mustache::Node::TypeHasDot) != 0;
}

void addBounded(std::size_t amount, std::size_t maximum, std::size_t * total, const char * message)
{
  if (*total > maximum || amount > maximum - *total) {
    throw mustache::Exception(message);
  }
  *total += amount;
}

class ArchiveBuilder {
  public:
    ArchiveBuilder(const CistaArchiveLimits& limits, std::size_t cistaHeaderBytes) :
        limits_(limits),
        cistaHeaderBytes_(cistaHeaderBytes)
    {}

    ArchiveGraph build(const mustache::Node& root, const mustache::Node::Partials& partials)
    {
      validateMinimumSize();
      const EffectivePartials effectivePartials = collectEffectivePartials(root.partials, partials);
      countNode(root, 0);
      for (const EffectivePartial& partial : effectivePartials) {
        countNode(*partial.second, 0);
      }
      graph_.nodes.reserve(static_cast<std::uint32_t>(nodeCount_));
      graph_.partials.reserve(static_cast<std::uint32_t>(effectivePartials.size()));
      graph_.strings.reserve(static_cast<std::uint32_t>(stringBytes_));

      graph_.root = appendNode(root);
      for (const EffectivePartial& partial : effectivePartials) {
        ArchivePartial archived{};
        archived.name = appendString(partial.first);
        archived.root = appendNode(*partial.second);
        graph_.partials.push_back(std::move(archived));
      }
      return std::move(graph_);
    }

  private:
    using EffectivePartials = std::map<std::string_view, const mustache::Node *>;
    using EffectivePartial = EffectivePartials::value_type;

    EffectivePartials collectEffectivePartials(
        const mustache::Node::Partials& fallback, const mustache::Node::Partials& overriding)
    {
      EffectivePartials effective;
      for (const mustache::Node::PartialPair& partial : fallback) {
        if (partial.second != nullptr) {
          const auto inserted = effective.emplace(partial.first, partial.second.get());
          if (inserted.second) {
            addPartialName(partial.first);
          }
        }
      }
      // A null external entry does not mask the root-owned fallback in
      // OwnedPartialSource, so only non-null overrides enter the archive.
      for (const mustache::Node::PartialPair& partial : overriding) {
        if (partial.second != nullptr) {
          const auto position = effective.find(partial.first);
          if (position == effective.end()) {
            effective.emplace(partial.first, partial.second.get());
            addPartialName(partial.first);
          } else {
            position->second = partial.second.get();
          }
        }
      }
      return effective;
    }

    void addPartialName(std::string_view name)
    {
      // Every archived partial owns at least one root node, so this conservative
      // writer bound guarantees that its output fits the paired reader's shared
      // aggregate node budget before the partial table is allocated.
      if (limits_.maxNodes == 0 || partialCount_ >= limits_.maxNodes - 1) {
        throw mustache::Exception("Cista archive node count limit exceeded");
      }
      if (partialCount_ == std::numeric_limits<std::uint32_t>::max()) {
        throw mustache::Exception("Cista archive partial count exceeds format limit");
      }
      ++partialCount_;
      addStringBytes(name.size());
      validateMinimumSize();
    }

    void addStringBytes(std::size_t bytes)
    {
      if (stringBytes_ > std::numeric_limits<std::uint32_t>::max() ||
          bytes > std::numeric_limits<std::uint32_t>::max() - stringBytes_) {
        throw mustache::Exception("Cista archive string bytes exceed format limit");
      }
      if (stringBytes_ > limits_.maxStringBytes || bytes > limits_.maxStringBytes - stringBytes_) {
        throw mustache::Exception("Cista archive string byte limit exceeded");
      }
      stringBytes_ += bytes;
      validateMinimumSize();
    }

    void countNode(const mustache::Node& source, std::size_t depth)
    {
      if (depth >= limits_.maxNestingDepth || depth >= renderNestingCeiling) {
        throw mustache::Exception("Cista archive nesting limit exceeded");
      }
      if (nodeCount_ >= limits_.maxNodes || nodeCount_ == std::numeric_limits<std::uint32_t>::max()) {
        throw mustache::Exception("Cista archive node count limit exceeded");
      }
      ++nodeCount_;
      validateMinimumSize();
      if (source.data.has_value()) {
        addStringBytes(source.data->size());
      }
      if (source.startSequence.has_value()) {
        addStringBytes(source.startSequence->size());
      }
      if (source.stopSequence.has_value()) {
        addStringBytes(source.stopSequence->size());
      }
      const NodeTypeValue type = nodeTypeValue(source);
      if (isSerializableType(type) && typeUsesDataParts(static_cast<mustache::Node::Type>(type)) &&
          source.data.has_value()) {
        const std::size_t parts =
            static_cast<std::size_t>(std::count(source.data->begin(), source.data->end(), '.')) + 1;
        if (parts > limits_.maxDataPartsPerNode) {
          throw mustache::Exception("Cista archive per-node data-part limit exceeded");
        }
        addBounded(parts, limits_.maxDataParts, &dataParts_, "Cista archive data-part limit exceeded");
      }
      for (const std::unique_ptr<mustache::Node>& child : source.children) {
        if (child == nullptr) {
          throw mustache::Exception("Invalid null Cista archive child");
        }
        countNode(*child, depth + 1);
      }
    }

    void validateMinimumSize() const
    {
      std::size_t minimum = archivePreambleSize;
      const auto addMinimum = [&minimum, this](std::size_t amount) {
        if (minimum > limits_.maxInputBytes || amount > limits_.maxInputBytes - minimum) {
          throw mustache::Exception("Cista archive output byte limit exceeded");
        }
        minimum += amount;
      };
      addMinimum(cistaHeaderBytes_);
      addMinimum(sizeof(ArchiveGraph));
      if (nodeCount_ > limits_.maxInputBytes / sizeof(ArchiveNode) ||
          partialCount_ > limits_.maxInputBytes / sizeof(ArchivePartial)) {
        throw mustache::Exception("Cista archive output byte limit exceeded");
      }
      addMinimum(nodeCount_ * sizeof(ArchiveNode));
      addMinimum(partialCount_ * sizeof(ArchivePartial));
      addMinimum(stringBytes_);
    }

    ArchiveSlice appendString(std::string_view value)
    {
      ArchiveSlice result;
      result.offset = static_cast<std::uint32_t>(graph_.strings.size());
      result.length = static_cast<std::uint32_t>(value.size());
      if (!value.empty()) {
        graph_.strings.insert(graph_.strings.end(), reinterpret_cast<const std::uint8_t *>(value.data()),
            reinterpret_cast<const std::uint8_t *>(value.data()) + value.size());
      }
      return result;
    }

    std::uint32_t appendNode(const mustache::Node& source)
    {
      if (graph_.nodes.size() == std::numeric_limits<std::uint32_t>::max()) {
        throw mustache::Exception("Cista archive node count exceeds format limit");
      }
      if (source.child != nullptr) {
        throw mustache::Exception("Cista archive experiment does not support container nodes");
      }
      if (source.flags < 0 || static_cast<unsigned int>(source.flags) > std::numeric_limits<std::uint16_t>::max()) {
        throw mustache::Exception("Cista archive node flags exceed format limit");
      }
      const NodeTypeValue type = nodeTypeValue(source);
      if (!isSerializableType(type)) {
        throw mustache::Exception("Invalid Cista archive node type");
      }

      const std::uint32_t index = static_cast<std::uint32_t>(graph_.nodes.size());
      graph_.nodes.emplace_back();

      ArchiveNode archived{};
      archived.type = static_cast<std::uint16_t>(type);
      archived.flags = static_cast<std::uint16_t>(source.flags);
      if (source.data.has_value()) {
        archived.presence |= HasData;
        archived.data = appendString(*source.data);
      }
      if (source.startSequence.has_value()) {
        archived.presence |= HasStartSequence;
        archived.startSequence = appendString(*source.startSequence);
      }
      if (source.stopSequence.has_value()) {
        archived.presence |= HasStopSequence;
        archived.stopSequence = appendString(*source.stopSequence);
      }
      std::uint32_t previousChild = invalidIndex;
      for (const std::unique_ptr<mustache::Node>& child : source.children) {
        const std::uint32_t childIndex = appendNode(*child);
        if (archived.firstChild == invalidIndex) {
          archived.firstChild = childIndex;
        } else {
          graph_.nodes[previousChild].nextSibling = childIndex;
        }
        previousChild = childIndex;
      }
      graph_.nodes[index] = std::move(archived);
      return index;
    }

    ArchiveGraph graph_{};
    const CistaArchiveLimits& limits_;
    std::size_t cistaHeaderBytes_;
    std::size_t nodeCount_ = 0;
    std::size_t partialCount_ = 0;
    std::size_t stringBytes_ = 0;
    std::size_t dataParts_ = 0;
};

class ArchiveValidator {
  public:
    ArchiveValidator(const ArchiveGraph& graph, const CistaArchiveLimits& limits, std::size_t inputBytes = 0) :
        graph_(graph),
        limits_(limits),
        inputBytes_(inputBytes),
        nodes_(0),
        dataParts_(0)
    {}

    void validate()
    {
      if (graph_.magic != archiveGraphMagic || graph_.version != archiveSchemaVersion || graph_.nodes.empty()) {
        throw mustache::Exception("Invalid Cista archive header");
      }
#if defined(_MSC_VER) && defined(_M_IX86)
      if (graph_.reserved != 0) {
        throw mustache::Exception("Invalid Cista archive reserved bytes");
      }
#endif
      if (graph_.nodes.size() > limits_.maxNodes) {
        throw mustache::Exception("Cista archive node count limit exceeded");
      }
      states_.assign(graph_.nodes.size(), 0);
      if (inputBytes_ != 0 && graph_.serializedSize != inputBytes_) {
        throw mustache::Exception("Invalid Cista archive encoded length");
      }
      if (graph_.strings.size() > limits_.maxStringBytes) {
        throw mustache::Exception("Cista archive string byte limit exceeded");
      }
      validateRoot(graph_.root);
      std::string_view previousName;
      bool hasPreviousName = false;
      for (const ArchivePartial& partial : graph_.partials) {
        const std::string_view name = validateString(partial.name);
        if (name.empty()) {
          throw mustache::Exception("Invalid empty Cista archive partial name");
        }
        if (hasPreviousName && !(previousName < name)) {
          throw mustache::Exception("Cista archive partial names are not canonical");
        }
        previousName = name;
        hasPreviousName = true;
        validatePartial(partial.root);
      }
      if (nodes_ != graph_.nodes.size()) {
        throw mustache::Exception("Unreachable Cista archive node");
      }
    }

  private:
    std::string_view validateString(const ArchiveSlice& value) const
    {
      if (value.offset > graph_.strings.size() || value.length > graph_.strings.size() - value.offset) {
        throw mustache::Exception("Invalid Cista archive string range");
      }
      return archiveSliceView(graph_, value);
    }

    void validateRoot(std::uint32_t index)
    {
      if (index >= graph_.nodes.size() || graph_.nodes[index].type != mustache::Node::TypeRoot) {
        throw mustache::Exception("Invalid Cista archive root");
      }
      if (graph_.nodes[index].nextSibling != invalidIndex) {
        throw mustache::Exception("Invalid Cista archive root sibling");
      }
      validateNode(index, 0, false);
    }

    void validatePartial(std::uint32_t index)
    {
      if (index >= graph_.nodes.size()) {
        throw mustache::Exception("Invalid Cista archive partial root");
      }
      if (graph_.nodes[index].nextSibling != invalidIndex) {
        throw mustache::Exception("Invalid Cista archive partial root sibling");
      }
      validateNode(index, 0, false);
    }

    void validateNode(std::uint32_t index, std::size_t depth, bool partialIndentationMetadata)
    {
      if (index >= graph_.nodes.size()) {
        throw mustache::Exception("Invalid Cista archive node index");
      }
      if (depth >= limits_.maxNestingDepth || depth >= renderNestingCeiling) {
        throw mustache::Exception("Cista archive nesting limit exceeded");
      }
      if (nodes_ >= limits_.maxNodes) {
        throw mustache::Exception("Cista archive node count limit exceeded");
      }
      if (states_[index] != 0) {
        throw mustache::Exception("Cista archive nodes must form disjoint trees");
      }
      states_[index] = 1;
      ++nodes_;

      const ArchiveNode& node = graph_.nodes[index];
      if (!isSerializableType(static_cast<NodeTypeValue>(node.type))) {
        throw mustache::Exception("Invalid Cista archive node type");
      }
      const mustache::Node::Type type = static_cast<mustache::Node::Type>(node.type);
      const std::uint16_t validFlags = static_cast<std::uint16_t>(
          mustache::Node::FlagEscape | mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent);
      if ((node.flags & ~validFlags) != 0 ||
          ((node.flags & mustache::Node::FlagLambdaOnly) != 0 &&
              (type != mustache::Node::TypeOutput ||
                  (node.flags != mustache::Node::FlagLambdaOnly &&
                      node.flags != (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent)))) ||
          ((node.flags & mustache::Node::FlagPartialIndent) != 0 &&
              (type != mustache::Node::TypeOutput ||
                  node.flags != (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent)))) {
        throw mustache::Exception("Invalid Cista archive node flags");
      }
      if ((node.flags & mustache::Node::FlagPartialIndent) != 0 && !partialIndentationMetadata) {
        throw mustache::Exception("Invalid Cista archive partial indentation metadata");
      }
      if ((node.presence & ~(HasData | HasStartSequence | HasStopSequence)) != 0) {
        throw mustache::Exception("Invalid Cista archive node presence bits");
      }
      if (node.reserved0 != 0 || node.reserved1 != 0 || node.reserved2 != 0) {
        throw mustache::Exception("Invalid Cista archive reserved bytes");
      }

      const bool hasData = (node.presence & HasData) != 0;
      if ((type == mustache::Node::TypeRoot && hasData) || (type != mustache::Node::TypeRoot && !hasData)) {
        throw mustache::Exception("Invalid Cista archive node data");
      }
      const std::string_view data = validateString(node.data);
      if (!hasData && (node.data.offset != 0 || node.data.length != 0)) {
        throw mustache::Exception("Invalid absent Cista archive node data");
      }
      if (node.firstChild != invalidIndex && !typeAllowsChildren(type)) {
        throw mustache::Exception("Invalid Cista archive node children");
      }

      const bool hasStart = (node.presence & HasStartSequence) != 0;
      const bool hasStop = (node.presence & HasStopSequence) != 0;
      validateString(node.startSequence);
      validateString(node.stopSequence);
      if (hasStart != hasStop || ((hasStart || hasStop) && type != mustache::Node::TypeSection) ||
          (!hasStart &&
              (node.startSequence.offset != 0 || node.startSequence.length != 0 || node.stopSequence.offset != 0 ||
                  node.stopSequence.length != 0))) {
        throw mustache::Exception("Invalid Cista archive section delimiters");
      }

      validateDataParts(node, type, data);
      validateChildren(node, depth);
      states_[index] = 2;
    }

    void validateDataParts(const ArchiveNode&, mustache::Node::Type type, std::string_view data)
    {
      if (!typeUsesDataParts(type)) {
        return;
      }
      const std::size_t dots = static_cast<std::size_t>(std::count(data.begin(), data.end(), '.'));
      const std::size_t parts = dots + 1;
      if (parts > limits_.maxDataPartsPerNode) {
        throw mustache::Exception("Cista archive per-node data-part limit exceeded");
      }
      addBounded(parts, limits_.maxDataParts, &dataParts_, "Cista archive data-part limit exceeded");
    }

    void validateChildren(const ArchiveNode& node, std::size_t depth)
    {
      std::uint32_t childIndex = node.firstChild;
      while (childIndex != invalidIndex) {
        if (childIndex >= graph_.nodes.size()) {
          throw mustache::Exception("Invalid Cista archive child index");
        }
        const ArchiveNode& child = graph_.nodes[childIndex];
        const std::uint32_t nextSibling = child.nextSibling;
        if (nextSibling != invalidIndex && nextSibling >= graph_.nodes.size()) {
          throw mustache::Exception("Invalid Cista archive sibling index");
        }
        bool indentationMetadata = false;
        if ((child.flags & mustache::Node::FlagPartialIndent) != 0) {
          if (child.type != mustache::Node::TypeOutput ||
              child.flags != (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent) ||
              (child.presence & HasData) == 0 || nextSibling == invalidIndex) {
            throw mustache::Exception("Invalid Cista archive partial indentation metadata");
          }
          const std::string_view indentation = validateString(child.data);
          if (!std::all_of(indentation.begin(), indentation.end(),
                  [](char value) {
                    return value == ' ' || value == '\t';
                  }) ||
              graph_.nodes[nextSibling].type != mustache::Node::TypePartial) {
            throw mustache::Exception("Invalid Cista archive partial indentation metadata");
          }
          indentationMetadata = true;
        }
        validateNode(childIndex, depth + 1, indentationMetadata);
        childIndex = nextSibling;
      }
    }

    const ArchiveGraph& graph_;
    const CistaArchiveLimits& limits_;
    std::size_t inputBytes_;
    std::vector<std::uint8_t> states_;
    std::size_t nodes_;
    std::size_t dataParts_;
};

class CistaSizeCounter {
  public:
    explicit CistaSizeCounter(std::size_t maximum) noexcept :
        maximum_(maximum)
    {}

    cista::offset_t write(const void *, std::size_t bytes, std::size_t alignment = 0)
    {
      const std::size_t start = alignedSize(alignment);
      if (start > maximum_ || bytes > maximum_ - start ||
          start > static_cast<std::uintmax_t>(std::numeric_limits<cista::offset_t>::max())) {
        throw mustache::Exception("Cista archive output byte limit exceeded");
      }
      size_ = start + bytes;
      return static_cast<cista::offset_t>(start);
    }

    template <typename Value> void write(std::size_t position, const Value&)
    {
      if (position > size_ || cista::serialized_size<Value>() > size_ - position) {
        throw mustache::Exception("Invalid Cista archive serializer write");
      }
    }

    std::uint64_t checksum(cista::offset_t = 0) const noexcept
    {
      return 0;
    }

    std::size_t size() const noexcept
    {
      return size_;
    }

  private:
    std::size_t alignedSize(std::size_t alignment) const
    {
      if (alignment <= 1) {
        return size_;
      }
      const std::size_t remainder = size_ % alignment;
      if (remainder == 0) {
        return size_;
      }
      const std::size_t padding = alignment - remainder;
      if (size_ > maximum_ || padding > maximum_ - size_) {
        throw mustache::Exception("Cista archive output byte limit exceeded");
      }
      return size_ + padding;
    }

    std::size_t maximum_;
    std::size_t size_ = 0;
};

class CistaBoundedBuffer {
  public:
    CistaBoundedBuffer(std::size_t maximum, std::size_t expected) :
        maximum_(maximum),
        expected_(expected)
    {
      if (expected > maximum) {
        throw mustache::Exception("Cista archive output byte limit exceeded");
      }
      bytes_.reserve(expected);
    }

    cista::offset_t write(const void * source, std::size_t bytes, std::size_t alignment = 0)
    {
      const std::size_t start = alignedSize(alignment);
      if (start > maximum_ || bytes > maximum_ - start ||
          start > static_cast<std::uintmax_t>(std::numeric_limits<cista::offset_t>::max())) {
        throw mustache::Exception("Cista archive output byte limit exceeded");
      }
      bytes_.resize(start + bytes);
      if (bytes != 0) {
        std::memcpy(bytes_.data() + start, source, bytes);
      }
      return static_cast<cista::offset_t>(start);
    }

    template <typename Value> void write(std::size_t position, const Value& value)
    {
      const std::size_t bytes = cista::serialized_size<Value>();
      if (position > bytes_.size() || bytes > bytes_.size() - position) {
        throw mustache::Exception("Invalid Cista archive serializer write");
      }
      std::memcpy(bytes_.data() + position, &value, bytes);
    }

    std::uint64_t checksum(cista::offset_t start = 0) const noexcept
    {
      const std::size_t offset = static_cast<std::size_t>(start);
      return cista::hash(
          std::string_view(reinterpret_cast<const char *>(bytes_.data() + offset), bytes_.size() - offset));
    }

    std::vector<std::uint8_t> release()
    {
      if (bytes_.size() != expected_) {
        throw mustache::Exception("Cista archive size changed while framing");
      }
      return std::move(bytes_);
    }

  private:
    std::size_t alignedSize(std::size_t alignment) const
    {
      if (alignment <= 1) {
        return bytes_.size();
      }
      const std::size_t remainder = bytes_.size() % alignment;
      if (remainder == 0) {
        return bytes_.size();
      }
      const std::size_t padding = alignment - remainder;
      if (bytes_.size() > maximum_ || padding > maximum_ - bytes_.size()) {
        throw mustache::Exception("Cista archive output byte limit exceeded");
      }
      return bytes_.size() + padding;
    }

    std::size_t maximum_;
    std::size_t expected_;
    std::vector<std::uint8_t> bytes_;
};

class ArchiveChildCursor;
class ArchiveDataPartCursor;

/*! Zero-copy view of one validated archive node for the shared renderer. */
class ArchiveNodeView {
  public:
    ArchiveNodeView() noexcept :
        graph_(nullptr),
        index_(invalidIndex)
    {}

    static ArchiveNodeView fromNode(const ArchiveGraph * graph, std::uint32_t index) noexcept
    {
      return ArchiveNodeView(graph, index);
    }

    explicit operator bool() const noexcept
    {
      return graph_ != nullptr && index_ != invalidIndex;
    }

    mustache::Node::Type type() const noexcept
    {
      return static_cast<mustache::Node::Type>(node().type);
    }

    int flags() const noexcept
    {
      return static_cast<int>(node().flags);
    }

    mustache::detail::RenderString data() const noexcept
    {
      return string(node().data, HasData);
    }

    ArchiveDataPartCursor dataParts() const noexcept;

    ArchiveChildCursor children() const noexcept;

    ArchiveNodeView containerChild() const noexcept
    {
      return ArchiveNodeView();
    }

    mustache::detail::RenderString startSequence() const noexcept
    {
      return string(node().startSequence, HasStartSequence);
    }

    mustache::detail::RenderString stopSequence() const noexcept
    {
      return string(node().stopSequence, HasStopSequence);
    }

  private:
    friend class ArchiveChildCursor;
    friend class ArchiveDataPartCursor;

    ArchiveNodeView(const ArchiveGraph * graph, std::uint32_t index) noexcept :
        graph_(graph),
        index_(index)
    {}

    const ArchiveNode& node() const noexcept
    {
      assert(static_cast<bool>(*this));
      assert(index_ < graph_->nodes.size());
      return graph_->nodes[index_];
    }

    mustache::detail::RenderString string(const ArchiveSlice& slice, std::uint8_t presence) const noexcept
    {
      return (node().presence & presence) == 0
          ? mustache::detail::RenderString()
          : mustache::detail::RenderString::fromView(archiveSliceView(*graph_, slice));
    }

    const ArchiveGraph * graph_;
    std::uint32_t index_;
};

/*! Single-pass cursor over dotted components borrowed from an archive string. */
class ArchiveDataPartCursor {
  public:
    ArchiveDataPartCursor() noexcept :
        value_(),
        start_(0),
        stop_(std::string_view::npos),
        valid_(false)
    {}

    explicit operator bool() const noexcept
    {
      return valid_;
    }

    mustache::detail::RenderString value() const noexcept
    {
      assert(valid_);
      const std::size_t length = stop_ == std::string_view::npos ? value_.size() - start_ : stop_ - start_;
      return mustache::detail::RenderString::fromView(value_.substr(start_, length));
    }

    void advance() noexcept
    {
      assert(valid_);
      if (stop_ == std::string_view::npos) {
        valid_ = false;
        return;
      }
      start_ = stop_ + 1;
      stop_ = value_.find('.', start_);
    }

  private:
    friend class ArchiveNodeView;

    explicit ArchiveDataPartCursor(std::string_view value) noexcept :
        value_(value),
        start_(0),
        stop_(value.find('.')),
        valid_(stop_ != std::string_view::npos)
    {}

    std::string_view value_;
    std::size_t start_;
    std::size_t stop_;
    bool valid_;
};

/*! Linear cursor over an archive node's sibling-linked child list. */
class ArchiveChildCursor {
  public:
    ArchiveChildCursor() noexcept :
        graph_(nullptr),
        index_(invalidIndex)
    {}

    explicit operator bool() const noexcept
    {
      return graph_ != nullptr && index_ != invalidIndex;
    }

    ArchiveNodeView value() const noexcept
    {
      assert(static_cast<bool>(*this));
      return ArchiveNodeView::fromNode(graph_, index_);
    }

    void advance() noexcept
    {
      assert(static_cast<bool>(*this));
      index_ = graph_->nodes[index_].nextSibling;
    }

  private:
    friend class ArchiveNodeView;

    ArchiveChildCursor(const ArchiveGraph * graph, std::uint32_t index) noexcept :
        graph_(graph),
        index_(index)
    {}

    const ArchiveGraph * graph_;
    std::uint32_t index_;
};

ArchiveChildCursor ArchiveNodeView::children() const noexcept
{
  return ArchiveChildCursor(graph_, node().firstChild);
}

ArchiveDataPartCursor ArchiveNodeView::dataParts() const noexcept
{
  if ((type() & mustache::Node::TypeHasDot) == 0) {
    return ArchiveDataPartCursor();
  }
  return ArchiveDataPartCursor(archiveSliceView(*graph_, node().data));
}

/*! Partial lookup policy over the archive's sorted partial index. */
class ArchivePartialSource {
  public:
    explicit ArchivePartialSource(const ArchiveGraph& graph) noexcept :
        graph_(graph)
    {}

    template <typename Callback> bool withPartial(mustache::detail::RenderString name, Callback&& callback) const
    {
      assert(name);
      const std::string_view expected = name.value();
      const auto position = std::lower_bound(graph_.partials.begin(), graph_.partials.end(), expected,
          [this](const ArchivePartial& partial, std::string_view value) {
            return archiveSliceView(graph_, partial.name) < value;
          });
      if (position == graph_.partials.end() || archiveSliceView(graph_, position->name) != expected) {
        return false;
      }
      std::forward<Callback>(callback)(ArchiveNodeView::fromNode(&graph_, position->root));
      return true;
    }

  private:
    const ArchiveGraph& graph_;
};

template <cista::mode Mode> const ArchiveGraph& readArchive(std::string_view bytes, const CistaArchiveLimits& limits)
{
  const std::string_view payload = readArchivePreamble(bytes, limits);
  if (reinterpret_cast<std::uintptr_t>(payload.data()) % alignof(ArchiveGraph) != 0) {
    throw mustache::Exception("Unaligned Cista archive buffer");
  }
  validateArchiveGraphLayout<Mode>(payload);
  const ArchiveGraph * graph = nullptr;
  try {
    graph = cista::deserialize<ArchiveGraph, Mode>(payload);
  } catch (const cista::cista_exception& exception) {
    throw mustache::Exception(exception.what());
  }
  if (graph == nullptr) {
    throw mustache::Exception("Invalid Cista archive root");
  }
  ArchiveValidator(*graph, limits, payload.size()).validate();
  return *graph;
}

template <cista::mode Mode>
std::vector<std::uint8_t> serializeCistaArchiveWithMode(
    const mustache::Node& root, const mustache::Node::Partials& partials, const CistaArchiveLimits& archiveLimits)
{
  if (archiveLimits.maxInputBytes < archivePreambleSize) {
    throw mustache::Exception("Cista archive output byte limit exceeded");
  }
  const std::size_t maximumPayloadBytes = archiveLimits.maxInputBytes - archivePreambleSize;
  ArchiveGraph graph =
      ArchiveBuilder(archiveLimits, static_cast<std::size_t>(cista::data_start(Mode))).build(root, partials);
  ArchiveValidator(graph, archiveLimits).validate();
  CistaSizeCounter counter(maximumPayloadBytes);
  cista::serialize<Mode>(counter, graph);
  graph.serializedSize = counter.size();
  CistaBoundedBuffer buffer(maximumPayloadBytes, counter.size());
  cista::serialize<Mode>(buffer, graph);
  return frameArchive(buffer.release(), archiveLimits);
}

template <cista::mode Mode>
void validateCistaArchiveWithMode(std::string_view bytes, const CistaArchiveLimits& archiveLimits)
{
  static_cast<void>(readArchive<Mode>(bytes, archiveLimits));
}

template <cista::mode Mode>
std::string renderCistaArchiveWithMode(std::string_view bytes, const mustache::Data& data,
    const CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits)
{
  const ArchiveGraph& graph = readArchive<Mode>(bytes, archiveLimits);
  std::string output;
  mustache::Renderer renderer;
  renderer.init(nullptr, &data, nullptr, &output, renderLimits);
  ArchivePartialSource partialSource(graph);
  mustache::detail::RenderEngine<ArchivePartialSource> engine(renderer, partialSource);
  engine.renderRoot(ArchiveNodeView::fromNode(&graph, graph.root));
  return output;
}

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES) && !defined(MUSTACHE_CISTA_ARCHIVE_PROTOTYPE_ONLY)
CistaArchiveLimits toCistaArchiveLimits(const mustache::ArchivedTemplateLimits& limits)
{
  CistaArchiveLimits converted;
  converted.maxInputBytes = limits.maxInputBytes;
  converted.maxNestingDepth = limits.maxNestingDepth;
  converted.maxNodes = limits.maxNodes;
  converted.maxStringBytes = limits.maxStringBytes;
  converted.maxDataPartsPerNode = limits.maxDataPartsPerNode;
  converted.maxDataParts = limits.maxDataParts;
  return converted;
}

const void * validateProtectedArchive(std::string_view bytes, const mustache::ArchivedTemplateLimits& limits)
{
  return &readArchive<archiveModeDeepCheckAndIntegrity>(bytes, toCistaArchiveLimits(limits));
}

std::string renderProtectedArchive(
    const void * validatedGraph, const mustache::Data& data, const mustache::RenderLimits& renderLimits)
{
  if (validatedGraph == nullptr) {
    throw mustache::Exception("Empty archived template");
  }
  const ArchiveGraph& graph = *static_cast<const ArchiveGraph *>(validatedGraph);
  std::string output;
  mustache::Renderer renderer;
  renderer.init(nullptr, &data, nullptr, &output, renderLimits);
  ArchivePartialSource partialSource(graph);
  mustache::detail::RenderEngine<ArchivePartialSource> engine(renderer, partialSource);
  engine.renderRoot(ArchiveNodeView::fromNode(&graph, graph.root));
  return output;
}
#endif

} // namespace

#if defined(_MSC_VER) && defined(_M_IX86)
namespace detail {

std::size_t win32ArchiveGraphReservedMemberOffset() noexcept
{
  return cista_member_offset(ArchiveGraph, reserved);
}

} // namespace detail
#endif

const char * cistaSecurityModeName(CistaSecurityMode mode) noexcept
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return "neither";
    case CistaSecurityMode::DeepCheck:
      return "deep_check";
    case CistaSecurityMode::Integrity:
      return "integrity";
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return "deep_check_integrity";
  }
  return "unknown";
}

const char * cistaVersionModeName() noexcept
{
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
  return "version";
#else
  return "static_version";
#endif
}

const char * cistaIntegrityAlgorithmName() noexcept
{
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
  return "cista_xxh3_64";
#else
  return "cista_fnv1a_64";
#endif
}

const char * cistaChecksumAlgorithmName(CistaChecksumAlgorithm algorithm) noexcept
{
  switch (algorithm) {
    case CistaChecksumAlgorithm::None:
      return "none";
    case CistaChecksumAlgorithm::Fnv1a64:
      return "fnv1a_64";
    case CistaChecksumAlgorithm::Crc32:
      return "crc32";
    case CistaChecksumAlgorithm::Xxh3_64:
      return "xxh3_64";
  }
  return "unknown";
}

std::uint64_t checksumCistaArchive(std::string_view bytes, CistaChecksumAlgorithm algorithm)
{
  const char * data = bytes.empty() ? "" : bytes.data();
  switch (algorithm) {
    case CistaChecksumAlgorithm::None:
      return 0;
    case CistaChecksumAlgorithm::Fnv1a64: {
      std::uint64_t hash = UINT64_C(14695981039346656037);
      for (const unsigned char byte : bytes) {
        hash = (hash ^ byte) * UINT64_C(1099511628211);
      }
      return hash;
    }
    case CistaChecksumAlgorithm::Crc32:
      return static_cast<std::uint32_t>(
          crc32_z(0, reinterpret_cast<const Bytef *>(data), static_cast<z_size_t>(bytes.size())));
    case CistaChecksumAlgorithm::Xxh3_64:
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
      return mustache_cista_xxh3_64bits_with_seed(data, bytes.size(), 0);
#else
      return XXH3_64bits(data, bytes.size());
#endif
  }
  throw mustache::Exception("Unknown Cista checksum algorithm");
}

std::vector<std::uint8_t> serializeCistaArchive(
    const mustache::Node& root, const mustache::Node::Partials& partials, const CistaArchiveLimits& archiveLimits)
{
  return serializeCistaArchive(root, partials, CistaSecurityMode::DeepCheckAndIntegrity, archiveLimits);
}

std::vector<std::uint8_t> serializeCistaArchive(const mustache::Node& root, const mustache::Node::Partials& partials,
    CistaSecurityMode mode, const CistaArchiveLimits& archiveLimits)
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return serializeCistaArchiveWithMode<archiveModeNeither>(root, partials, archiveLimits);
    case CistaSecurityMode::DeepCheck:
      return serializeCistaArchiveWithMode<archiveModeDeepCheck>(root, partials, archiveLimits);
    case CistaSecurityMode::Integrity:
      return serializeCistaArchiveWithMode<archiveModeIntegrity>(root, partials, archiveLimits);
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return serializeCistaArchiveWithMode<archiveModeDeepCheckAndIntegrity>(root, partials, archiveLimits);
  }
  throw mustache::Exception("Unknown Cista security mode");
}

void validateCistaArchive(std::string_view bytes, const CistaArchiveLimits& archiveLimits)
{
  validateCistaArchive(bytes, CistaSecurityMode::DeepCheckAndIntegrity, archiveLimits);
}

void validateCistaArchive(std::string_view bytes, CistaSecurityMode mode, const CistaArchiveLimits& archiveLimits)
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return validateCistaArchiveWithMode<archiveModeNeither>(bytes, archiveLimits);
    case CistaSecurityMode::DeepCheck:
      return validateCistaArchiveWithMode<archiveModeDeepCheck>(bytes, archiveLimits);
    case CistaSecurityMode::Integrity:
      return validateCistaArchiveWithMode<archiveModeIntegrity>(bytes, archiveLimits);
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return validateCistaArchiveWithMode<archiveModeDeepCheckAndIntegrity>(bytes, archiveLimits);
  }
  throw mustache::Exception("Unknown Cista security mode");
}

std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data,
    const CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits)
{
  return renderCistaArchive(bytes, data, CistaSecurityMode::DeepCheckAndIntegrity, archiveLimits, renderLimits);
}

std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data, CistaSecurityMode mode,
    const CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits)
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return renderCistaArchiveWithMode<archiveModeNeither>(bytes, data, archiveLimits, renderLimits);
    case CistaSecurityMode::DeepCheck:
      return renderCistaArchiveWithMode<archiveModeDeepCheck>(bytes, data, archiveLimits, renderLimits);
    case CistaSecurityMode::Integrity:
      return renderCistaArchiveWithMode<archiveModeIntegrity>(bytes, data, archiveLimits, renderLimits);
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return renderCistaArchiveWithMode<archiveModeDeepCheckAndIntegrity>(bytes, data, archiveLimits, renderLimits);
  }
  throw mustache::Exception("Unknown Cista security mode");
}

} // namespace mustache_benchmark

#if defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES) && !defined(MUSTACHE_CISTA_ARCHIVE_PROTOTYPE_ONLY)
namespace mustache {

struct ArchivedTemplateView::State {
    explicit State(std::vector<std::uint8_t> archiveBytes) :
        bytes(std::move(archiveBytes)),
        graph(nullptr)
    {}

    std::vector<std::uint8_t> bytes;
    const void * graph;
};

ArchivedTemplateView::ArchivedTemplateView() noexcept = default;

ArchivedTemplateView::ArchivedTemplateView(const ArchivedTemplateView& other) noexcept = default;

ArchivedTemplateView& ArchivedTemplateView::operator=(const ArchivedTemplateView& other) noexcept = default;

ArchivedTemplateView::ArchivedTemplateView(ArchivedTemplateView&& other) noexcept = default;

ArchivedTemplateView& ArchivedTemplateView::operator=(ArchivedTemplateView&& other) noexcept = default;

ArchivedTemplateView::~ArchivedTemplateView() = default;

ArchivedTemplateView::ArchivedTemplateView(std::shared_ptr<const State> state) noexcept :
    state(std::move(state))
{}

bool ArchivedTemplateView::empty() const noexcept
{
  return !state;
}

ArchivedTemplateView::operator bool() const noexcept
{
  return !empty();
}

std::vector<std::uint8_t> serializeArchivedTemplate(
    const Node& root, const Node::Partials& partials, const ArchivedTemplateLimits& limits)
{
  return mustache_benchmark::serializeCistaArchive(root, partials, mustache_benchmark::toCistaArchiveLimits(limits));
}

ArchivedTemplateView loadArchivedTemplate(const std::vector<std::uint8_t>& bytes, const ArchivedTemplateLimits& limits)
{
  if (bytes.size() > limits.maxInputBytes) {
    throw Exception("Cista archive input byte limit exceeded");
  }
  std::shared_ptr<ArchivedTemplateView::State> loaded =
      std::make_shared<ArchivedTemplateView::State>(std::vector<std::uint8_t>(bytes));
  const char * data = loaded->bytes.empty() ? "" : reinterpret_cast<const char *>(loaded->bytes.data());
  const std::string_view archivedBytes(data, loaded->bytes.size());
  loaded->graph = mustache_benchmark::validateProtectedArchive(archivedBytes, limits);
  return ArchivedTemplateView(std::move(loaded));
}

ArchivedTemplateView loadArchivedTemplate(std::string_view bytes, const ArchivedTemplateLimits& limits)
{
  if (bytes.size() > limits.maxInputBytes) {
    throw Exception("Cista archive input byte limit exceeded");
  }
  std::vector<std::uint8_t> ownedBytes(bytes.size());
  if (!bytes.empty()) {
    std::memcpy(ownedBytes.data(), bytes.data(), bytes.size());
  }
  std::shared_ptr<ArchivedTemplateView::State> loaded =
      std::make_shared<ArchivedTemplateView::State>(std::move(ownedBytes));
  const char * data = loaded->bytes.empty() ? "" : reinterpret_cast<const char *>(loaded->bytes.data());
  const std::string_view archivedBytes(data, loaded->bytes.size());
  loaded->graph = mustache_benchmark::validateProtectedArchive(archivedBytes, limits);
  return ArchivedTemplateView(std::move(loaded));
}

std::string render(const ArchivedTemplateView& archived, const Data& data)
{
  return render(archived, data, RenderLimits());
}

std::string render(const ArchivedTemplateView& archived, const Data& data, const RenderLimits& limits)
{
  if (archived.empty()) {
    throw Exception("Empty archived template");
  }
  return mustache_benchmark::renderProtectedArchive(archived.state->graph, data, limits);
}

std::string Mustache::render(const ArchivedTemplateView& archived, const Data& data) const
{
  return mustache::render(archived, data);
}

std::string Mustache::render(const ArchivedTemplateView& archived, const Data& data, const RenderLimits& limits) const
{
  return mustache::render(archived, data, limits);
}

} // namespace mustache
#endif
