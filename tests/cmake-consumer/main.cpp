#include <mustache/mustache.hpp>

#include <string>

#ifndef MUSTACHE_EXPECTED_VERSION
#define MUSTACHE_EXPECTED_VERSION "0.5.0"
#endif

int main()
{
    return std::string(mustache_version()) == MUSTACHE_EXPECTED_VERSION ? 0 : 1;
}
