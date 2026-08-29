#include "cista-archive.hpp"
#include "render_engine.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class CursorContractNodeView {
  public:
    class ChildCursor {
      public:
        ChildCursor() noexcept :
            partCursorRequests_(nullptr),
            partValues_(nullptr),
            valid_(false)
        {}

        explicit operator bool() const noexcept
        {
          return valid_;
        }

        CursorContractNodeView value() const noexcept
        {
          return CursorContractNodeView(Kind::Variable, partCursorRequests_, partValues_);
        }

        void advance() noexcept
        {
          valid_ = false;
        }

      private:
        friend class CursorContractNodeView;

        ChildCursor(std::size_t * partCursorRequests, std::size_t * partValues) noexcept :
            partCursorRequests_(partCursorRequests),
            partValues_(partValues),
            valid_(true)
        {}

        std::size_t * partCursorRequests_;
        std::size_t * partValues_;
        bool valid_;
    };

    class DataPartCursor {
      public:
        DataPartCursor() noexcept :
            values_(nullptr),
            index_(3)
        {}

        explicit operator bool() const noexcept
        {
          return values_ != nullptr && index_ < 3;
        }

        mustache::detail::RenderString value() const noexcept
        {
          static constexpr std::array<std::string_view, 3> parts = {"a", "b", "c"};
          ++*values_;
          return mustache::detail::RenderString::fromView(parts[index_]);
        }

        void advance() noexcept
        {
          ++index_;
        }

      private:
        friend class CursorContractNodeView;

        explicit DataPartCursor(std::size_t * values) noexcept :
            values_(values),
            index_(0)
        {}

        std::size_t * values_;
        std::size_t index_;
    };

    CursorContractNodeView() noexcept :
        kind_(Kind::Null),
        partCursorRequests_(nullptr),
        partValues_(nullptr)
    {}

    static CursorContractNodeView root(std::size_t * partCursorRequests, std::size_t * partValues) noexcept
    {
      return CursorContractNodeView(Kind::Root, partCursorRequests, partValues);
    }

    explicit operator bool() const noexcept
    {
      return kind_ != Kind::Null;
    }

    mustache::Node::Type type() const noexcept
    {
      return kind_ == Kind::Root ? mustache::Node::TypeRoot : mustache::Node::TypeVariable;
    }

    int flags() const noexcept
    {
      return kind_ == Kind::Variable ? mustache::Node::FlagEscape : mustache::Node::FlagNone;
    }

    mustache::detail::RenderString data() const noexcept
    {
      return kind_ == Kind::Variable ? mustache::detail::RenderString::fromView("a.b.c")
                                     : mustache::detail::RenderString();
    }

    DataPartCursor dataParts() const noexcept
    {
      if (kind_ != Kind::Variable) {
        return DataPartCursor();
      }
      ++*partCursorRequests_;
      return DataPartCursor(partValues_);
    }

    ChildCursor children() const noexcept
    {
      return kind_ == Kind::Root ? ChildCursor(partCursorRequests_, partValues_) : ChildCursor();
    }

    CursorContractNodeView containerChild() const noexcept
    {
      return CursorContractNodeView();
    }

    mustache::detail::RenderString startSequence() const noexcept
    {
      return mustache::detail::RenderString();
    }

    mustache::detail::RenderString stopSequence() const noexcept
    {
      return mustache::detail::RenderString();
    }

  private:
    enum class Kind {
      Null,
      Root,
      Variable
    };

    CursorContractNodeView(Kind kind, std::size_t * partCursorRequests, std::size_t * partValues) noexcept :
        kind_(kind),
        partCursorRequests_(partCursorRequests),
        partValues_(partValues)
    {}

    Kind kind_;
    std::size_t * partCursorRequests_;
    std::size_t * partValues_;
};

class EmptyPartialSource {
  public:
    template <typename Callback> bool withPartial(mustache::detail::RenderString, Callback&&) const noexcept
    {
      return false;
    }
};

class FixedLambda final : public mustache::Lambda {
  public:
    explicit FixedLambda(std::string value = "lambda") :
        value_(std::move(value))
    {}

    std::string invoke() override
    {
      return value_;
    }

  private:
    std::string value_;
};

class RenderingSectionLambda final : public mustache::Lambda {
  public:
    explicit RenderingSectionLambda(std::string * observed) :
        observed_(observed)
    {}

    std::string invoke() override
    {
      return std::string();
    }

    std::string invoke(std::string_view text, mustache::LambdaRenderContext context) override
    {
      observed_->assign(text.data(), text.size());
      mustache::Node name(mustache::Node::TypeVariable, "name", mustache::Node::FlagEscape);
      return context.render(name);
    }

  private:
    std::string * observed_;
};

void expect(bool condition, const char * message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

constexpr std::size_t archivePreambleSize = 16;

std::uint64_t readLittleEndian(const std::vector<std::uint8_t>& bytes, std::size_t offset, std::size_t width)
{
  if (offset > bytes.size() || width > bytes.size() - offset || width > sizeof(std::uint64_t)) {
    throw std::runtime_error("archive preamble field is out of bounds");
  }
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < width; ++index) {
    value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
  }
  return value;
}

template <typename Field> Field readNativeArchiveField(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
  if (offset > bytes.size() || sizeof(Field) > bytes.size() - offset) {
    throw std::runtime_error("native archive field is out of bounds");
  }
  Field value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

template <typename Field>
void writeNativeArchiveField(std::vector<std::uint8_t> * bytes, std::size_t offset, Field value)
{
  if (offset > bytes->size() || sizeof(value) > bytes->size() - offset) {
    throw std::runtime_error("native archive field is out of bounds");
  }
  std::memcpy(bytes->data() + offset, &value, sizeof(value));
}

bool isGoldenPlatform(std::size_t pointerBytes) noexcept
{
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3) && (defined(__x86_64__) || defined(_M_X64)) && !defined(_MSC_VER)
  const std::uint16_t value = 1;
  return pointerBytes == 8 && *reinterpret_cast<const std::uint8_t *>(&value) == 1;
#else
  static_cast<void>(pointerBytes);
  return false;
#endif
}

mustache_benchmark::CistaChecksumAlgorithm cistaIntegrityChecksumAlgorithm() noexcept
{
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
  return mustache_benchmark::CistaChecksumAlgorithm::Xxh3_64;
#else
  return mustache_benchmark::CistaChecksumAlgorithm::Fnv1a64;
#endif
}

std::uint8_t hexNibble(char value)
{
  if (value >= '0' && value <= '9') {
    return static_cast<std::uint8_t>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<std::uint8_t>(value - 'a' + 10);
  }
  if (value >= 'A' && value <= 'F') {
    return static_cast<std::uint8_t>(value - 'A' + 10);
  }
  throw std::runtime_error("golden Cista archive contains a non-hexadecimal byte");
}

std::vector<std::uint8_t> readGoldenArchive()
{
#if defined(_MSC_VER)
  char * environmentValue = nullptr;
  std::size_t environmentValueSize = 0;
  if (_dupenv_s(&environmentValue, &environmentValueSize, "top_srcdir") != 0) {
    throw std::runtime_error("unable to read top_srcdir");
  }
  const std::unique_ptr<char, decltype(&std::free)> ownedEnvironmentValue(environmentValue, &std::free);
  const char * topSourceDirectory = ownedEnvironmentValue.get();
#else
  const char * topSourceDirectory = std::getenv("top_srcdir");
#endif
  if (topSourceDirectory == nullptr || *topSourceDirectory == '\0') {
    throw std::runtime_error("top_srcdir is required to locate the golden Cista archive");
  }
  const std::string path = std::string(topSourceDirectory) + "/tests/fixtures/cista-archive-v1-x86_64-le-itanium.hex";
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("unable to open the golden Cista archive");
  }
  std::vector<std::uint8_t> bytes;
  std::string encoded;
  while (stream >> encoded) {
    if (encoded.size() != 2) {
      throw std::runtime_error("golden Cista archive contains a malformed byte");
    }
    bytes.push_back(static_cast<std::uint8_t>((hexNibble(encoded[0]) << 4) | hexNibble(encoded[1])));
  }
  if (!stream.eof() || bytes.empty()) {
    throw std::runtime_error("unable to read the golden Cista archive");
  }
  return bytes;
}

void testDataPartCursorContract()
{
  mustache::Data levelC = mustache::Data::object();
  levelC.set("c", mustache::Data::string("cursor"));
  mustache::Data levelB = mustache::Data::object();
  levelB.set("b", std::move(levelC));
  mustache::Data data = mustache::Data::object();
  data.set("a", std::move(levelB));

  std::size_t cursorRequests = 0;
  std::size_t partValues = 0;
  std::string output;
  mustache::Renderer renderer;
  renderer.init(nullptr, &data, nullptr, &output);
  EmptyPartialSource partialSource;
  mustache::detail::RenderEngine<EmptyPartialSource> engine(renderer, partialSource);
  engine.renderRoot(CursorContractNodeView::root(&cursorRequests, &partValues));

  expect(output == "cursor", "data-part cursor adapter did not render a three-component lookup");
  expect(cursorRequests == 1, "shared lookup requested the data-part cursor more than once");
  expect(partValues == 3, "shared lookup did not consume dotted-name components exactly once");
}

mustache::Data makeData()
{
  mustache::Data first = mustache::Data::object();
  first.set("name", mustache::Data::string("<first>'"));
  first.set("category", mustache::Data::object().set("slug", mustache::Data::string("one")));
  first.set("visible", mustache::Data::boolean(true));

  mustache::Data second = mustache::Data::object();
  second.set("name", mustache::Data::string("second"));
  second.set("category", mustache::Data::object().set("slug", mustache::Data::string("two")));
  second.set("visible", mustache::Data::boolean(false));

  mustache::Data products = mustache::Data::array();
  products.push_back(std::move(first));
  products.push_back(std::move(second));

  mustache::Data root = mustache::Data::object();
  root.set("products", std::move(products));
  return root;
}

void expectRejected(std::string_view bytes, const mustache::Data& data, const char * description)
{
  try {
    mustache_benchmark::renderCistaArchive(bytes, data);
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string("invalid Cista archive was accepted: ") + description);
}

void rewriteProtectedArchiveIntegrity(std::vector<std::uint8_t> * archive, std::size_t graphOffset)
{
  constexpr std::size_t cistaVersionFieldSize = 8;
  constexpr std::size_t cistaIntegrityFieldOffset = archivePreambleSize + cistaVersionFieldSize;
  const std::uint64_t integrity = mustache_benchmark::checksumCistaArchive(
      std::string_view(reinterpret_cast<const char *>(archive->data() + graphOffset), archive->size() - graphOffset),
      cistaIntegrityChecksumAlgorithm());
  writeNativeArchiveField(archive, cistaIntegrityFieldOffset, integrity);
}

template <typename Mutator>
void expectProtectedMutationRejected(const std::vector<std::uint8_t>& archive, std::size_t graphOffset,
    const mustache::Data& data, Mutator&& mutate, const std::string& description)
{
  std::vector<std::uint8_t> mutated = archive;
  std::forward<Mutator>(mutate)(&mutated);
  rewriteProtectedArchiveIntegrity(&mutated, graphOffset);
  expectRejected(
      std::string_view(reinterpret_cast<const char *>(mutated.data()), mutated.size()), data, description.c_str());
}

void testProtectedArchiveVectorValidation(const std::vector<std::uint8_t>& archive, const mustache::Data& data)
{
  static_assert(sizeof(std::intptr_t) == sizeof(void *), "Cista offsets must match the native pointer width");
  constexpr std::size_t cistaVersionFieldSize = 8;
  constexpr std::size_t cistaIntegrityFieldSize = 8;
  constexpr std::size_t graphOffset = archivePreambleSize + cistaVersionFieldSize + cistaIntegrityFieldSize;
  constexpr std::size_t firstVectorOffset = graphOffset + 24;
  constexpr std::size_t serializedVectorSize = sizeof(std::intptr_t) + 16;
  constexpr std::size_t usedSizeMemberOffset = sizeof(std::intptr_t);
  constexpr std::size_t allocatedSizeMemberOffset = usedSizeMemberOffset + sizeof(std::uint32_t);
  constexpr std::size_t selfAllocatedMemberOffset = allocatedSizeMemberOffset + sizeof(std::uint32_t);

  struct VectorLayout {
      const char * name;
      std::size_t headerOffset;
      std::size_t elementSize;
      std::size_t elementAlignment;
  };
  const std::array<VectorLayout, 3> layouts = {{
      {"nodes", firstVectorOffset, 40, 4},
      {"partials", firstVectorOffset + serializedVectorSize, 12, 4},
      {"strings", firstVectorOffset + serializedVectorSize * 2, 1, 1},
  }};
  const std::size_t payloadSize = archive.size() - archivePreambleSize;

  for (const VectorLayout& layout : layouts) {
    const std::size_t pointerOffsetInPayload = layout.headerOffset - archivePreambleSize;
    const std::size_t usedSizeOffset = layout.headerOffset + usedSizeMemberOffset;
    const std::size_t allocatedSizeOffset = layout.headerOffset + allocatedSizeMemberOffset;
    const std::size_t selfAllocatedOffset = layout.headerOffset + selfAllocatedMemberOffset;
    const std::uint32_t originalUsedSize = readNativeArchiveField<std::uint32_t>(archive, usedSizeOffset);
    expect(originalUsedSize != 0, "vector validation fixture unexpectedly contains an empty vector");

    const auto reject = [&](auto&& mutate, const char * caseDescription) {
      expectProtectedMutationRejected(archive, graphOffset, data, std::forward<decltype(mutate)>(mutate),
          std::string(layout.name) + " vector " + caseDescription);
    };

    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, layout.headerOffset, std::numeric_limits<std::intptr_t>::min());
        },
        "null pointer with nonzero size");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, layout.headerOffset, std::numeric_limits<std::intptr_t>::max());
        },
        "maximum positive offset");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, layout.headerOffset, std::numeric_limits<std::intptr_t>::min() + 1);
        },
        "near-minimum negative offset");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          const std::intptr_t underflowOffset = -static_cast<std::intptr_t>(pointerOffsetInPayload) - 1;
          writeNativeArchiveField(mutated, layout.headerOffset, underflowOffset);
        },
        "negative offset before the payload");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, layout.headerOffset, std::intptr_t{0});
          writeNativeArchiveField(mutated, usedSizeOffset, std::uint32_t{0});
          writeNativeArchiveField(mutated, allocatedSizeOffset, std::uint32_t{0});
        },
        "nonnull pointer with zero sizes");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, layout.headerOffset, std::numeric_limits<std::intptr_t>::min());
          writeNativeArchiveField(mutated, usedSizeOffset, std::uint32_t{0});
          writeNativeArchiveField(mutated, allocatedSizeOffset, std::uint32_t{0});
        },
        "valid null layout with an invalid graph");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, allocatedSizeOffset, originalUsedSize + 1);
        },
        "used and allocated size mismatch");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, selfAllocatedOffset, std::uint8_t{1});
        },
        "self-allocated state");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(mutated, usedSizeOffset, std::numeric_limits<std::uint32_t>::max());
          writeNativeArchiveField(mutated, allocatedSizeOffset, std::numeric_limits<std::uint32_t>::max());
        },
        "maximum sizes");

    const std::uintmax_t originalByteSize =
        static_cast<std::uintmax_t>(originalUsedSize) * static_cast<std::uintmax_t>(layout.elementSize);
    expect(originalByteSize < payloadSize, "vector validation fixture cannot exercise a one-byte range overflow");
    const std::size_t overflowingDataOffset = payloadSize - static_cast<std::size_t>(originalByteSize) + 1;
    expect(overflowingDataOffset >= pointerOffsetInPayload,
        "vector validation fixture unexpectedly requires a negative range-overflow offset");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(
              mutated, layout.headerOffset, static_cast<std::intptr_t>(overflowingDataOffset - pointerOffsetInPayload));
        },
        "span one byte beyond the payload");
    reject(
        [&](std::vector<std::uint8_t> * mutated) {
          writeNativeArchiveField(
              mutated, layout.headerOffset, static_cast<std::intptr_t>(payloadSize - pointerOffsetInPayload));
        },
        "nonnull pointer at the exact payload end");

    if (layout.elementAlignment > 1) {
      reject(
          [&](std::vector<std::uint8_t> * mutated) {
            writeNativeArchiveField(mutated, layout.headerOffset, std::intptr_t{1});
          },
          "misaligned in-range pointer");
      reject(
          [&](std::vector<std::uint8_t> * mutated) {
            writeNativeArchiveField(mutated, layout.headerOffset, std::intptr_t{-1});
          },
          "misaligned negative pointer");
    } else {
      const std::intptr_t originalRelativeOffset = readNativeArchiveField<std::intptr_t>(archive, layout.headerOffset);
      expect(originalRelativeOffset >= 0, "strings vector unexpectedly uses a negative offset");
      const std::size_t originalDataOffset = pointerOffsetInPayload + static_cast<std::size_t>(originalRelativeOffset);
      expect(originalDataOffset + static_cast<std::size_t>(originalByteSize) == payloadSize,
          "strings vector no longer exercises a valid exact-end span");
    }
  }
}

void expectPreambleMutationRejected(
    const std::vector<std::uint8_t>& archive, std::size_t offset, const mustache::Data& data, const char * description)
{
  std::vector<std::uint8_t> mutated = archive;
  mutated.at(offset) ^= std::uint8_t{0x80};
  expectRejected(std::string_view(reinterpret_cast<const char *>(mutated.data()), mutated.size()), data, description);
}

void expectSingleByteMutationsRejected(
    const std::vector<std::uint8_t>& archive, const mustache::Data& data, const char * description)
{
  for (std::size_t offset = 0; offset < archive.size(); ++offset) {
    std::vector<std::uint8_t> mutated = archive;
    mutated[offset] ^= std::uint8_t{0x80};
    try {
      (void)mustache_benchmark::renderCistaArchive(
          std::string_view(reinterpret_cast<const char *>(mutated.data()), mutated.size()), data);
    } catch (const std::exception&) {
      continue;
    }
    throw std::runtime_error(std::string("single-byte archive mutation was accepted at offset ") +
        std::to_string(offset) + ": " + description);
  }
}

void expectRejected(std::string_view bytes, const mustache::Data& data,
    const mustache_benchmark::CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits,
    const char * description)
{
  try {
    mustache_benchmark::renderCistaArchive(bytes, data, archiveLimits, renderLimits);
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string("limited Cista archive render was accepted: ") + description);
}

template <typename Operation> void expectOperationRejected(Operation&& operation, const char * description)
{
  try {
    operation();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(std::string("invalid Cista archive operation was accepted: ") + description);
}

void expectEqualArchives(
    const std::vector<std::uint8_t>& first, const std::vector<std::uint8_t>& second, const char * description)
{
  if (first.size() != second.size()) {
    throw std::runtime_error(
        std::string(description) + ": sizes " + std::to_string(first.size()) + " and " + std::to_string(second.size()));
  }
  const auto mismatch = std::mismatch(first.begin(), first.end(), second.begin());
  if (mismatch.first != first.end()) {
    throw std::runtime_error(std::string(description) + " at byte " +
        std::to_string(static_cast<std::size_t>(mismatch.first - first.begin())) + ": values " +
        std::to_string(*mismatch.first) + " and " + std::to_string(*mismatch.second));
  }
}

} // namespace

int main()
{
  try {
    expect(!isGoldenPlatform(4), "x86-64 x32 ABI incorrectly selected the 64-bit golden archive");
    testDataPartCursorContract();

    mustache::Mustache engine;
    mustache::Node root;
    engine.tokenize("{{#products}}\n  {{> card}}\n{{/products}}\n", &root);

    mustache::Node::Partials partials;
    mustache::Node card;
    engine.tokenize("<p class=\"{{category.slug}}\">{{name}}</p>{{^visible}}hidden{{/visible}}\n", &card);
    partials.emplace("card", std::make_unique<mustache::Node>(std::move(card)));

    const mustache::Data data = makeData();
    std::string expected;
    engine.render(&root, &data, &partials, &expected);

    const std::vector<std::uint8_t> archive = mustache_benchmark::serializeCistaArchive(root, partials);
    expect(archive == mustache_benchmark::serializeCistaArchive(root, partials),
        "Cista archive serialization is not deterministic");
    constexpr std::array<std::uint8_t, 8> expectedMagic = {'M', 'U', 'S', 'T', 'A', 'R', 'C', 0};
    expect(archive.size() > archivePreambleSize, "Cista archive preamble has no payload");
    expect(std::equal(expectedMagic.begin(), expectedMagic.end(), archive.begin()),
        "Cista archive preamble magic changed");
    expect(readLittleEndian(archive, 8, 8) == 1, "libmustache archive format generation changed");
    if (isGoldenPlatform(sizeof(void *))) {
      expect(archive == readGoldenArchive(), "Cista archive differs from the version 1 golden fixture");
    }
    const std::string_view bytes(reinterpret_cast<const char *>(archive.data()), archive.size());
    mustache_benchmark::validateCistaArchive(bytes);
    const std::string actual = mustache_benchmark::renderCistaArchive(bytes, data);
    expect(actual == expected, "Cista archive rendering differs from Node rendering");

    const std::array<mustache_benchmark::CistaSecurityMode, 4> securityModes = {
        mustache_benchmark::CistaSecurityMode::Neither,
        mustache_benchmark::CistaSecurityMode::DeepCheck,
        mustache_benchmark::CistaSecurityMode::Integrity,
        mustache_benchmark::CistaSecurityMode::DeepCheckAndIntegrity,
    };
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
    expect(std::string_view(mustache_benchmark::cistaVersionModeName()) == "version",
        "Cista XXH3 benchmark did not select runtime versioning");
    expect(std::string_view(mustache_benchmark::cistaIntegrityAlgorithmName()) == "cista_xxh3_64",
        "Cista XXH3 benchmark did not select built-in XXH3 integrity");
#else
    expect(std::string_view(mustache_benchmark::cistaVersionModeName()) == "static_version",
        "Cista baseline benchmark did not select static versioning");
    expect(std::string_view(mustache_benchmark::cistaIntegrityAlgorithmName()) == "cista_fnv1a_64",
        "Cista baseline benchmark did not select built-in FNV-1a integrity");
#endif
    std::array<std::vector<std::uint8_t>, securityModes.size()> modeArchives;
    for (std::size_t index = 0; index < securityModes.size(); ++index) {
      const mustache_benchmark::CistaSecurityMode mode = securityModes[index];
      modeArchives[index] = mustache_benchmark::serializeCistaArchive(root, partials, mode);
      mustache_benchmark::validateCistaArchive(
          std::string_view(reinterpret_cast<const char *>(modeArchives[index].data()), modeArchives[index].size()),
          mode);
      const std::string modeOutput = mustache_benchmark::renderCistaArchive(
          std::string_view(reinterpret_cast<const char *>(modeArchives[index].data()), modeArchives[index].size()),
          data, mode);
      expect(modeOutput == expected, "Cista security mode changed rendered output");
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    for (std::size_t repeat = 0; repeat < 64; ++repeat) {
      for (std::size_t index = 0; index < securityModes.size(); ++index) {
        expectEqualArchives(modeArchives[index],
            mustache_benchmark::serializeCistaArchive(root, partials, securityModes[index]),
            "Win32 archive serialization was not deterministic");
      }
    }
#endif
    expectEqualArchives(modeArchives[0], modeArchives[1], "deep checking unexpectedly changed the serialized archive");
    expectEqualArchives(modeArchives[2], modeArchives[3], "deep checking changed the integrity-protected archive");
    expect(modeArchives[2].size() > modeArchives[0].size(), "integrity mode did not add archive framing");
    expect(mustache_benchmark::renderCistaArchive(
               std::string_view(reinterpret_cast<const char *>(modeArchives[0].data()), modeArchives[0].size()), data,
               mustache_benchmark::CistaSecurityMode::DeepCheck) == expected,
        "deep checking is not an independent reader policy");

    std::vector<mustache_benchmark::CistaChecksumAlgorithm> checksumAlgorithms;
    checksumAlgorithms.reserve(3);
    checksumAlgorithms.push_back(mustache_benchmark::CistaChecksumAlgorithm::Fnv1a64);
    checksumAlgorithms.push_back(mustache_benchmark::CistaChecksumAlgorithm::Xxh3_64);
#if defined(MUSTACHE_CISTA_HAVE_ZLIB)
    checksumAlgorithms.push_back(mustache_benchmark::CistaChecksumAlgorithm::Crc32);
#endif
    expect(std::string_view(mustache_benchmark::cistaChecksumAlgorithmName(
               mustache_benchmark::CistaChecksumAlgorithm::None)) == "none",
        "disabled checksum policy is not identified as none");
    expect(mustache_benchmark::checksumCistaArchive(
               "archive payload", mustache_benchmark::CistaChecksumAlgorithm::None) == 0,
        "disabled checksum policy unexpectedly hashed the archive");
    expect(mustache_benchmark::checksumCistaArchive("", mustache_benchmark::CistaChecksumAlgorithm::Fnv1a64) ==
            UINT64_C(0xCBF29CE484222325),
        "FNV-1a checksum does not match the standard empty-input vector");
#if defined(MUSTACHE_CISTA_HAVE_ZLIB)
    expect(mustache_benchmark::checksumCistaArchive("123456789", mustache_benchmark::CistaChecksumAlgorithm::Crc32) ==
            UINT32_C(0xCBF43926),
        "CRC-32 checksum does not match the standard check value");
#else
    expectOperationRejected(
        []() {
          (void)mustache_benchmark::checksumCistaArchive(
              "123456789", mustache_benchmark::CistaChecksumAlgorithm::Crc32);
        },
        "unavailable zlib CRC-32");
#endif
    expect(mustache_benchmark::checksumCistaArchive("", mustache_benchmark::CistaChecksumAlgorithm::Xxh3_64) ==
            UINT64_C(0x2D06800538D394C2),
        "XXH3 checksum does not match the standard empty-input vector");
    for (const mustache_benchmark::CistaChecksumAlgorithm algorithm : checksumAlgorithms) {
      expect(mustache_benchmark::checksumCistaArchive("archive payload", algorithm) !=
              mustache_benchmark::checksumCistaArchive("archive payloae", algorithm),
          "checksum did not detect a one-byte payload change");
    }
    expectOperationRejected(
        []() {
          (void)mustache_benchmark::checksumCistaArchive(
              "archive payload", static_cast<mustache_benchmark::CistaChecksumAlgorithm>(255));
        },
        "unknown checksum algorithm");

    std::vector<std::uint8_t> integrityCorruption = modeArchives[2];
    integrityCorruption.back() ^= std::uint8_t{0xFF};
    bool validationRejected = false;
    try {
      mustache_benchmark::validateCistaArchive(
          std::string_view(reinterpret_cast<const char *>(integrityCorruption.data()), integrityCorruption.size()),
          mustache_benchmark::CistaSecurityMode::Integrity);
    } catch (const mustache::Exception&) {
      validationRejected = true;
    }
    expect(validationRejected, "validation-only API did not translate Cista integrity rejection");
    expectOperationRejected(
        [&integrityCorruption, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(integrityCorruption.data()), integrityCorruption.size()),
              data, mustache_benchmark::CistaSecurityMode::Integrity);
        },
        "integrity-protected corruption");

    mustache::Node invalidType;
    invalidType.type = static_cast<mustache::Node::Type>(65537);
    expectOperationRejected(
        [&invalidType]() {
          (void)mustache_benchmark::serializeCistaArchive(invalidType);
        },
        "node type narrowed before validation");

    mustache_benchmark::CistaArchiveLimits serializationLimits;
    serializationLimits.maxInputBytes = archive.size() - 1;
    expectOperationRejected(
        [&root, &partials, &serializationLimits]() {
          (void)mustache_benchmark::serializeCistaArchive(root, partials, serializationLimits);
        },
        "serialized output exceeds the paired reader limit");
    serializationLimits.maxInputBytes = archive.size();
    expect(mustache_benchmark::serializeCistaArchive(root, partials, serializationLimits) == archive,
        "serialized output at the exact paired reader limit was rejected or changed");

    std::string nulSource("before\0after ", 13);
    nulSource.append("{{products}}");
    mustache::Node nulRoot;
    engine.tokenize(std::string_view(nulSource), &nulRoot);
    std::string expectedNul;
    engine.render(&nulRoot, &data, nullptr, &expectedNul);
    const std::vector<std::uint8_t> nulArchive = mustache_benchmark::serializeCistaArchive(nulRoot);
    const std::string actualNul = mustache_benchmark::renderCistaArchive(
        std::string_view(reinterpret_cast<const char *>(nulArchive.data()), nulArchive.size()), data);
    expect(actualNul == expectedNul, "Cista archive did not preserve embedded NUL bytes");

    mustache::Node nestedRoot;
    engine.tokenize("  {{> outer}}\n", &nestedRoot);
    mustache::Node::Partials nestedPartials;
    std::unique_ptr<mustache::Node> outer = std::make_unique<mustache::Node>();
    engine.tokenize("{{> inner}}\n", outer.get());
    nestedPartials.emplace("outer", std::move(outer));
    std::unique_ptr<mustache::Node> inner = std::make_unique<mustache::Node>();
    engine.tokenize("x\ny\n", inner.get());
    nestedPartials.emplace("inner", std::move(inner));
    std::string expectedNested;
    engine.render(&nestedRoot, &data, &nestedPartials, &expectedNested);
    const std::vector<std::uint8_t> nestedArchive =
        mustache_benchmark::serializeCistaArchive(nestedRoot, nestedPartials);
    const std::string actualNested = mustache_benchmark::renderCistaArchive(
        std::string_view(reinterpret_cast<const char *>(nestedArchive.data()), nestedArchive.size()), data);
    expect(actualNested == expectedNested, "Cista archive lost inherited partial indentation");

    mustache::Node semanticRoot;
    engine.tokenize(
        "{{a.b.c}}|{{#values}}[{{.}}]{{/values}}|{{integer}}|{{floating}}|{{> missing}}|{{=<% %>=}}<%integer%>",
        &semanticRoot);
    mustache::Data values = mustache::Data::array();
    values.push_back(mustache::Data::string("first"));
    values.push_back(mustache::Data::integer(2));
    mustache::Data semanticData = mustache::Data::object();
    mustache::Data nestedC = mustache::Data::object();
    nestedC.set("c", mustache::Data::string("deep"));
    mustache::Data nestedB = mustache::Data::object();
    nestedB.set("b", std::move(nestedC));
    semanticData.set("a", std::move(nestedB));
    semanticData.set("values", std::move(values));
    semanticData.set("integer", mustache::Data::integer(-7));
    semanticData.set("floating", mustache::Data::floating(1.5));
    std::string expectedSemantics;
    engine.render(&semanticRoot, &semanticData, nullptr, &expectedSemantics);
    const std::vector<std::uint8_t> semanticArchive = mustache_benchmark::serializeCistaArchive(semanticRoot);
    const std::string actualSemantics = mustache_benchmark::renderCistaArchive(
        std::string_view(reinterpret_cast<const char *>(semanticArchive.data()), semanticArchive.size()), semanticData);
    expect(
        expectedSemantics == "deep|[first][2]|-7|1.5||-7", "ordinary renderer semantic fixture changed unexpectedly");
    expect(actualSemantics == expectedSemantics,
        "Cista archive changed dotted-name, current-context, numeric, delimiter, or missing-partial semantics");

    mustache::Mustache delimiterEngine;
    delimiterEngine.setStartSequence("<%");
    delimiterEngine.setStopSequence("%>");
    mustache::Node sectionLambdaRoot;
    delimiterEngine.tokenize("<%#section%>original <%name%><%/section%>", &sectionLambdaRoot);
    const std::vector<std::uint8_t> sectionLambdaArchive = mustache_benchmark::serializeCistaArchive(sectionLambdaRoot);
    const std::string_view sectionLambdaBytes(
        reinterpret_cast<const char *>(sectionLambdaArchive.data()), sectionLambdaArchive.size());
    std::string expectedSection;
    mustache::Data ownedSectionLambdaData = mustache::Data::object();
    ownedSectionLambdaData.set("name", mustache::Data::string("<safe>"));
    ownedSectionLambdaData.set(
        "section", mustache::Data::lambda(std::make_unique<RenderingSectionLambda>(&expectedSection)));
    std::string expectedSectionOutput;
    delimiterEngine.render(&sectionLambdaRoot, &ownedSectionLambdaData, nullptr, &expectedSectionOutput);

    std::string actualSection;
    mustache::Data archiveSectionLambdaData = mustache::Data::object();
    archiveSectionLambdaData.set("name", mustache::Data::string("<safe>"));
    archiveSectionLambdaData.set(
        "section", mustache::Data::lambda(std::make_unique<RenderingSectionLambda>(&actualSection)));
    const std::string actualSectionOutput =
        mustache_benchmark::renderCistaArchive(sectionLambdaBytes, archiveSectionLambdaData);
    expect(expectedSectionOutput == "&lt;safe&gt;", "ordinary section-lambda rendering changed unexpectedly");
    expect(actualSectionOutput == expectedSectionOutput,
        "Cista archive section-lambda callback rendering differs from Node rendering");
    expect(expectedSection == "original <%name%>", "ordinary section lambda received unexpected source text");
    expect(actualSection == expectedSection, "Cista archive section-lambda source differs from Node rendering");

    mustache::Node lambdaRoot;
    engine.tokenize("{{call}}", &lambdaRoot);
    const std::vector<std::uint8_t> lambdaArchive = mustache_benchmark::serializeCistaArchive(lambdaRoot);
    const std::string_view lambdaBytes(reinterpret_cast<const char *>(lambdaArchive.data()), lambdaArchive.size());
    mustache::Data plainCallData = mustache::Data::object();
    plainCallData.set("call", mustache::Data::string("plain"));
    expect(mustache_benchmark::renderCistaArchive(lambdaBytes, plainCallData) == "plain",
        "lambda fixture archive could not render ordinary data");
    mustache::Data lambdaData = mustache::Data::object();
    lambdaData.set("call", mustache::Data::lambda(std::make_unique<FixedLambda>()));
    expect(mustache_benchmark::renderCistaArchive(lambdaBytes, lambdaData) == "lambda",
        "Cista archive variable-lambda rendering differs from Node rendering");

    mustache::Node lambdaPartialRoot;
    engine.tokenize("{{call}}", &lambdaPartialRoot);
    mustache::Node::Partials lambdaPartials;
    std::unique_ptr<mustache::Node> lambdaPartial = std::make_unique<mustache::Node>();
    engine.tokenize("[{{name}}]", lambdaPartial.get());
    lambdaPartials.emplace("card", std::move(lambdaPartial));
    const std::vector<std::uint8_t> lambdaPartialArchive =
        mustache_benchmark::serializeCistaArchive(lambdaPartialRoot, lambdaPartials);
    mustache::Data lambdaPartialData = mustache::Data::object();
    lambdaPartialData.set("name", mustache::Data::string("archive"));
    lambdaPartialData.set("call", mustache::Data::lambda(std::make_unique<FixedLambda>("{{>card}}")));
    expect(
        mustache_benchmark::renderCistaArchive(
            std::string_view(reinterpret_cast<const char *>(lambdaPartialArchive.data()), lambdaPartialArchive.size()),
            lambdaPartialData) == "[archive]",
        "lambda-generated owned nodes did not resolve archived partials");

    mustache::RenderLimits lambdaLimits;
    lambdaLimits.maxLambdaTemplateBytes = 0;
    expectRejected(
        lambdaBytes, lambdaData, mustache_benchmark::CistaArchiveLimits(), lambdaLimits, "lambda template byte limit");

    const auto parsedPartial = [&engine](std::string_view source) {
      std::unique_ptr<mustache::Node> partial = std::make_unique<mustache::Node>();
      engine.tokenize(source, partial.get());
      return partial;
    };
    mustache::Node ownedPartialRoot;
    engine.tokenize("{{>card}}|{{>fallback}}|{{>nested}}|{{>leaf}}|{{>missing}}", &ownedPartialRoot);
    ownedPartialRoot.partials.emplace("card", parsedPartial("root-card"));
    ownedPartialRoot.partials.emplace("fallback", parsedPartial("root-fallback"));
    ownedPartialRoot.partials.emplace("inner", parsedPartial("root-inner"));
    ownedPartialRoot.partials.emplace(
        "leaf", std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "root-leaf"));
    ownedPartialRoot.partials.emplace("missing", std::unique_ptr<mustache::Node>());
    ownedPartialRoot.partials.emplace("nested", parsedPartial("{{>inner}}"));

    const std::vector<std::uint8_t> rootPartialArchive = mustache_benchmark::serializeCistaArchive(ownedPartialRoot);
    expect(mustache_benchmark::renderCistaArchive(
               std::string_view(reinterpret_cast<const char *>(rootPartialArchive.data()), rootPartialArchive.size()),
               data) == "root-card|root-fallback|root-inner|root-leaf|",
        "Cista archive did not render root-owned fallback partials");

    mustache::Node::Partials overridingPartials;
    overridingPartials.emplace("card", std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "external-card"));
    overridingPartials.emplace("fallback", std::unique_ptr<mustache::Node>());
    overridingPartials.emplace("inner", parsedPartial("external-inner"));
    const std::vector<std::uint8_t> overridingPartialArchive =
        mustache_benchmark::serializeCistaArchive(ownedPartialRoot, overridingPartials);
    expect(mustache_benchmark::renderCistaArchive(
               std::string_view(
                   reinterpret_cast<const char *>(overridingPartialArchive.data()), overridingPartialArchive.size()),
               data) == "external-card|root-fallback|external-inner|root-leaf|",
        "Cista archive partial precedence differs from the owned renderer");

    mustache::Node containerRoot;
    engine.tokenize("root", &containerRoot);
    containerRoot.child = std::make_unique<mustache::Node>();
    engine.tokenize("child", containerRoot.child.get());
    expectOperationRejected(
        [&containerRoot]() {
          (void)mustache_benchmark::serializeCistaArchive(containerRoot);
        },
        "container child ownership");

    for (std::size_t truncatedSize = 0; truncatedSize <= archivePreambleSize * 8; ++truncatedSize) {
      expectRejected(bytes.substr(0, truncatedSize), data, "empty, preamble-only, or truncated Cista graph header");
    }
    expectRejected(bytes.substr(0, bytes.size() - 1), data, "truncated");

    expectPreambleMutationRejected(archive, 0, data, "preamble magic");
    expectPreambleMutationRejected(archive, 8, data, "format generation");

    std::vector<std::uint8_t> corrupted = archive;
    corrupted[archivePreambleSize] ^= std::uint8_t{0xFF};
    expectRejected(std::string_view(reinterpret_cast<const char *>(corrupted.data()), corrupted.size()), data,
        "payload type hash");

    corrupted = archive;
    corrupted.back() ^= std::uint8_t{0xFF};
    expectRejected(
        std::string_view(reinterpret_cast<const char *>(corrupted.data()), corrupted.size()), data, "integrity");
    expectSingleByteMutationsRejected(archive, data, "preamble, type, integrity, and payload coverage");

    constexpr std::size_t cistaVersionFieldSize = 8;
    constexpr std::size_t archiveGraphOffset = archivePreambleSize + cistaVersionFieldSize;
    constexpr std::size_t archiveGraphSchemaOffset = archiveGraphOffset + 8;
    constexpr std::size_t archiveGraphSerializedSizeOffset = archiveGraphOffset + 16;
    constexpr std::size_t archiveGraphNodesPointerOffset = archiveGraphOffset + 24;
    testProtectedArchiveVectorValidation(modeArchives[2], data);

    std::vector<std::uint8_t> unprotectedMutation = modeArchives[0];
#if defined(_MSC_VER) && defined(_M_IX86)
    const std::size_t win32ArchiveGraphReservedMemberOffset =
        mustache_benchmark::detail::win32ArchiveGraphReservedMemberOffset();
    const std::size_t win32ArchiveGraphReservedOffset = archiveGraphOffset + win32ArchiveGraphReservedMemberOffset;
    for (std::size_t offset = 0; offset < sizeof(std::uint32_t); ++offset) {
      expect(unprotectedMutation.at(win32ArchiveGraphReservedOffset + offset) == 0,
          "Win32 archive graph reserved bytes were not initialized");
    }
    unprotectedMutation.at(win32ArchiveGraphReservedOffset) = 1;
    expectOperationRejected(
        [&unprotectedMutation, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(unprotectedMutation.data()), unprotectedMutation.size()),
              data, mustache_benchmark::CistaSecurityMode::Neither);
        },
        "Win32 graph reserved bytes without integrity");

    constexpr std::size_t protectedArchiveGraphOffset = archiveGraphOffset + 8;
    const std::size_t protectedWin32ArchiveGraphReservedOffset =
        protectedArchiveGraphOffset + win32ArchiveGraphReservedMemberOffset;
    for (std::size_t offset = 0; offset < sizeof(std::uint32_t); ++offset) {
      expect(modeArchives[2].at(protectedWin32ArchiveGraphReservedOffset + offset) == 0,
          "protected Win32 archive graph reserved bytes were not initialized");
    }
    expectProtectedMutationRejected(
        modeArchives[2], protectedArchiveGraphOffset, data,
        [protectedWin32ArchiveGraphReservedOffset](std::vector<std::uint8_t> * mutated) {
          mutated->at(protectedWin32ArchiveGraphReservedOffset) = 1;
        },
        "Win32 graph reserved bytes with repaired integrity");

    unprotectedMutation = modeArchives[0];
#endif
    unprotectedMutation.at(archiveGraphOffset) ^= std::uint8_t{0x80};
    expectOperationRejected(
        [&unprotectedMutation, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(unprotectedMutation.data()), unprotectedMutation.size()),
              data, mustache_benchmark::CistaSecurityMode::Neither);
        },
        "graph magic without integrity");

    unprotectedMutation = modeArchives[0];
    writeNativeArchiveField(&unprotectedMutation, archiveGraphSchemaOffset, std::uint32_t{2});
    expectOperationRejected(
        [&unprotectedMutation, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(unprotectedMutation.data()), unprotectedMutation.size()),
              data, mustache_benchmark::CistaSecurityMode::Neither);
        },
        "graph schema without integrity");

    unprotectedMutation = modeArchives[0];
    writeNativeArchiveField(&unprotectedMutation, archiveGraphSerializedSizeOffset, std::uint64_t{0});
    expectOperationRejected(
        [&unprotectedMutation, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(unprotectedMutation.data()), unprotectedMutation.size()),
              data, mustache_benchmark::CistaSecurityMode::Neither);
        },
        "graph serialized size without integrity");

    unprotectedMutation = modeArchives[0];
    unprotectedMutation.push_back(0);
    expectOperationRejected(
        [&unprotectedMutation, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(unprotectedMutation.data()), unprotectedMutation.size()),
              data, mustache_benchmark::CistaSecurityMode::Neither);
        },
        "trailing payload byte without integrity");

    unprotectedMutation = modeArchives[0];
    writeNativeArchiveField(&unprotectedMutation, archiveGraphNodesPointerOffset, std::intptr_t{-1});
    expectOperationRejected(
        [&unprotectedMutation, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(unprotectedMutation.data()), unprotectedMutation.size()),
              data, mustache_benchmark::CistaSecurityMode::DeepCheck);
        },
        "out-of-range graph pointer under deep checking");

    expectOperationRejected(
        [&modeArchives, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(modeArchives[0].data()), modeArchives[0].size()), data,
              mustache_benchmark::CistaSecurityMode::Integrity);
        },
        "unprotected payload read as integrity-protected");
    expectOperationRejected(
        [&modeArchives, &data]() {
          (void)mustache_benchmark::renderCistaArchive(
              std::string_view(reinterpret_cast<const char *>(modeArchives[2].data()), modeArchives[2].size()), data,
              mustache_benchmark::CistaSecurityMode::Neither);
        },
        "integrity-protected payload read as unprotected");

    std::vector<std::uint8_t> unaligned(archive.size() + 1);
    std::copy(archive.begin(), archive.end(), unaligned.begin() + 1);
    expectRejected(std::string_view(reinterpret_cast<const char *>(unaligned.data() + 1), archive.size()), data,
        "unaligned buffer");

    mustache_benchmark::CistaArchiveLimits archiveLimits;
    archiveLimits.maxInputBytes = archive.size() - 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "input byte limit");
    archiveLimits.maxInputBytes = archive.size();
    expect(mustache_benchmark::renderCistaArchive(bytes, data, archiveLimits) == expected,
        "archive at the exact input byte limit was rejected or rendered differently");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxNodes = 0;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "node limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxDataPartsPerNode = 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "per-node data-part limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxDataParts = 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "aggregate data-part limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxNestingDepth = 1;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "archive nesting limit");
    archiveLimits = mustache_benchmark::CistaArchiveLimits();
    archiveLimits.maxStringBytes = 0;
    expectRejected(bytes, data, archiveLimits, mustache::RenderLimits(), "archive string byte limit");

    mustache::RenderLimits renderLimits;
    renderLimits.maxOutputBytes = 0;
    expectRejected(bytes, data, mustache_benchmark::CistaArchiveLimits(), renderLimits, "output byte limit");
    renderLimits = mustache::RenderLimits();
    renderLimits.maxNodeVisits = 0;
    expectRejected(bytes, data, mustache_benchmark::CistaArchiveLimits(), renderLimits, "node visit limit");
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "cista archive test failed: %s\n", exception.what());
    return 1;
  }
  return 0;
}
