
#ifndef MUSTACHE_EXCEPTION_HPP
#define MUSTACHE_EXCEPTION_HPP

#include <exception>
#include <stdexcept>
#include <string>

namespace mustache {


/*! \class Exception
    \brief Exception class

    Exceptions thrown will be of this class.
*/
class Exception : public std::runtime_error {
  public:
      Exception(const std::string& desc) : std::runtime_error(desc) { }
};


} // namespace Mustache

#endif


