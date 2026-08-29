#include <mustache/mustache.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef MUSTACHE_EXPECTED_VERSION
#define MUSTACHE_EXPECTED_VERSION "0.6.0"
#endif

#if defined(_MSVC_LANG)
#if _MSVC_LANG < 201703L
#error "The installed mustache target must require C++17"
#endif
#elif __cplusplus < 201703L
#error "The installed mustache target must require C++17"
#endif

#if !defined(MUSTACHE_CXX_STANDARD) || MUSTACHE_CXX_STANDARD != 17
#error "mustache_config.h must advertise C++17"
#endif
#ifndef MUSTACHE_HAVE_CXX17
#error "mustache_config.h must advertise C++17 support"
#endif
#ifndef MUSTACHE_HAVE_CXX11
#error "The deprecated C++11 compatibility macro must remain defined"
#endif
#if defined(MUSTACHE_EXPECT_STATIC_DEFINE) && !defined(MUSTACHE_STATIC_DEFINE)
#error "The installed static target must define MUSTACHE_STATIC_DEFINE"
#endif

static_assert(std::is_copy_constructible<mustache::Data>::value, "mustache::Data must be safely copy constructible");
static_assert(std::is_copy_assignable<mustache::Data>::value, "mustache::Data must be safely copy assignable");
static_assert(
    std::is_nothrow_move_constructible<mustache::Data>::value, "mustache::Data must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::Data>::value, "mustache::Data must be nothrow move assignable");
static_assert(!std::is_copy_constructible<mustache::Node>::value, "mustache::Node must not be copy constructible");
static_assert(!std::is_copy_assignable<mustache::Node>::value, "mustache::Node must not be copy assignable");
static_assert(
    std::is_nothrow_move_constructible<mustache::Node>::value, "mustache::Node must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::Node>::value, "mustache::Node must be nothrow move assignable");
static_assert(std::is_same<mustache::Node::Children::value_type, std::unique_ptr<mustache::Node>>::value,
    "installed Node children must have explicit ownership");
static_assert(std::is_same<mustache::Node::Partials::mapped_type, std::unique_ptr<mustache::Node>>::value,
    "installed Node partials must have explicit ownership");
static_assert(std::is_copy_constructible<mustache::CompiledTemplate>::value,
    "installed CompiledTemplate must be copy constructible");
static_assert(std::is_nothrow_move_constructible<mustache::CompiledTemplate>::value,
    "installed CompiledTemplate must be nothrow move constructible");
static_assert(!std::is_copy_constructible<mustache::Renderer>::value,
    "installed Renderer must not copy borrowed operation state");
static_assert(!std::is_move_constructible<mustache::Renderer>::value,
    "installed Renderer must not move borrowed operation state");
static_assert(!std::is_copy_constructible<mustache::Mustache>::value,
    "installed Mustache must not copy its renderer's borrowed state");
static_assert(!std::is_move_constructible<mustache::Mustache>::value,
    "installed Mustache must not move its renderer's borrowed state");
static_assert(std::is_copy_constructible<mustache::LambdaRenderContext>::value,
    "installed lambda context must be safely retainable by value");
static_assert(std::is_nothrow_move_constructible<mustache::LambdaRenderContext>::value,
    "installed lambda context must be nothrow movable");

class ConsumerLambda final : public mustache::Lambda {
  public:
    std::string invoke() override
    {
      return "consumer-lambda";
    }
};

int main()
{
  if (std::string(mustache_version()) != MUSTACHE_EXPECTED_VERSION) {
    return 1;
  }

  const char templateSource[] = {'o', 'k'};
  mustache::Mustache mustache;
  mustache::Node parsed;
  mustache::Tokenizer::Limits parseLimits;
  mustache.tokenize(std::string_view(templateSource, sizeof(templateSource)), &parsed, parseLimits);

  mustache::Node::SerializationLimits limits;
  mustache::Node root;
  root.type = mustache::Node::TypeRoot;
  root.children.push_back(std::make_unique<mustache::Node>(mustache::Node::TypeOutput, "owned"));
  mustache::Node movedRoot(std::move(root));
  const std::vector<uint8_t> serial = movedRoot.serializeValue(limits);
  const char * serialData = serial.empty() ? "" : reinterpret_cast<const char *>(serial.data());
  std::unique_ptr<mustache::Node> decoded =
      mustache::Node::unserializeOwned(std::string_view(serialData, serial.size()), limits);
  mustache::Node::TemplateStringLimits templateLimits;
  templateLimits.maxOutputBytes = 5;
  templateLimits.maxNestingDepth = 2;
  templateLimits.maxNodes = 2;
  const std::string nodeTemplate = movedRoot.to_template_string("{{", "}}", templateLimits);

  mustache::Data::ParseLimits dataLimits;
#ifdef MUSTACHE_HAVE_LIBJSON
  const char jsonData[] = {'"', 'c', 'o', 'm', 'p', 'i', 'l', 'e', 'd', '"'};
  const mustache::Data scalar = mustache::Data::fromJSON(std::string_view(jsonData, sizeof(jsonData)), dataLimits);
#elif defined(MUSTACHE_HAVE_LIBYAML)
  const mustache::Data scalar = mustache::Data::fromYAML("compiled", dataLimits);
#else
  static_cast<void>(dataLimits);
  const mustache::Data scalar = mustache::Data::string("compiled");
#endif
  mustache::CompiledTemplate compiled = mustache::compile("[{{>value}}]");
  mustache::PartialMap partials;
  partials.emplace("value", mustache::compile("{{.}}"));
  mustache::RenderLimits renderLimits;
  renderLimits.maxOutputBytes = 10;
  const std::string compiledOutput = mustache::render(compiled, scalar, partials, renderLimits);
  const mustache::Data consumerLambda = mustache::Data::lambda(std::make_unique<ConsumerLambda>());
  const std::string lambdaOutput = mustache::render(mustache::compile("{{.}}"), consumerLambda);

#ifdef MUSTACHE_HAVE_ARCHIVED_TEMPLATES
  const std::vector<std::uint8_t> archiveBytes = mustache::serializeArchivedTemplate(compiled, partials);
  mustache::ArchivedTemplateLimits archiveLimits;
  const mustache::ArchivedTemplateView archived = mustache::loadArchivedTemplate(archiveBytes, archiveLimits);
  const std::string_view archiveByteView(reinterpret_cast<const char *>(archiveBytes.data()), archiveBytes.size());
  const mustache::ArchivedTemplateView archivedFromView =
      mustache::loadArchivedTemplate(archiveByteView, archiveLimits);
  mustache::ArchivedTemplateView archivedCopy(archived);
  mustache::ArchivedTemplateView archivedCopyAssigned;
  archivedCopyAssigned = archivedFromView;
  mustache::ArchivedTemplateView archivedMoved(std::move(archivedCopy));
  mustache::ArchivedTemplateView archivedMoveAssigned;
  archivedMoveAssigned = std::move(archivedCopyAssigned);
  const mustache::ArchivedTemplateView emptyArchive;
  const bool archivedHandlesValid = static_cast<bool>(archived) && !archived.empty() &&
      static_cast<bool>(archivedFromView) && !archivedFromView.empty() && static_cast<bool>(archivedMoved) &&
      static_cast<bool>(archivedMoveAssigned) && emptyArchive.empty() && !static_cast<bool>(emptyArchive);
  const std::string archivedOutput = mustache.render(archived, scalar);
  const std::string archivedLimitedOutput = mustache.render(archivedFromView, scalar, renderLimits);
  const std::string archivedFreeOutput = mustache::render(archivedMoved, scalar);
  const std::string archivedLimitedFreeOutput = mustache::render(archivedMoveAssigned, scalar, renderLimits);
  const std::string expectedArchivedOutput = "[compiled]";
#else
  const bool archivedHandlesValid = true;
  const std::string archivedOutput = "ok";
  const std::string archivedLimitedOutput = "ok";
  const std::string archivedFreeOutput = "ok";
  const std::string archivedLimitedFreeOutput = "ok";
  const std::string expectedArchivedOutput = "ok";
#endif

  mustache::LambdaRenderContext inactiveContext;
  bool inactiveContextRejected = false;
  try {
    static_cast<void>(inactiveContext.render(parsed));
  } catch (const mustache::Exception& exception) {
    inactiveContextRejected = std::string(exception.what()) == "Lambda render context is no longer active";
  }

  return decoded->type == mustache::Node::TypeRoot && decoded->children.size() == 1 &&
          decoded->children.front()->data.has_value() && *decoded->children.front()->data == "owned" &&
          nodeTemplate == "owned" && compiledOutput == "[compiled]" && lambdaOutput == "consumer-lambda" &&
          archivedHandlesValid && archivedOutput == expectedArchivedOutput &&
          archivedLimitedOutput == expectedArchivedOutput && archivedFreeOutput == expectedArchivedOutput &&
          archivedLimitedFreeOutput == expectedArchivedOutput && !inactiveContext.active() && inactiveContextRejected
      ? 0
      : 1;
}
