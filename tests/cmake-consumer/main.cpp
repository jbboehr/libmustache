#include <mustache/mustache.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef MUSTACHE_EXPECTED_VERSION
#define MUSTACHE_EXPECTED_VERSION "0.5.0"
#endif

static_assert(!std::is_copy_constructible<mustache::Data>::value,
    "mustache::Data must not be copy constructible");
static_assert(!std::is_copy_assignable<mustache::Data>::value,
    "mustache::Data must not be copy assignable");
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

int main()
{
    if (std::string(mustache_version()) != MUSTACHE_EXPECTED_VERSION) {
        return 1;
    }

    mustache::Node::SerializationLimits limits;
    mustache::Node root;
    root.type = mustache::Node::TypeRoot;
    mustache::Node movedRoot(std::move(root));
    std::unique_ptr<std::vector<uint8_t> > serial(movedRoot.serialize(limits));
    std::size_t position = 0;
    std::unique_ptr<mustache::Node> decoded(
        mustache::Node::unserialize(*serial, 0, &position, limits));
    return decoded->type == mustache::Node::TypeRoot &&
            position == serial->size()
        ? 0
        : 1;
}
