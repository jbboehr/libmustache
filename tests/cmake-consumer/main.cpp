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

static_assert(std::is_copy_constructible<mustache::Data>::value,
    "mustache::Data must be safely copy constructible");
static_assert(std::is_copy_assignable<mustache::Data>::value,
    "mustache::Data must be safely copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::Data>::value,
    "mustache::Data must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::Data>::value,
    "mustache::Data must be nothrow move assignable");
static_assert(!std::is_copy_constructible<mustache::Node>::value,
    "mustache::Node must not be copy constructible");
static_assert(!std::is_copy_assignable<mustache::Node>::value,
    "mustache::Node must not be copy assignable");
static_assert(std::is_nothrow_move_constructible<mustache::Node>::value,
    "mustache::Node must be nothrow move constructible");
static_assert(std::is_nothrow_move_assignable<mustache::Node>::value,
    "mustache::Node must be nothrow move assignable");
static_assert(std::is_same<mustache::Node::Children::value_type,
        std::unique_ptr<mustache::Node> >::value,
    "installed Node children must have explicit ownership");
static_assert(std::is_same<mustache::Node::Partials::mapped_type,
        std::unique_ptr<mustache::Node> >::value,
    "installed Node partials must have explicit ownership");
static_assert(std::is_copy_constructible<mustache::CompiledTemplate>::value,
    "installed CompiledTemplate must be copy constructible");
static_assert(
    std::is_nothrow_move_constructible<mustache::CompiledTemplate>::value,
    "installed CompiledTemplate must be nothrow move constructible");
static_assert(!std::is_copy_constructible<mustache::Renderer>::value,
    "installed Renderer must not copy borrowed operation state");
static_assert(!std::is_move_constructible<mustache::Renderer>::value,
    "installed Renderer must not move borrowed operation state");
static_assert(!std::is_copy_constructible<mustache::Mustache>::value,
    "installed Mustache must not copy its renderer's borrowed state");
static_assert(!std::is_move_constructible<mustache::Mustache>::value,
    "installed Mustache must not move its renderer's borrowed state");

int main()
{
    if (std::string(mustache_version()) != MUSTACHE_EXPECTED_VERSION) {
        return 1;
    }

    const char templateSource[] = {'o', 'k'};
    mustache::Mustache mustache;
    mustache::Node parsed;
    mustache::Tokenizer::Limits parseLimits;
    mustache.tokenize(
        std::string_view(templateSource, sizeof(templateSource)), &parsed,
        parseLimits);

    mustache::Node::SerializationLimits limits;
    mustache::Node root;
    root.type = mustache::Node::TypeRoot;
    root.children.push_back(std::make_unique<mustache::Node>(
        mustache::Node::TypeOutput, "owned"));
    mustache::Node movedRoot(std::move(root));
    std::unique_ptr<std::vector<uint8_t> > serial(movedRoot.serialize(limits));
    std::size_t position = 0;
    std::unique_ptr<mustache::Node> decoded(
        mustache::Node::unserialize(
            serial->data(), serial->size(), 0, &position, limits));

    mustache::Data::ParseLimits dataLimits;
    const char jsonData[] = {'"', 'c', 'o', 'm', 'p', 'i', 'l', 'e', 'd', '"'};
    const mustache::Data scalar = mustache::Data::fromJSON(
        std::string_view(jsonData, sizeof(jsonData)), dataLimits);
    mustache::CompiledTemplate compiled = mustache::compile("[{{>value}}]");
    mustache::PartialMap partials;
    partials.emplace("value", mustache::compile("{{.}}"));
    mustache::RenderLimits renderLimits;
    renderLimits.maxOutputBytes = 10;
    const std::string compiledOutput =
        mustache::render(compiled, scalar, partials, renderLimits);

    return decoded->type == mustache::Node::TypeRoot &&
            decoded->children.size() == 1 &&
            decoded->children.front()->data.has_value() &&
            *decoded->children.front()->data == "owned" &&
            position == serial->size() &&
            compiledOutput == "[compiled]"
        ? 0
        : 1;
}
