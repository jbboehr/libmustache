
#ifndef MUSTACHE_EXCEPTION_HPP
#define MUSTACHE_EXCEPTION_HPP

#include "mustache_export.hpp"

#include <exception>
#include <stdexcept>
#include <string>

namespace mustache {

/*! \class Exception
    \brief Exception class

    Exceptions thrown will be of this class.
*/
#if defined(_MSC_VER)
// MSVC warns for every exported standard-library exception base even though
// exceptions are a supported DLL boundary. Export this public base so derived
// libmustache exceptions share RTTI with consumers, and scope the exemption to
// the one inheritance declaration that requires it.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif
class MUSTACHE_API Exception : public std::runtime_error {
  public:
    Exception(const std::string& desc) :
        std::runtime_error(desc)
    {}
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

/*! \class TokenizerException
    \brief Exception class

    Exceptions thrown in the tokenizer will be of this class.
*/
class TokenizerException : public Exception {
  public:
    const int lineNo;
    const int charNo;
    TokenizerException(const std::string& desc, int lineNo = 0, int charNo = 0) :
        Exception(desc),
        lineNo(lineNo),
        charNo(charNo) {};
};

} // namespace mustache

#endif
