#include <mustache/mustache.hpp>

#include <string>

int main()
{
    return std::string(mustache_version()) == "0.5.0" ? 0 : 1;
}
