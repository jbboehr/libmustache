#include "mustache_config.h"

#if !defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
#error "test_archived_template requires archived-template support"
#endif

#include "archived_template.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "lambda.hpp"
#include "mustache.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

int failures = 0;

constexpr std::size_t archivePreambleSize = 24;
constexpr std::size_t archiveFormatGenerationOffset = 8;
constexpr std::size_t archiveCompatibilityFingerprintOffset = 16;

class FixedLambda final : public mustache::Lambda {
  public:
    explicit FixedLambda(std::string value) :
        value_(std::move(value))
    {}

    std::string invoke() override
    {
      return value_;
    }

  private:
    std::string value_;
};

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    ++failures;
  }
}

std::uint64_t readLittleEndian(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
  if (offset > bytes.size() || sizeof(std::uint64_t) > bytes.size() - offset) {
    throw std::runtime_error("archive preamble field is out of bounds");
  }
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8);
  }
  return value;
}

bool isGoldenPlatform(std::size_t pointerBytes) noexcept
{
#if defined(__x86_64__) && !defined(_MSC_VER)
  const std::uint16_t value = 1;
  return pointerBytes == 8 && *reinterpret_cast<const std::uint8_t *>(&value) == 1;
#else
  static_cast<void>(pointerBytes);
  return false;
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
  throw std::runtime_error("golden archive contains a non-hexadecimal byte");
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
    throw std::runtime_error("top_srcdir is required to locate the golden archive");
  }
  const std::string path = std::string(topSourceDirectory) + "/tests/fixtures/cista-archive-v2-x86_64-le-itanium.hex";
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("unable to open the golden archive");
  }
  std::vector<std::uint8_t> bytes;
  std::string encoded;
  while (stream >> encoded) {
    if (encoded.size() != 2) {
      throw std::runtime_error("golden archive contains a malformed byte");
    }
    bytes.push_back(static_cast<std::uint8_t>((hexNibble(encoded[0]) << 4) | hexNibble(encoded[1])));
  }
  if (!stream.eof() || bytes.empty()) {
    throw std::runtime_error("unable to read the golden archive");
  }
  return bytes;
}

void testPublicArchiveGolden()
{
  if (!isGoldenPlatform(sizeof(void *))) {
    return;
  }

  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{#products}}\n  {{> card}}\n{{/products}}\n", &root);

  mustache::Node card;
  engine.tokenize("<p class=\"{{category.slug}}\">{{name}}</p>{{^visible}}hidden{{/visible}}\n", &card);
  mustache::Node::Partials partials;
  partials.emplace("card", std::make_unique<mustache::Node>(std::move(card)));

  const std::vector<std::uint8_t> archive = mustache::serializeArchivedTemplate(root, partials);
  expect(archive == readGoldenArchive(), "the public archive API changed the version 2 golden fixture");
  expect(static_cast<bool>(mustache::loadArchivedTemplate(archive)),
      "the public archive API did not load the golden fixture");
}

void testArchiveCompatibilityPreamble()
{
  const mustache::CompiledTemplate compiled = mustache::compile("compatibility");
  const std::vector<std::uint8_t> archive = mustache::serializeArchivedTemplate(compiled);
  expect(archive.size() > archivePreambleSize, "the compatibility preamble has no archive payload");
  expect(readLittleEndian(archive, archiveFormatGenerationOffset) == 2,
      "the archived-template API did not select format generation 2");
  const std::uint64_t compatibilityFingerprint = readLittleEndian(archive, archiveCompatibilityFingerprintOffset);
  expect(compatibilityFingerprint != 0, "the archived-template compatibility fingerprint is empty");

  const std::string_view compatibilityTag = mustache::archivedTemplateCompatibilityTag();
  constexpr std::string_view compatibilityTagPrefix = "libmustache-cista-v2-";
  expect(compatibilityTag.size() == compatibilityTagPrefix.size() + 16,
      "the archived-template compatibility tag has an unexpected size");
  expect(compatibilityTag.substr(0, compatibilityTagPrefix.size()) == compatibilityTagPrefix,
      "the archived-template compatibility tag has an unexpected prefix");
  std::uint64_t taggedFingerprint = 0;
  if (compatibilityTag.size() == compatibilityTagPrefix.size() + 16) {
    for (const char digit : compatibilityTag.substr(compatibilityTagPrefix.size())) {
      taggedFingerprint = (taggedFingerprint << 4) | hexNibble(digit);
    }
  }
  expect(taggedFingerprint == compatibilityFingerprint,
      "the public compatibility tag does not identify the serialized archive representation");
  if (isGoldenPlatform(sizeof(void *))) {
    expect(compatibilityTag == "libmustache-cista-v2-cb437bcd4adcbe5d",
        "the x86-64 little-endian Itanium compatibility tag changed");
  }

  std::vector<std::uint8_t> previousGeneration = archive;
  previousGeneration[archiveFormatGenerationOffset] = 1;
  bool rejected = false;
  try {
    static_cast<void>(mustache::loadArchivedTemplate(previousGeneration));
  } catch (const mustache::ArchivedTemplateException& exception) {
    rejected = exception.reason() == mustache::ArchivedTemplateError::UnsupportedFormat &&
        std::string_view(exception.what()) == "Unsupported libmustache archive format generation";
  }
  expect(rejected, "an archive from a different format generation was not rejected at the preamble boundary");

  std::vector<std::uint8_t> incompatible = archive;
  incompatible[archiveCompatibilityFingerprintOffset] ^= std::uint8_t{1};
  rejected = false;
  try {
    static_cast<void>(mustache::loadArchivedTemplate(incompatible));
  } catch (const mustache::ArchivedTemplateException& exception) {
    rejected = exception.reason() == mustache::ArchivedTemplateError::UnsupportedFormat &&
        std::string_view(exception.what()) == "Unsupported libmustache archive compatibility";
  }
  expect(rejected, "an incompatible native archive was not rejected at the preamble boundary");
}

void testLoadingErrorCategories()
{
  const std::vector<std::uint8_t> serialized = mustache::serializeArchivedTemplate(mustache::compile("categorized"));

  std::vector<std::uint8_t> corrupted(serialized);
  corrupted.back() ^= std::uint8_t{0xFF};
  bool invalidArchive = false;
  try {
    static_cast<void>(mustache::loadArchivedTemplate(corrupted));
  } catch (const mustache::ArchivedTemplateException& exception) {
    invalidArchive = exception.reason() == mustache::ArchivedTemplateError::InvalidArchive;
  }
  expect(invalidArchive, "corrupt archive bytes did not report InvalidArchive");

  mustache::ArchivedTemplateLimits limits;
  limits.maxArchiveBytes = serialized.size() - 1;
  const std::string_view serializedView(reinterpret_cast<const char *>(serialized.data()), serialized.size());
  bool limitExceeded = false;
  try {
    static_cast<void>(mustache::loadArchivedTemplate(serializedView, limits));
  } catch (const mustache::ArchivedTemplateException& exception) {
    limitExceeded = exception.reason() == mustache::ArchivedTemplateError::LimitExceeded;
  }
  expect(limitExceeded, "an archive loading limit did not report LimitExceeded");

  bool caughtByLegacyBase = false;
  try {
    static_cast<void>(mustache::loadArchivedTemplate(corrupted));
  } catch (const mustache::Exception&) {
    caughtByLegacyBase = true;
  }
  expect(caughtByLegacyBase, "a categorized load error was not catchable as mustache::Exception");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxArchiveBytes = 0;
  bool writerUsedGenericError = false;
  try {
    static_cast<void>(mustache::serializeArchivedTemplate(mustache::compile("writer"), mustache::PartialMap(), limits));
  } catch (const mustache::ArchivedTemplateException&) {
    writerUsedGenericError = false;
  } catch (const mustache::Exception&) {
    writerUsedGenericError = true;
  }
  expect(writerUsedGenericError, "archive serialization unexpectedly used a loading error category");

  bool rendererUsedGenericError = false;
  try {
    static_cast<void>(mustache::render(mustache::ArchivedTemplate(), mustache::Data::object()));
  } catch (const mustache::ArchivedTemplateException&) {
    rendererUsedGenericError = false;
  } catch (const mustache::Exception&) {
    rendererUsedGenericError = true;
  }
  expect(rendererUsedGenericError, "archived-template rendering unexpectedly used a loading error category");
}

void testOwnershipAndRendering()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("Hello {{name}}", &root);
  std::vector<std::uint8_t> serialized;
  try {
    serialized = mustache::serializeArchivedTemplate(root);
  } catch (const std::exception& exception) {
    throw std::runtime_error(std::string("public serialization failed: ") + exception.what());
  }

  mustache::ArchivedTemplate archived;
  expect(archived.empty() && !archived, "a default archived template was not empty");
  {
    std::vector<std::uint8_t> unaligned(serialized.size() + 1);
    std::copy(serialized.begin(), serialized.end(), unaligned.begin() + 1);
    try {
      archived = mustache::loadArchivedTemplate(
          std::string_view(reinterpret_cast<const char *>(unaligned.data() + 1), serialized.size()));
    } catch (const std::exception& exception) {
      throw std::runtime_error(std::string("public loading failed: ") + exception.what());
    }
    std::fill(unaligned.begin(), unaligned.end(), std::uint8_t{0});
  }

  mustache::ArchivedTemplate copy(archived);
  mustache::ArchivedTemplate moved(std::move(archived));
  expect(archived.empty(), "a moved-from archived template was not empty");
  expect(copy && moved, "copying or moving lost the archived template");

  mustache::Data first = mustache::Data::object({{"name", mustache::Data::string("Ada")}});
  mustache::Data second = mustache::Data::object({{"name", mustache::Data::string("Grace")}});
  expect(mustache::render(copy, first) == "Hello Ada", "an archived template did not own copied input bytes");
  expect(engine.render(moved, second) == "Hello Grace", "an archived template was not reusable");

  mustache::ArchivedTemplate temporaryBytes =
      mustache::loadArchivedTemplate(std::vector<std::uint8_t>(serialized.begin(), serialized.end()));
  expect(mustache::render(temporaryBytes, first) == "Hello Ada", "copied archive bytes did not render");

  mustache::RenderLimits renderLimits;
  renderLimits.maxOutputBytes = 1;
  bool rejected = false;
  try {
    (void)engine.render(temporaryBytes, first, renderLimits);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "an archived-template render ignored its output limit");

  rejected = false;
  try {
    (void)mustache::render(mustache::ArchivedTemplate(), first);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "an empty archived template was rendered");

  std::vector<std::uint8_t> corrupted(serialized);
  corrupted.back() ^= std::uint8_t{0xFF};
  rejected = false;
  try {
    (void)mustache::loadArchivedTemplate(corrupted);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "corrupt public archive input was accepted");
}

void testCopiedInputCannotMutateValidatedState()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("alias-marker", &root);
  std::vector<std::uint8_t> serialized = mustache::serializeArchivedTemplate(root);
  const std::string marker = "alias-marker";
  const auto markerPosition = std::search(serialized.begin(), serialized.end(), marker.begin(), marker.end());
  expect(markerPosition != serialized.end(), "the archive did not contain the alias-mutation fixture string");
  if (markerPosition == serialized.end()) {
    return;
  }

  const std::size_t markerOffset = static_cast<std::size_t>(markerPosition - serialized.begin());
  std::uint8_t * const callerAlias = serialized.data();
  const mustache::ArchivedTemplate archived = mustache::loadArchivedTemplate(serialized);

  // Loading defensively copies the caller's bytes, so later caller mutations
  // must not change the already validated private archive.
  callerAlias[markerOffset] = std::uint8_t{'A'};
  expect(mustache::render(archived, mustache::Data::object()) == marker,
      "mutating caller-owned bytes changed a validated archived template");
}

void testPartialsAndLambdas()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{>card}}|{{call}}", &root);

  mustache::Node::Partials partials;
  std::unique_ptr<mustache::Node> card = std::make_unique<mustache::Node>();
  engine.tokenize("[{{name}}]", card.get());
  partials.emplace("card", std::move(card));

  const std::vector<std::uint8_t> serialized = mustache::serializeArchivedTemplate(root, partials);
  const mustache::ArchivedTemplate archived = mustache::loadArchivedTemplate(serialized);

  mustache::Data ownedData = mustache::Data::object();
  ownedData.set("name", mustache::Data::string("Ada"));
  ownedData.set("call", mustache::Data::lambda(std::make_unique<FixedLambda>("{{>card}}")));
  std::string expected;
  engine.render(&root, &ownedData, &partials, &expected);

  mustache::Data archivedData = mustache::Data::object();
  archivedData.set("name", mustache::Data::string("Ada"));
  archivedData.set("call", mustache::Data::lambda(std::make_unique<FixedLambda>("{{>card}}")));
  expect(expected == "[Ada]|[Ada]", "ordinary partial/lambda fixture changed unexpectedly");
  expect(mustache::render(archived, archivedData) == expected,
      "the public archived-template API changed partial or lambda rendering");
}

void testCompiledTemplateSerialization()
{
  const std::string inlineSource = "Hello {{name}}";
  const mustache::CompiledTemplate inlineCompiled = mustache::compile(inlineSource);
  mustache::Node inlineRoot;
  mustache::Mustache engine;
  engine.tokenize(inlineSource, &inlineRoot);
  const std::vector<std::uint8_t> inlineCompiledBytes = mustache::serializeArchivedTemplate(inlineCompiled);
  const std::vector<std::uint8_t> inlineNodeBytes = mustache::serializeArchivedTemplate(inlineRoot);
  expect(inlineCompiledBytes == inlineNodeBytes,
      "compiled and node serialization produced different archives for the same template");
  mustache::Data inlineData = mustache::Data::object();
  inlineData.set("name", mustache::Data::string("Ada"));
  expect(mustache::render(mustache::loadArchivedTemplate(inlineCompiledBytes), inlineData) == "Hello Ada",
      "the documented compile-serialize-render path changed behavior");

  const mustache::CompiledTemplate compiled = mustache::compile("[{{>value}}]");
  mustache::PartialMap partials;
  partials.emplace("value", mustache::compile("{{.}}"));

  const std::vector<std::uint8_t> serialized = mustache::serializeArchivedTemplate(compiled, partials);
  const mustache::ArchivedTemplate archived = mustache::loadArchivedTemplate(serialized);

  expect(mustache::render(archived, mustache::Data::string("compiled")) == "[compiled]",
      "compiled templates and partials did not compose with archived serialization");

  bool rejected = false;
  try {
    static_cast<void>(mustache::serializeArchivedTemplate(mustache::CompiledTemplate()));
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "an empty compiled template was serialized");

  mustache::PartialMap emptyPartials;
  emptyPartials.emplace("value", mustache::CompiledTemplate());
  rejected = false;
  try {
    static_cast<void>(mustache::serializeArchivedTemplate(compiled, emptyPartials));
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "an empty compiled partial was serialized");

  const auto expectLimitBeforePartialInspection = [](const mustache::CompiledTemplate& source,
                                                      mustache::ArchivedTemplateLimits limits,
                                                      const char * expectedMessage, const char * failureMessage) {
    mustache::PartialMap uninspectablePartials;
    uninspectablePartials.emplace("unused", mustache::CompiledTemplate());
    try {
      static_cast<void>(mustache::serializeArchivedTemplate(source, uninspectablePartials, limits));
      expect(false, failureMessage);
    } catch (const mustache::Exception& exception) {
      expect(std::string(exception.what()) == expectedMessage, failureMessage);
    }
  };

  mustache::ArchivedTemplateLimits limits;
  limits.maxArchiveBytes = 0;
  expectLimitBeforePartialInspection(compiled, limits, "Cista archive output byte limit exceeded",
      "the output byte limit was checked after compiled partial preprocessing");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNodes = 0;
  expectLimitBeforePartialInspection(compiled, limits, "Cista archive node count limit exceeded",
      "the node limit was checked after compiled partial preprocessing");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNodes = 2;
  expectLimitBeforePartialInspection(mustache::compile("x"), limits, "Cista archive node count limit exceeded",
      "the aggregate node limit was checked after compiled partial preprocessing");
}

void testSerializationLimits()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{user.name}}", &root);

  const auto expectSerializationRejected = [&root](mustache::ArchivedTemplateLimits limits, const char * message) {
    bool rejected = false;
    try {
      (void)mustache::serializeArchivedTemplate(root, mustache::Node::Partials(), limits);
    } catch (const mustache::Exception&) {
      rejected = true;
    }
    expect(rejected, message);
  };

  mustache::ArchivedTemplateLimits limits;
  limits.maxArchiveBytes = 0;
  expectSerializationRejected(limits, "archive serialization ignored its output byte limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNodes = 1;
  expectSerializationRejected(limits, "archive serialization ignored its node limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxTotalStringBytes = 0;
  expectSerializationRejected(limits, "archive serialization ignored its string byte limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNestingDepth = 1;
  expectSerializationRejected(limits, "archive serialization ignored its nesting limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxDataPartsPerNode = 1;
  expectSerializationRejected(limits, "archive serialization ignored its per-node data-part limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxTotalDataParts = 1;
  expectSerializationRejected(limits, "archive serialization ignored its aggregate data-part limit");
}

void testLoadingLimits()
{
  mustache::Mustache engine;
  mustache::Node root;
  engine.tokenize("{{user.name}}", &root);
  const std::vector<std::uint8_t> serialized = mustache::serializeArchivedTemplate(root);

  const auto expectLoadingRejected = [&serialized](mustache::ArchivedTemplateLimits limits, const char * message) {
    bool rejected = false;
    try {
      (void)mustache::loadArchivedTemplate(serialized, limits);
    } catch (const mustache::ArchivedTemplateException& exception) {
      rejected = exception.reason() == mustache::ArchivedTemplateError::LimitExceeded;
    }
    expect(rejected, message);
  };

  mustache::ArchivedTemplateLimits limits;
  limits.maxArchiveBytes = 0;
  expectLoadingRejected(limits, "archive loading ignored its archive byte limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNestingDepth = 0;
  expectLoadingRejected(limits, "archive loading ignored its nesting limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNodes = 0;
  expectLoadingRejected(limits, "archive loading ignored its node limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxTotalStringBytes = 0;
  expectLoadingRejected(limits, "archive loading ignored its aggregate string byte limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxDataPartsPerNode = 0;
  expectLoadingRejected(limits, "archive loading ignored its per-node data-part limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxTotalDataParts = 0;
  expectLoadingRejected(limits, "archive loading ignored its aggregate data-part limit");
}

void testArchiveLimitDefaults()
{
  const mustache::ArchivedTemplateLimits limits;
  expect(limits.maxArchiveBytes == 64 * 1024 * 1024, "default archive byte limit changed");
  expect(limits.maxNestingDepth == 64, "default archive nesting limit changed");
  expect(limits.maxNodes == 100000, "default archive node limit changed");
  expect(limits.maxTotalStringBytes == 64 * 1024 * 1024, "default aggregate archive string limit changed");
  expect(limits.maxDataPartsPerNode == 256, "default per-node archive data-part limit changed");
  expect(limits.maxTotalDataParts == 100000, "default aggregate archive data-part limit changed");
}

} // namespace

static_assert(
    std::is_copy_constructible<mustache::ArchivedTemplate>::value, "archived templates must be copy constructible");
static_assert(std::is_copy_assignable<mustache::ArchivedTemplate>::value, "archived templates must be copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::ArchivedTemplate>::value,
    "archived templates must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::ArchivedTemplate>::value,
    "archived templates must be nothrow move assignable");
static_assert(noexcept(mustache::archivedTemplateCompatibilityTag()),
    "the archived-template compatibility tag query must be non-throwing");
static_assert(!std::is_aggregate<mustache::ArchivedTemplateLimits>::value,
    "archived-template limits must reject positional aggregate initialization");
static_assert(std::is_base_of<mustache::Exception, mustache::ArchivedTemplateException>::value,
    "archived-template loading errors must remain catchable as mustache::Exception");
static_assert(noexcept(std::declval<const mustache::ArchivedTemplateException&>().reason()),
    "reading an archived-template error reason must be non-throwing");

int main()
{
  try {
    testPublicArchiveGolden();
    testArchiveCompatibilityPreamble();
    testLoadingErrorCategories();
    testOwnershipAndRendering();
    testCopiedInputCannotMutateValidatedState();
    testPartialsAndLambdas();
    testCompiledTemplateSerialization();
    testSerializationLimits();
    testLoadingLimits();
    testArchiveLimitDefaults();
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "archived-template API test failed: %s\n", exception.what());
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
