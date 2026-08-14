#include <mustache.hpp>

#include <string>

int main()
{
    const mustache::Data data = mustache::Data::string("subproject");
    const mustache::CompiledTemplate compiled = mustache::compile("{{.}}");
    return mustache_version()[0] != '\0' &&
            mustache::render(compiled, data) == "subproject"
        ? 0
        : 1;
}
