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
  const std::string path = std::string(topSourceDirectory) + "/tests/fixtures/cista-archive-v1-x86_64-le-itanium.hex";
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
  expect(archive == readGoldenArchive(), "the public archive API changed the version 1 golden fixture");
  expect(static_cast<bool>(mustache::loadArchivedTemplate(archive)),
      "the public archive API did not load the golden fixture");
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

  mustache::ArchivedTemplateView archived;
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

  mustache::ArchivedTemplateView copy(archived);
  mustache::ArchivedTemplateView moved(std::move(archived));
  expect(archived.empty(), "a moved-from archived template was not empty");
  expect(copy && moved, "copying or moving lost the archived template");

  mustache::Data first = mustache::Data::object({{"name", mustache::Data::string("Ada")}});
  mustache::Data second = mustache::Data::object({{"name", mustache::Data::string("Grace")}});
  expect(mustache::render(copy, first) == "Hello Ada", "an archived template did not own copied input bytes");
  expect(engine.render(moved, second) == "Hello Grace", "an archived template was not reusable");

  mustache::ArchivedTemplateView temporaryBytes =
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
    (void)mustache::render(mustache::ArchivedTemplateView(), first);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "an empty archived template was rendered");

  mustache::ArchivedTemplateLimits archiveLimits;
  archiveLimits.maxInputBytes = serialized.size() - 1;
  rejected = false;
  try {
    (void)mustache::loadArchivedTemplate(serialized, archiveLimits);
  } catch (const mustache::Exception&) {
    rejected = true;
  }
  expect(rejected, "public archive loading ignored its input limit");

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
  const mustache::ArchivedTemplateView archived = mustache::loadArchivedTemplate(serialized);

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
  const mustache::ArchivedTemplateView archived = mustache::loadArchivedTemplate(serialized);

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
  limits.maxInputBytes = 0;
  expectSerializationRejected(limits, "archive serialization ignored its output byte limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNodes = 1;
  expectSerializationRejected(limits, "archive serialization ignored its node limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxStringBytes = 0;
  expectSerializationRejected(limits, "archive serialization ignored its string byte limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxNestingDepth = 1;
  expectSerializationRejected(limits, "archive serialization ignored its nesting limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxDataPartsPerNode = 1;
  expectSerializationRejected(limits, "archive serialization ignored its per-node data-part limit");

  limits = mustache::ArchivedTemplateLimits();
  limits.maxDataParts = 1;
  expectSerializationRejected(limits, "archive serialization ignored its aggregate data-part limit");
}

} // namespace

static_assert(
    std::is_copy_constructible<mustache::ArchivedTemplateView>::value, "archived templates must be copy constructible");
static_assert(
    std::is_copy_assignable<mustache::ArchivedTemplateView>::value, "archived templates must be copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::ArchivedTemplateView>::value,
    "archived templates must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::ArchivedTemplateView>::value,
    "archived templates must be nothrow move assignable");

int main()
{
  try {
    testPublicArchiveGolden();
    testOwnershipAndRendering();
    testCopiedInputCannotMutateValidatedState();
    testPartialsAndLambdas();
    testSerializationLimits();
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "archived-template API test failed: %s\n", exception.what());
    return 1;
  }
  return failures == 0 ? 0 : 1;
}
