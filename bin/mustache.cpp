#include "cli.hpp"

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char * argv[])
{
  // run() is noexcept; this boundary protects argv materialization itself.
  try {
    std::vector<std::string> arguments;
    if (argc > 1) {
      arguments.reserve(static_cast<std::size_t>(argc - 1));
    }
    for (int index = 1; index < argc; ++index) {
      arguments.emplace_back(argv[index]);
    }
    return mustache_cli::run(arguments, std::cout, std::cerr);
  } catch (const std::exception& exception) {
    try {
      std::cerr << "mustachec: " << exception.what() << '\n';
    } catch (...) {
    }
  } catch (...) {
    try {
      std::cerr << "mustachec: unknown failure\n";
    } catch (...) {
    }
  }
  return 1;
}
