
#ifndef MUSTACHE_RENDERER_HPP
#define MUSTACHE_RENDERER_HPP

#include <functional>
#include <iostream>
#include <map>
#include <string>

#include "data.hpp"
#include "exception.hpp"
#include "node.hpp"
#include "tokenizer.hpp"
#include "utils.hpp"

namespace mustache {

class Mustache;


/*! \class Renderer
    \brief Renders a token tree

    This class renders a token tree.
*/
class Renderer {
  private:
    typedef std::function<const Node *(const std::string&)> PartialResolver;

    //! The root token node
    const Node * _node;
    
    //! The root data node
    Data * _data;
    
    //! The data stack
    Stack<Data *> * _stack;
    
    //! Partials
    const Node::Partials * _partials;

    //! Optional lookup for opaque compiled partials
    PartialResolver _partialResolver;
    
    //! Current output buffer
    std::string * _output;
    
    //! Renders a single node
    void _renderNode(const Node * node);
    
    Data * _lookup(const Node * node);

    void setPartialResolver(PartialResolver resolver);

    friend class Mustache;
    
    bool _strictPaths;
    
  public:
    //! The default output buffer length
    static const int outputBufferLength = 1000;
    
    //! Constructor
    Renderer() : 
        _node(NULL), 
        _data(NULL), 
        _stack(NULL), 
        _partials(NULL), 
        _output(NULL), 
        _strictPaths(false) {};
    
    //! Destructor
    ~Renderer();
    
    //! Clears any assigned values
    void clear();
    
    //! Initializes the renderer
    void init(const Node * node, Data * data,
        const Node::Partials * partials, std::string * output);
    
    //! Sets the current root token node
    void setNode(const Node * node);
    
    //! Sets the current root data node
    void setData(Data * data);
    
    //! Sets the current partials
    void setPartials(const Node::Partials * partials);
    
    //! Sets the current output buffer
    void setOutput(std::string * output);
    
    //! Renders using the stored variables
    void render();

    //! Renders the given node to the given output using the stored variables
    void renderForLambda(const Node * node, std::string * output);
};


} // namespace Mustache

#endif
