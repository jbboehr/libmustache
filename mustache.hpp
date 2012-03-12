
#ifndef MUSTACHE_HPP
#define MUSTACHE_HPP

#include <iostream>
#include <memory>
#include <string>

#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "renderer.hpp"
#include "tokenizer.hpp"
#include "utils.hpp"

namespace mustache {


class Mustache {
  private:
  public:
    typedef std::auto_ptr<Mustache> Ptr;
    
    Tokenizer tokenizer;
    Renderer renderer;
    
    void tokenize(std::string * tmpl, Node * root);
    void render(std::string * tmpl, Data * data, std::string * output);
    
    void setStartSequence(const std::string& start) {
      return tokenizer.setStartSequence(start);
    };
    void setStartSequence(const char * start) {
      tokenizer.setStartSequence(start);
    };
    void setStopSequence(const std::string& stop) {
      tokenizer.setStopSequence(stop);
    };
    void setStopSequence(const char * stop) {
      tokenizer.setStopSequence(stop);
    };
    void setEscapeByDefault(bool flag) {
      tokenizer.setEscapeByDefault(flag);
    };
    const std::string & getStartSequence() {
      return tokenizer.getStartSequence();
    }
    const std::string & getStopSequence() {
      return tokenizer.getStopSequence();
    };
    bool getEscapeByDefault() {
      return tokenizer.getEscapeByDefault();
    };
};


} // namespace Mustache

#endif
