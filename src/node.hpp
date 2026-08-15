
#ifndef MUSTACHE_NODE_HPP
#define MUSTACHE_NODE_HPP

#include "mustache_export.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <stdint.h>

namespace mustache {


/*! \class Node
    \brief Represents a token

    This class represents a token.
*/
class Node {
  public:
    typedef std::vector<std::unique_ptr<Node> > Children;
    typedef std::map<std::string,std::string> RawPartials;
    typedef std::map<std::string,std::unique_ptr<Node> > Partials;
    typedef Partials::value_type PartialPair;

    /*! \struct SerializationLimits
        \brief Resource limits for serialized AST encoding and decoding.

        Every field is an enforced maximum. A zero value therefore rejects
        any input that consumes that resource; zero never means unlimited.
    */
    struct SerializationLimits {
      std::size_t maxInputBytes;
      std::size_t maxOutputBytes;
      std::size_t maxNestingDepth;
      std::size_t maxNodes;
      std::size_t maxDataPartsPerNode;
      std::size_t maxDataParts;

      MUSTACHE_API SerializationLimits();
    };

    /*! \struct TemplateStringLimits
        \brief Resource limits for reconstructing template source.

        The receiver is the first visited node and occupies the first nesting
        level, including for children_to_template_string(). Every field is an
        enforced maximum; zero never means unlimited.
    */
    struct TemplateStringLimits {
      std::size_t maxOutputBytes;
      std::size_t maxNestingDepth;
      std::size_t maxNodes;

      MUSTACHE_API TemplateStringLimits();
    };
    
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
      TypeHasChildren = Node::TypeRoot | Node::TypeNegate | Node::TypeSection | Node::TypeInlinePartial,
      
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
      FlagEscape = 1,

      // Internal output retained only when reconstructing section text for
      // lambda callbacks. Normal rendering suppresses these nodes.
      FlagLambdaOnly = 2,

      // Internal source metadata immediately preceding a standalone partial.
      // The output node's data contains the partial's local indentation.
      FlagPartialIndent = 4
    };
    
    //! The type from Node::Type
    Node::Type type;
    
    //! The flags
    int flags;
    
    //! The string value
    std::optional<std::string> data;
    
    //! The string parts for dot notation
    std::vector<std::string> dataParts;
    
    //! Child nodes
    Node::Children children;
    
    //! Owned child node for compatibility with TypeContainer
    std::unique_ptr<Node> child;
    
    //! Internal partials
    Node::Partials partials;

    //! The start sequence value for this node's children (used when rendering lambdas as sections, so only needed if this node is a section)
    std::optional<std::string> startSequence;

    //! The stop sequence value for this node's children (used when rendering lambdas as sections, so only needed if this node is a section)
    std::optional<std::string> stopSequence;
    
    //! Constructor
    Node() : 
        type(Node::TypeNone),
        flags(Node::FlagNone) {}
    Node(Node::Type type, const std::string& data, int flags = 0) :
        type(type),
        flags(flags) {
      setData(data);
    }

    // Node owns its AST storage and must never be shallow-copied.
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    MUSTACHE_API Node(Node&& other) noexcept;
    MUSTACHE_API Node& operator=(Node&& other) noexcept;
        
    
    //! Destructor
    MUSTACHE_API ~Node();

    /*! Reconstruct child template source using compatibility defaults. */
    MUSTACHE_API std::string children_to_template_string(
        const std::string& start, const std::string& stop) const;

    //! Reconstruct child template source with explicit resource limits.
    MUSTACHE_API std::string children_to_template_string(
        const std::string& start, const std::string& stop,
        const TemplateStringLimits& limits) const;
    
    //! Set data
    MUSTACHE_API void setData(const std::string& value);
    
    /*! Legacy owning-pointer serializer. Prefer serializeValue(). */
    MUSTACHE_API std::vector<uint8_t> * serialize() const;

    /*! Legacy owning-pointer serializer. Prefer serializeValue(). */
    MUSTACHE_API std::vector<uint8_t> * serialize(
        const SerializationLimits& limits) const;

    //! Serialize into an owned value.
    MUSTACHE_API std::vector<uint8_t> serializeValue() const;

    //! Serialize into an owned value with explicit resource limits.
    MUSTACHE_API std::vector<uint8_t> serializeValue(
        const SerializationLimits& limits) const;

    /*! Reconstruct this node using compatibility defaults. */
    MUSTACHE_API std::string to_template_string(
        const std::string& start, const std::string& stop) const;

    //! Reconstruct this node with explicit resource limits.
    MUSTACHE_API std::string to_template_string(
        const std::string& start, const std::string& stop,
        const TemplateStringLimits& limits) const;

    //! Decode a complete serialized AST into an RAII owner.
    static MUSTACHE_API std::unique_ptr<Node> unserializeOwned(
        std::string_view serial);

    //! Decode a complete serialized AST with explicit resource limits.
    static MUSTACHE_API std::unique_ptr<Node> unserializeOwned(
        std::string_view serial, const SerializationLimits& limits);
    
    /*! Legacy owning-pointer decoder. Prefer unserializeOwned(). */
    static MUSTACHE_API Node * unserialize(
        std::vector<uint8_t> & serial, size_t offset, size_t * vpos);

    /*! Legacy owning-pointer decoder. Prefer unserializeOwned(). */
    static MUSTACHE_API Node * unserialize(
        std::vector<uint8_t> & serial, size_t offset, size_t * vpos,
        const SerializationLimits& limits);

    /*! Legacy owning-pointer decoder. Prefer unserializeOwned(). */
    static MUSTACHE_API Node * unserialize(const uint8_t * serial,
        size_t length,
        size_t offset, size_t * vpos);

    /*! Legacy owning-pointer decoder. Prefer unserializeOwned(). */
    static MUSTACHE_API Node * unserialize(const uint8_t * serial,
        size_t length,
        size_t offset, size_t * vpos, const SerializationLimits& limits);

    /*! Legacy owning-pointer decoder. Prefer unserializeOwned(). */
    static MUSTACHE_API Node * unserialize(
        std::string_view serial, size_t offset,
        size_t * vpos);

    /*! Legacy owning-pointer decoder. Prefer unserializeOwned(). */
    static MUSTACHE_API Node * unserialize(
        std::string_view serial, size_t offset,
        size_t * vpos, const SerializationLimits& limits);

  private:
    void resetMovedFrom() noexcept;
};

} // namespace Mustache

#endif
