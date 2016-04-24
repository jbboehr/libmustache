
#ifndef MUSTACHE_HPP
#define MUSTACHE_HPP

//#ifdef HAVE_CONFIG_H
#include "mustache_config.h"
//#endif

#include <stdint.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "compiler.hpp"
#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "tokenizer.hpp"
#include "utils.hpp"
#include "vm.hpp"


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
    
    //! Compiler
    Compiler compiler;
    
    //! VM
    VM vm;
    
    //! Utility method for Tokenizer::tokenize()
    void tokenize(std::string * tmpl, Node * root);
    
    //! Utility method for Renderer::init() and Renderer::render()
    void render(Node * node, Data * data, Node::Partials * partials, std::string * output);
    
    //! Compile a node
    void compile(Node * node, uint8_t ** codes, size_t * length);

    //! Compile a node (with partials)
    void compile(Node * node, Node::Partials * partials, uint8_t ** codes, size_t * length);
    
    //! Execute the VM
    void execute(uint8_t * codes, int length, Data * data, std::string * output);
    
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
