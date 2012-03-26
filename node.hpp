
#ifndef MUSTACHE_NODE_HPP
#define MUSTACHE_NODE_HPP

#include <map>
#include <memory>
#include <stack>
#include <string>
#include <vector>

namespace mustache {


/*! \class Node
    \brief Represents a token

    This class represents a token.
*/
class Node {
  public:
    typedef std::auto_ptr<Node> Ptr;
    typedef std::vector<Node *> Children;
    typedef std::stack<Node *> Stack;
    typedef std::map<std::string,std::string> RawPartials;
    typedef std::map<std::string,Node> Partials;
    typedef std::pair<std::string,Node> PartialPair;
    
    //! Enum of token types
    enum Type {
      TypeNone = 0,
      TypeRoot = 1,
      TypeOutput = 2,
      TypeTag = 4,
      TypeContainer = 8,
      
      // These are extensions of the tag type
      TypeVariable = 16,
      TypeNegate = 32,
      TypeSection = 64,
      TypeStop = 128,
      TypeComment = 256,
      TypePartial = 512,
      TypeInlinePartial = 1024,
      
      // If the type allows children
      TypeHasChildren = Node::TypeNegate | Node::TypeSection | Node::TypeInlinePartial,
      
      // If the type pushes data to the stack
      TypeHasData = Node::TypeVariable | Node::TypeNegate | Node::TypeSection | Node::TypePartial,
      
      // If the type does not have a string
      TypeHasNoString = Node::TypeNone | Node::TypeRoot | Node::TypeContainer,
      
      // If the type can use dot notation
      TypeHasDot = Node::TypeNegate | Node::TypeSection | Node::TypeStop | Node::TypeTag | Node::TypeVariable
    };
    
    //! Enum of token flags
    enum Flag { 
      FlagNone = 0,
      FlagEscape = 1
    };
    
    //! The type from Node::Type
    Node::Type type;
    
    //! The flags
    int flags;
    
    //! The string value
    std::string * data;
    
    //! The string parts for dot notation
    std::vector<std::string> * dataParts;
    
    //! Child nodes
    Node::Children children;
    
    //! Child node. Should not be freed
    Node * child;
    
    //! Internal partials
    Node::Partials partials;
    
    //! Constructor
    Node() : 
        type(Node::TypeNone),
        data(NULL),
        dataParts(NULL),
        flags(Node::FlagNone), 
        child(NULL) {};
    Node(Node::Type type, const std::string& data, int flags = 0) :
        type(type),
        dataParts(NULL), 
        flags(flags), 
        child(NULL) {
      setData(data);
    };
        
    
    //! Destructor
    ~Node();
    
    //! Set data
    void setData(const std::string& data);
};


} // namespace Mustache

#endif


