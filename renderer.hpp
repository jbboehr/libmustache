
#ifndef MUSTACHE_RENDERER_HPP
#define MUSTACHE_RENDERER_HPP

#include <map>
#include <string>

#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "utils.hpp"

namespace mustache {


class Renderer {
  private:
    Node * _node;
    Data * _data;
    Stack * _stack;
    Node::Partials * _partials;
    std::string * _output;
    
    void _renderNode(Node * node);
    
  public:
    static const int outputBufferLength = 100000;
    
    Renderer();
    ~Renderer();
    void clear();
    void init(Node * node, Data * data, Node::Partials * partials, std::string * output);
    void setNode(Node * node);
    void setData(Data * data);
    void setPartials(Node::Partials * partials);
    void setOutput(std::string * output);
    void render();
};


} // namespace Mustache

#endif
