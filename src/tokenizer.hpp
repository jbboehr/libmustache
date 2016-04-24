
#ifndef MUSTACHE_TOKENIZER_HPP
#define MUSTACHE_TOKENIZER_HPP

#include <iostream>
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <utility>

#include "exception.hpp"
#include "node.hpp"
#include "utils.hpp"

namespace mustache {


/*! \class Tokenizer
    \brief Tokenizes a template

    This class tokenizes a string template and returns a token tree.
*/
class Tokenizer {
  private:
    //! The default initial start string
    std::string _startSequence;
    
    //! The default initial stop string
    std::string _stopSequence;
    
    //! Whether to escape HTML by default
    bool _escapeByDefault;
    
  public:
    //! Constructor
    Tokenizer() : _startSequence("{{"), _stopSequence("}}"), _escapeByDefault(true) {};
    
    //! Sets the start sequence
    void setStartSequence(const std::string& start);
    
    //! Sets the start sequence
    void setStartSequence(const char * start, int len = 0);
    
    //! Sets the stop sequence
    void setStopSequence(const std::string& stop);
    
    //! Sets the stop sequence
    void setStopSequence(const char * stop, int len = 0);
    
    //! Sets whether to escape HTML by default
    void setEscapeByDefault(bool flag);
    
    //! Gets the current start sequence
    const std::string & getStartSequence();
    
    //! Gets the current stop sequence
    const std::string & getStopSequence();
    
    //! Gets whether to escape HTML by default
    bool getEscapeByDefault();
    
    //! Tokenizes the given string template
    void tokenize(std::string * tmpl, Node * root, bool escapeOutput = false);
};


} // namespace Mustache

#endif
