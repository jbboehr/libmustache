
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
      TypeTag = 3,
      TypeContainer = 4
    };
    
    //! Enum of token flags
    enum Flag { 
      FlagNone = 0,
      FlagEscape = 1,
      FlagNegate = 2,
      FlagSection = 4,
      FlagStop = 8,
      FlagComment = 16,
      FlagPartial = 32,
      FlagInlinePartial = 64,
      
      FlagHasChildren = Node::FlagNegate | Node::FlagSection | Node::FlagInlinePartial
    };
    
    //! The type from Node::Type
    Node::Type type;
    
    //! The flags
    int flags;
    
    //! The string value
    std::string * data;
    
    //! Child nodes
    Node::Children children;
    
    //! Child node. Should not be freed
    Node * child;
    
    //! Constructor
    Node() : data(NULL), type(Node::TypeNone), flags(Node::FlagNone), child(NULL) {};
    
    //! Destructor
    ~Node();
};


} // namespace Mustache

#endif


