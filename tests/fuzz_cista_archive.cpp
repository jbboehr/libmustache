#include "mustache_config.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cista-archive.hpp"
#include "exception.hpp"
#include "mustache.hpp"

namespace {

constexpr std::size_t maxFuzzInputBytes = 4096;
constexpr std::size_t archivePreambleSize = 24;
constexpr std::size_t cistaVersionFieldSize = 8;
constexpr std::size_t cistaIntegrityFieldSize = 8;
constexpr std::size_t cistaIntegrityFieldOffset = archivePreambleSize + cistaVersionFieldSize;
constexpr std::size_t cistaGraphOffset = cistaIntegrityFieldOffset + cistaIntegrityFieldSize;
constexpr std::string_view hexSeedPrefix = "hex:";
constexpr std::string_view integrityRepairSeedPrefix = "corrupt-integrity:";

enum class SeedProtocol {
  Raw,
  IntegrityRepair,
};

class FixedLambda final : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "{{name}}";
    }
};

class AlignedInput {
  public:
    AlignedInput(const std::uint8_t * data, std::size_t size) :
        storage_((size + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t)),
        size_(size)
    {
      if (size_ != 0) {
        std::memcpy(storage_.data(), data, size_);
      }
    }

    std::uint8_t * data() noexcept
    {
      return reinterpret_cast<std::uint8_t *>(storage_.data());
    }

    std::string_view view() const noexcept
    {
      const char * data = size_ == 0 ? "" : reinterpret_cast<const char *>(storage_.data());
      return std::string_view(data, size_);
    }

    std::size_t size() const noexcept
    {
      return size_;
    }

  private:
    std::vector<std::max_align_t> storage_;
    std::size_t size_;
};

unsigned int hexDigit(std::uint8_t value)
{
  if (value >= '0' && value <= '9') {
    return static_cast<unsigned int>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<unsigned int>(value - 'a' + 10);
  }
  if (value >= 'A' && value <= 'F') {
    return static_cast<unsigned int>(value - 'A' + 10);
  }
  return 16;
}

bool decodeHexSeed(
    const std::uint8_t * data, std::size_t size, std::vector<std::uint8_t>& output, SeedProtocol * protocol)
{
  std::size_t offset = 0;
  SeedProtocol decodedProtocol = SeedProtocol::Raw;
  const char * inputData = size == 0 ? "" : reinterpret_cast<const char *>(data);
  const std::string_view input(inputData, size);
  if (input.size() >= hexSeedPrefix.size() && input.substr(0, hexSeedPrefix.size()) == hexSeedPrefix) {
    offset = hexSeedPrefix.size();
  } else if (input.size() >= integrityRepairSeedPrefix.size() &&
      input.substr(0, integrityRepairSeedPrefix.size()) == integrityRepairSeedPrefix) {
    offset = integrityRepairSeedPrefix.size();
    decodedProtocol = SeedProtocol::IntegrityRepair;
  } else {
    return false;
  }

  std::vector<std::uint8_t> decoded;
  unsigned int high = 16;
  for (std::size_t index = offset; index < size; ++index) {
    if (std::isspace(static_cast<unsigned char>(data[index])) != 0) {
      continue;
    }
    const unsigned int digit = hexDigit(data[index]);
    if (digit > 15) {
      return false;
    }
    if (high > 15) {
      high = digit;
    } else {
      decoded.push_back(static_cast<std::uint8_t>((high << 4) | digit));
      high = 16;
    }
  }
  if (high <= 15) {
    return false;
  }
  output.swap(decoded);
  *protocol = decodedProtocol;
  return true;
}

mustache::Data makeData()
{
  mustache::Data first = mustache::Data::object();
  first.set("name", mustache::Data::string("first"));
  mustache::Data second = mustache::Data::object();
  second.set("name", mustache::Data::string("second"));
  mustache::Data items = mustache::Data::array();
  items.push_back(std::move(first));
  items.push_back(std::move(second));

  mustache::Data nested = mustache::Data::object();
  nested.set("value", mustache::Data::string("nested"));

  mustache::Data data = mustache::Data::object();
  data.set("name", mustache::Data::string("<value>"));
  data.set("truthy", mustache::Data::boolean(true));
  data.set("falsey", mustache::Data::boolean(false));
  data.set("items", std::move(items));
  data.set("nested", std::move(nested));
  data.set("call", mustache::Data::lambda(std::make_unique<FixedLambda>()));
  return data;
}

mustache_benchmark::CistaArchiveLimits fuzzArchiveLimits(std::size_t maxArchiveBytes)
{
  mustache_benchmark::CistaArchiveLimits limits;
  limits.maxArchiveBytes = maxArchiveBytes;
  limits.maxNestingDepth = 64;
  limits.maxNodes = 1024;
  limits.maxTotalStringBytes = 16 * 1024;
  limits.maxDataPartsPerNode = 513;
  limits.maxTotalDataParts = 8192;
  return limits;
}

mustache::RenderLimits fuzzRenderLimits()
{
  mustache::RenderLimits limits;
  limits.maxOutputBytes = 64 * 1024;
  limits.maxNestingDepth = 64;
  limits.maxNodeVisits = 8192;
  limits.maxLambdaTemplateBytes = 16 * 1024;
  return limits;
}

bool validateArchive(std::string_view bytes)
{
  try {
    mustache_benchmark::validateCistaArchive(bytes, fuzzArchiveLimits(maxFuzzInputBytes));
  } catch (const mustache::Exception&) {
    return false;
  }
  return true;
}

void renderValidatedArchive(std::string_view bytes)
{
  if (!validateArchive(bytes)) {
    return;
  }
  mustache::Data data = makeData();
  static_cast<void>(
      mustache_benchmark::renderCistaArchive(bytes, data, fuzzArchiveLimits(maxFuzzInputBytes), fuzzRenderLimits()));
}

void writeLittleEndian(std::uint8_t * destination, std::uint64_t value)
{
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    destination[index] = static_cast<std::uint8_t>(value >> (index * 8));
  }
}

void rewriteProtectedIntegrity(AlignedInput& archive)
{
  if (archive.size() < cistaGraphOffset) {
    return;
  }
  const std::string_view bytes = archive.view();
  const std::uint64_t integrity = mustache_benchmark::checksumCistaArchive(
      bytes.substr(cistaGraphOffset), mustache_benchmark::CistaChecksumAlgorithm::Xxh3_64);
  writeLittleEndian(archive.data() + cistaIntegrityFieldOffset, integrity);
}

void exerciseIntegrityRepairProtocol(const std::uint8_t * data, std::size_t size)
{
  AlignedInput archive(data, size);
  if (!validateArchive(archive.view())) {
    return;
  }

  archive.data()[cistaIntegrityFieldOffset] ^= std::uint8_t{1};
  if (validateArchive(archive.view())) {
    std::abort();
  }

  rewriteProtectedIntegrity(archive);
  if (!validateArchive(archive.view())) {
    std::abort();
  }
  mustache::Data dataObject = makeData();
  static_cast<void>(mustache_benchmark::renderCistaArchive(
      archive.view(), dataObject, fuzzArchiveLimits(maxFuzzInputBytes), fuzzRenderLimits()));
}

void exerciseArchiveBytes(const std::uint8_t * data, std::size_t size)
{
  if (size > maxFuzzInputBytes) {
    return;
  }

  std::vector<std::uint8_t> decoded;
  SeedProtocol protocol = SeedProtocol::Raw;
  if (decodeHexSeed(data, size, decoded, &protocol)) {
    data = decoded.data();
    size = decoded.size();
  }
  if (protocol == SeedProtocol::IntegrityRepair) {
    exerciseIntegrityRepairProtocol(data, size);
    return;
  }

  AlignedInput raw(data, size);
  renderValidatedArchive(raw.view());

  AlignedInput integrityRepaired(data, size);
  rewriteProtectedIntegrity(integrityRepaired);
  renderValidatedArchive(integrityRepaired.view());
}

void exerciseTemplateRoundTrip(const std::uint8_t * data, std::size_t size)
{
  if (size > maxFuzzInputBytes) {
    return;
  }

  mustache::Tokenizer::Limits tokenizerLimits;
  tokenizerLimits.maxInputBytes = maxFuzzInputBytes;
  tokenizerLimits.maxNestingDepth = 32;
  tokenizerLimits.maxNodes = 512;
  tokenizerLimits.maxTagBytes = 512;
  tokenizerLimits.maxDelimiterBytes = 64;

  const char * sourceData = size == 0 ? "" : reinterpret_cast<const char *>(data);
  mustache::Mustache engine;
  mustache::Node root;
  try {
    engine.tokenize(std::string_view(sourceData, size), &root, tokenizerLimits);
  } catch (const mustache::Exception&) {
    return;
  }

  mustache::Node::Partials partials;
  std::unique_ptr<mustache::Node> partial = std::make_unique<mustache::Node>();
  engine.tokenize("[{{name}}]", partial.get(), tokenizerLimits);
  partials.emplace("partial", std::move(partial));

  const mustache::RenderLimits renderLimits = fuzzRenderLimits();
  bool ownedRejected = false;
  std::string ownedRejection;
  std::string ownedOutput;
  mustache::Data ownedData = makeData();
  try {
    engine.render(&root, &ownedData, &partials, &ownedOutput, renderLimits);
  } catch (const mustache::Exception& exception) {
    ownedRejected = true;
    ownedRejection = exception.what();
  }

  const mustache_benchmark::CistaArchiveLimits archiveLimits = fuzzArchiveLimits(128 * 1024);
  const std::vector<std::uint8_t> archive = mustache_benchmark::serializeCistaArchive(root, partials, archiveLimits);
  const std::string_view archiveBytes(reinterpret_cast<const char *>(archive.data()), archive.size());
  bool archiveRejected = false;
  std::string archiveRejection;
  std::string archiveOutput;
  mustache::Data archiveData = makeData();
  try {
    archiveOutput = mustache_benchmark::renderCistaArchive(archiveBytes, archiveData, archiveLimits, renderLimits);
  } catch (const mustache::Exception& exception) {
    archiveRejected = true;
    archiveRejection = exception.what();
  }

  if (archiveRejected != ownedRejected ||
      (archiveRejected ? archiveRejection != ownedRejection : archiveOutput != ownedOutput)) {
    std::abort();
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t * data, std::size_t size)
{
  exerciseArchiveBytes(data, size);
  exerciseTemplateRoundTrip(data, size);
  return 0;
}
