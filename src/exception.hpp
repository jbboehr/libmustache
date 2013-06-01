
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


} // namespace Mustache

#endif
