#ifndef MUSTACHE_CLI_HPP
#define MUSTACHE_CLI_HPP

#include <iosfwd>
#include <string>
#include <vector>

namespace mustache_cli {

/*! Run the render-only command-line interface.

    Arguments exclude the executable name. All failures are converted to a
    diagnostic and a nonzero result so no exception escapes this boundary.
*/
int run(const std::vector<std::string>& arguments,
    std::ostream& output, std::ostream& error) noexcept;

} // namespace mustache_cli

#endif
