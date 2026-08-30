#include "../src/archive/xxh3/xxh3.h"

#include "../src/archive/cista_include.hpp"
#include "../src/archive/cista_version.hpp"

int main()
{
  return cista::type_hash<mustache_benchmark::ArchiveGraph>() != mustache_benchmark::archiveRuntimeTypeVersion();
}
