
#ifndef MUSTACHE_HPP
#define MUSTACHE_HPP

#include "mustache_config.h"

#include <stdint.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "tokenizer.hpp"
#include "utils.hpp"


extern "C" const char * mustache_version();
extern "C" int mustache_version_int();


namespace mustache {


/*! \class Mustache
    \brief Container class for a tokenizer and a renderer

    This class contains the tokenizer and renderer.
*/
class Mustache {
  private:
  public:
    //! Tokenizer
    Tokenizer tokenizer;
    
    //! Renderer
    Renderer renderer;
    
    //! Utility method for Tokenizer::tokenize()
    void tokenize(std::string * tmpl, Node * root);
    
    //! Utility method for Renderer::init() and Renderer::render()
    void render(Node * node, Data * data, Node::Partials * partials, std::string * output);
    
    //! Utility method for Tokenizer::setStartSequence()
    void setStartSequence(const std::string& start) {
      return tokenizer.setStartSequence(start);
    };
    
    //! Utility method for Tokenizer::setStartSequence()
    void setStartSequence(const char * start, long len = 0) {
      tokenizer.setStartSequence(start, len);
    };
    
    //! Utility method for Tokenizer::setStopSequence()
    void setStopSequence(const std::string& stop) {
      tokenizer.setStopSequence(stop);
    };
    
    //! Utility method for Tokenizer::setStopSequence()
    void setStopSequence(const char * stop, long len = 0) {
      tokenizer.setStopSequence(stop, len);
    };
    
    //! Utility method for Tokenizer::setEscapeByDefault()
    void setEscapeByDefault(bool flag) {
      tokenizer.setEscapeByDefault(flag);
    };
    
    //! Utility method for Tokenizer::getStartSequence()
    const std::string & getStartSequence() {
      return tokenizer.getStartSequence();
    }
    
    //! Utility method for Tokenizer::getStopSequence()
    const std::string & getStopSequence() {
      return tokenizer.getStopSequence();
    };
    
    //! Utility method for Tokenizer::getEscapeByDefault()
    bool getEscapeByDefault() {
      return tokenizer.getEscapeByDefault();
    };
};


} // namespace Mustache

#endif
