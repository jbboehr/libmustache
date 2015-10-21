
#ifndef MUSTACHE_LAMBDA_HPP
#define MUSTACHE_LAMBDA_HPP

namespace mustache {

class Renderer;

/*! \class Lambda
    \brief A callable for use within mustache::Data.
*/
class Lambda {
  public:
    //! Destructor
    virtual ~Lambda() {};

    //! Invoke this lambda if it's being used as a variable
    virtual std::string invoke() = 0;

    //! Invoke this lambda if it's being used as a section
    virtual std::string invoke(std::string * text, Renderer * renderer) = 0;
};


} // namespace Mustache

#endif
