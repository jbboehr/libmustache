#ifndef MUSTACHE_ARCHIVE_CISTA_VERSION_HPP
#define MUSTACHE_ARCHIVE_CISTA_VERSION_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <string_view>
#include <type_traits>

// Include this private header only after the selected Cista serialization
// header. The build-time algorithm witness and the production adapter must use
// exactly the same schema and allocation-free runtime-version implementation.
namespace mustache_benchmark {
namespace {

namespace archive_data = cista::offset;

constexpr std::uint64_t archiveGraphMagic = UINT64_C(0x4D55535443495354);
constexpr std::uint32_t archiveSchemaVersion = 1;
constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

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

struct ArchiveTypeHashInsertion {
    unsigned ordering;
    bool inserted;
};

class ArchiveTypeHashState {
  public:
    ArchiveTypeHashInsertion add(cista::hash_t value) noexcept
    {
      for (std::size_t index = 0; index < size_; ++index) {
        if (values_[index] == value) {
          return {static_cast<unsigned>(index), false};
        }
      }
      if (size_ == values_.size()) {
        // A schema change must not turn the public noexcept cache tag into an
        // out-of-bounds write. Review and increase the fixed capacity first.
        std::terminate();
      }
      const unsigned ordering = static_cast<unsigned>(size_);
      values_[size_] = value;
      ++size_;
      return {ordering, true};
    }

  private:
    std::array<cista::hash_t, 128> values_{};
    std::size_t size_ = 0;
};

template <typename Type>
cista::hash_t archiveRuntimeTypeHash(const Type&, cista::hash_t, ArchiveTypeHashState&) noexcept;

template <typename Element, template <typename> typename Pointer, bool Indexed, typename Size>
cista::hash_t archiveRuntimeTypeHash(const cista::basic_vector<Element, Pointer, Indexed, Size>&, cista::hash_t hash,
    ArchiveTypeHashState& state) noexcept;

template <typename Type> cista::hash_t archiveCanonicalTypeHash() noexcept
{
  using Decayed = cista::decay_t<Type>;
  constexpr std::string_view typeName = cista::type_str<Decayed>();
  std::array<char, typeName.size()> canonicalName{};
  std::copy(typeName.begin(), typeName.end(), canonicalName.begin());
  std::size_t canonicalSize = canonicalName.size();

  const auto removeAll = [&canonicalName, &canonicalSize](std::string_view removed) noexcept {
    std::size_t position = 0;
    while (position + removed.size() <= canonicalSize) {
      std::size_t matched = 0;
      while (matched < removed.size() && canonicalName[position + matched] == removed[matched]) {
        ++matched;
      }
      if (matched == removed.size()) {
        for (std::size_t source = position + removed.size(); source < canonicalSize; ++source) {
          canonicalName[source - removed.size()] = canonicalName[source];
        }
        canonicalSize -= removed.size();
      } else {
        ++position;
      }
    }
  };

  removeAll("{anonymous}::");
  removeAll("(anonymous namespace)::");
  removeAll("`anonymous-namespace'::");
  removeAll("struct");
  removeAll("const");
  removeAll(" ");
  return cista::hash_combine(cista::hash(std::string_view(canonicalName.data(), canonicalSize)), sizeof(Decayed));
}

template <typename Type>
cista::hash_t archiveRuntimeTypeHash(const Type& value, cista::hash_t hash, ArchiveTypeHashState& state) noexcept
{
  using Decayed = cista::decay_t<Type>;
  const ArchiveTypeHashInsertion insertion = state.add(archiveCanonicalTypeHash<Decayed>());
  if (!insertion.inserted) {
    return cista::hash_combine(hash, insertion.ordering);
  }

  if constexpr (cista::is_pointer_v<Decayed>) {
    using Pointee = cista::remove_pointer_t<Decayed>;
    if constexpr (std::is_same_v<Pointee, void>) {
      return cista::hash_combine(hash, cista::hash("void*"));
    } else {
      return archiveRuntimeTypeHash(Pointee{}, cista::hash_combine(hash, cista::hash("pointer")), state);
    }
  } else if constexpr (std::is_integral_v<Decayed>) {
    return cista::hash_combine(hash, cista::hash("i"), sizeof(Decayed));
  } else if constexpr (std::is_scalar_v<Decayed>) {
    return cista::hash_combine(hash, archiveCanonicalTypeHash<Decayed>());
  } else {
    static_assert(cista::to_tuple_works_v<Decayed>, "Archive type must have a Cista tuple representation");
    hash = cista::hash_combine(hash, cista::hash("struct"));
    cista::for_each_field(value, [&hash, &state](const auto& member) noexcept {
      hash = archiveRuntimeTypeHash(member, hash, state);
    });
    return hash;
  }
}

template <typename Element, template <typename> typename Pointer, bool Indexed, typename Size>
cista::hash_t archiveRuntimeTypeHash(const cista::basic_vector<Element, Pointer, Indexed, Size>&, cista::hash_t hash,
    ArchiveTypeHashState& state) noexcept
{
  hash = cista::hash_combine(hash, cista::hash("vector"));
  return archiveRuntimeTypeHash(Element{}, hash, state);
}

std::uint64_t archiveRuntimeTypeVersion() noexcept
{
  ArchiveTypeHashState state;
  return static_cast<std::uint64_t>(archiveRuntimeTypeHash(ArchiveGraph{}, cista::BASE_HASH, state));
}

} // namespace
} // namespace mustache_benchmark

#endif
