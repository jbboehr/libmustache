#include <mustache.hpp>

int main()
{
    return mustache_version()[0] == '\0' ? 1 : 0;
}
