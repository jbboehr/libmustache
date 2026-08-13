#include <mustache/mustache.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifndef MUSTACHE_EXPECTED_VERSION
#define MUSTACHE_EXPECTED_VERSION "0.5.0"
#endif

int main()
{
    if (std::string(mustache_version()) != MUSTACHE_EXPECTED_VERSION) {
        return 1;
    }

    mustache::Node::SerializationLimits limits;
    mustache::Node root;
    root.type = mustache::Node::TypeRoot;
    std::unique_ptr<std::vector<uint8_t> > serial(root.serialize(limits));
    std::size_t position = 0;
    std::unique_ptr<mustache::Node> decoded(
        mustache::Node::unserialize(*serial, 0, &position, limits));
    return decoded->type == mustache::Node::TypeRoot &&
            position == serial->size()
        ? 0
        : 1;
}
