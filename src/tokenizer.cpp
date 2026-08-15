
#include "tokenizer.hpp"

#include <vector>

namespace mustache {

namespace {

void validateDelimiter(std::string_view delimiter, const char * name)
{
  if( delimiter.empty() ) {
    throw Exception(std::string("Empty ") + name + " delimiter");
  }
}

bool matchesAt(
    std::string_view input, size_t position, std::string_view sequence)
{
  return position <= input.size() &&
      sequence.size() <= input.size() - position &&
      input.compare(position, sequence.size(), sequence) == 0;
}

bool isStandaloneIndent(char value)
{
  return value == ' ' || value == '\t';
}

bool standaloneLineEnd(
    std::string_view input, size_t tagEnd, size_t * lineEnd)
{
  size_t position = tagEnd;
  while( position < input.size() && isStandaloneIndent(input[position]) ) {
    ++position;
  }
  if( position < input.size() ) {
    if( input[position] == '\n' ) {
      ++position;
    } else if( input[position] == '\r' &&
        position + 1 < input.size() && input[position + 1] == '\n' ) {
      position += 2;
    } else {
      return false;
    }
  }

  *lineEnd = position;
  return true;
}

bool isStandaloneType(Node::Type type)
{
  return type == Node::TypeSection || type == Node::TypeNegate ||
      type == Node::TypeStop || type == Node::TypeComment;
}

struct ParseFrame {
  ParseFrame(Node * node, int lineNo, int charNo) :
      node(node), lineNo(lineNo), charNo(charNo) {}

  Node * node;
  int lineNo;
  int charNo;
};

Node * appendNode(Node * parent, std::unique_ptr<Node> node,
    std::size_t * nodeCount, const Tokenizer::Limits& limits,
    int lineNo, int charNo)
{
  if( *nodeCount >= limits.maxNodes ) {
    throw TokenizerException(
        "Template node count limit exceeded", lineNo, charNo);
  }
  parent->children.push_back(std::move(node));
  ++*nodeCount;
  return parent->children.back().get();
}

void appendLambdaOnlyOutput(Node * parent, std::string_view output,
    std::size_t * nodeCount, const Tokenizer::Limits& limits,
    int lineNo, int charNo)
{
  if( output.empty() ) {
    return;
  }
  appendNode(parent, std::make_unique<Node>(
      Node::TypeOutput, std::string(output), Node::FlagLambdaOnly),
      nodeCount, limits, lineNo, charNo);
}

} // namespace

Tokenizer::Limits::Limits() :
    maxInputBytes(64 * 1024 * 1024),
    // Default serialization permits 64 nodes along a root-to-leaf path.
    // A parsed path includes the root and its innermost closing node.
    maxNestingDepth(62),
    maxNodes(100000),
    maxTagBytes(1024 * 1024),
    maxDelimiterBytes(1024)
{
}


void Tokenizer::setStartSequence(const std::string& start) {
  setStartSequence(std::string_view(start));
}

void Tokenizer::setStartSequence(std::string_view start) {
  validateDelimiter(start, "start");
  _startSequence.assign(start.data(), start.size());
}

void Tokenizer::setStartSequence(const char * start, int len) {
  if( start == NULL ) {
    throw Exception("Missing start delimiter");
  }
  if( len < 0 ) {
    throw Exception("Invalid start delimiter length");
  }
  setStartSequence(len == 0
      ? std::string_view(start)
      : std::string_view(start, static_cast<size_t>(len)));
}

void Tokenizer::setStopSequence(const std::string& stop) {
  setStopSequence(std::string_view(stop));
}

void Tokenizer::setStopSequence(std::string_view stop) {
  validateDelimiter(stop, "stop");
  _stopSequence.assign(stop.data(), stop.size());
}

void Tokenizer::setStopSequence(const char * stop, int len) {
  if( stop == NULL ) {
    throw Exception("Missing stop delimiter");
  }
  if( len < 0 ) {
    throw Exception("Invalid stop delimiter length");
  }
  setStopSequence(len == 0
      ? std::string_view(stop)
      : std::string_view(stop, static_cast<size_t>(len)));
}

void Tokenizer::setEscapeByDefault(bool flag) {
  _escapeByDefault = flag;
}

const std::string& Tokenizer::getStartSequence() {
  return _startSequence;
}

const std::string& Tokenizer::getStopSequence() {
  return _stopSequence;
}

bool Tokenizer::getEscapeByDefault() {
  return _escapeByDefault;
}

void Tokenizer::tokenize(std::string * tmpl, Node * root, bool escapeOutput)
{
  if( tmpl == NULL ) {
    throw Exception("Missing template");
  }
  tokenize(std::string_view(*tmpl), root, Limits(), escapeOutput);
}

void Tokenizer::tokenize(std::string * tmpl, Node * root,
    const Limits& limits, bool escapeOutput)
{
  if( tmpl == NULL ) {
    throw Exception("Missing template");
  }
  tokenize(std::string_view(*tmpl), root, limits, escapeOutput);
}

void Tokenizer::tokenize(
    std::string_view tmpl, Node * root, bool escapeOutput)
{
  tokenize(tmpl, root, Limits(), escapeOutput);
}

void Tokenizer::tokenize(std::string_view tmpl, Node * root,
    const Limits& limits, bool escapeOutput)
{
  if( root == NULL ) {
    throw Exception("Missing token root");
  }
  if( tmpl.size() > limits.maxInputBytes ) {
    throw TokenizerException("Template input size limit exceeded");
  }
  if( _startSequence.size() > limits.maxDelimiterBytes ||
      _stopSequence.size() > limits.maxDelimiterBytes ) {
    throw TokenizerException("Template delimiter size limit exceeded");
  }
  std::string stop(_stopSequence);
  std::string start(_startSequence);
  std::string buffer;
  buffer.reserve(100); // Reserver 100 chars
  std::string outputBeforeTag;
  
  const size_t tmplL = tmpl.length();
  
  char startC = start.at(0);
  size_t startL = start.length();
  
  char stopC = stop.at(0);
  size_t stopL = stop.length();
  size_t tmpStopL = stopL;
  
  size_t pos = 0;
  size_t skipUntil = std::string_view::npos;
  int lineNo = 1;
  int charNo = 0;
  size_t lineStart = 0;
  bool linePrefixIsIndent = true;
  
  int inTag = 0;
  int inTripleTag = 0;
  int skip = 0;
  int startCharNo = 0;
  int startLineNo = 0;
  size_t tagStart = 0;
  size_t tagIndentationBytes = 0;
  bool tagPrefixIsIndent = false;
  Node::Type currentType = Node::TypeNone;
  int currentFlags = Node::FlagNone;
  
  Node parsedRoot;
  std::vector<ParseFrame> nodeStack;
  std::size_t nodeCount = 0;
  Node * node;
  
  // Initialize root node and stack[0]
  parsedRoot.type = Node::TypeRoot;
  parsedRoot.flags = Node::FlagNone;
  
  nodeStack.push_back(ParseFrame(&parsedRoot, 0, 0));
  
  // Scan loop
  for( pos = 0; pos < tmplL; pos++ ) {

    // Maintain the state immediately before tmpl[pos]. This makes standalone
    // prefix classification linear even for many eligible tags on one line.
    if( pos > 0 ) {
      const char previous = tmpl[pos - 1];
      if( previous == '\n' ) {
        lineStart = pos;
        linePrefixIsIndent = true;
      } else if( !isStandaloneIndent(previous) ) {
        linePrefixIsIndent = false;
      }
    }
    
    // Track line numbers
    if( tmpl[pos] == '\n' ) {
      lineNo++;
      charNo = 0;
    } else {
      charNo++;
    }
      
    // Skip until position
    if( skipUntil != std::string_view::npos ) {
      if( pos <= skipUntil ) {
        continue;
      } else {
        skipUntil = std::string_view::npos;
      }
    }
    
    // Main
    if( !inTag ) {
      if( tmpl[pos] == startC && matchesAt(tmpl, pos, start) ) {
        // Delay publishing the preceding output until the tag is classified.
        // A standalone tag removes indentation from that output.
        outputBeforeTag.swap(buffer);
        // Open new buffer
        inTag = true;
        skipUntil = pos + startL - 1;
        tagStart = pos;
        tagPrefixIsIndent = linePrefixIsIndent;
        tagIndentationBytes = pos - lineStart;
        startLineNo = lineNo;
        startCharNo = charNo; // Could be inaccurate
        // Triple mustache
        if( start.compare("{{") == 0 && matchesAt(tmpl, pos + 2, "{") ) {
          inTripleTag = true;
          skipUntil++;
        }
      }
    } else {
      if( tmpl[pos] == stopC && matchesAt(tmpl, pos, stop) ) {
        // Trim buffer
        trim(buffer);
        if( buffer.length() <= 0 ) {
          std::ostringstream oss;
          oss << "Empty tag"
              << " at "
              << startLineNo << ":" << startCharNo;
          throw TokenizerException(oss.str(), startLineNo, startCharNo);
        }
        // Close and process previous buffer
        skip = false;
        tmpStopL = stopL;
        currentType = Node::TypeVariable;
        currentFlags = Node::FlagNone;
        switch( buffer[0] ) {
          case '&':
            currentFlags = currentFlags ^ Node::FlagEscape;
            break;
          case '^':
            currentType = Node::TypeNegate;
            break;
          case '#':
            currentType = Node::TypeSection;
            break;
          case '/':
            currentType = Node::TypeStop;
            break;
          case '!':
            currentType = Node::TypeComment;
            break;
          case '>':
            currentType = Node::TypePartial;
            break;
          case '=':
            if( buffer.at(buffer.length()-1) != '=' ) {
              std::ostringstream oss;
              oss << "Missing closing delimiter (=)"
                  << " in tag starting at "
                  << startLineNo << ":" << startCharNo;
              throw TokenizerException(oss.str(), startLineNo, startCharNo);
            }
            trim(buffer, " \f\n\r\t\v=");
            
            std::vector<std::string> delims;
            stringTok(buffer, whiteSpaces, &delims);
            if( delims.size() != 2 || delims[0].size() < 1 || delims[1].size() < 1 ) {
              std::ostringstream oss;
              oss << "Invalid delimiter format"
                  << " in tag starting at "
                  << startLineNo << ":" << startCharNo;
              throw TokenizerException(oss.str(), startLineNo, startCharNo);
            }
            if( delims[0].size() > limits.maxDelimiterBytes ||
                delims[1].size() > limits.maxDelimiterBytes ) {
              throw TokenizerException(
                  "Template delimiter size limit exceeded",
                  startLineNo, startCharNo);
            }
            
            // Assign new start/stop
            start.assign(delims.at(0));
            startC = start.at(0);
            startL = start.length();
            stop.assign(delims.at(1));
            stopC = stop.at(0);
            stopL = stop.length();
            skip = 1;
            break;
        }

        size_t standaloneEnd = std::string_view::npos;
        const bool standaloneCandidate = !inTripleTag &&
            (skip || isStandaloneType(currentType));
        if( standaloneCandidate && tagPrefixIsIndent &&
            standaloneLineEnd(tmpl, pos + tmpStopL, &standaloneEnd) ) {
          if( tagIndentationBytes > outputBeforeTag.size() ) {
            throw TokenizerException(
                "Invalid standalone parser state",
                startLineNo, startCharNo);
          }
          outputBeforeTag.resize(
              outputBeforeTag.size() - tagIndentationBytes);
        }

        if( !outputBeforeTag.empty() ) {
          appendNode(nodeStack.back().node, std::make_unique<Node>(
              Node::TypeOutput, outputBeforeTag), &nodeCount, limits,
              startLineNo, startCharNo);
        }
        outputBeforeTag.clear();

        if( standaloneEnd != std::string_view::npos ) {
          appendLambdaOnlyOutput(nodeStack.back().node,
              tmpl.substr(tagStart - tagIndentationBytes,
                  tagIndentationBytes),
              &nodeCount, limits, startLineNo, startCharNo);
        }

        if( !skip ) {
          if( currentType != Node::TypeVariable || currentFlags != 0 ) {
            buffer.erase(0, 1);
            trim(buffer);
          }
          if( currentType == Node::TypeVariable || currentType == Node::TypeTag ) {
            if( inTripleTag ) {
              currentFlags = currentFlags ^ Node::FlagEscape;
            }
            if( _escapeByDefault ) {
              currentFlags = currentFlags ^ Node::FlagEscape;
            }
          }

          if( currentType & Node::TypeHasChildren ) {
            const std::size_t currentDepth = nodeStack.size() - 1;
            if( currentDepth >= limits.maxNestingDepth ) {
              throw TokenizerException(
                  "Template nesting limit exceeded",
                  startLineNo, startCharNo);
            }
          } else if( currentType == Node::TypeStop ) {
            if( nodeStack.size() <= 1 ) {
              std::ostringstream oss;
              oss << "Extra closing section '" << buffer << "' at "
                  << startLineNo << ":" << startCharNo;
              throw TokenizerException(
                  oss.str(), startLineNo, startCharNo);
            }

            const ParseFrame& openSection = nodeStack.back();
            if( !openSection.node->data.has_value() ||
                *openSection.node->data != buffer ) {
              std::ostringstream oss;
              oss << "Mismatched closing section '" << buffer
                  << "' at " << startLineNo << ":" << startCharNo
                  << "; expected '"
                  << (!openSection.node->data.has_value()
                      ? std::string() : *openSection.node->data)
                  << "' opened at " << openSection.lineNo
                  << ":" << openSection.charNo;
              throw TokenizerException(
                  oss.str(), startLineNo, startCharNo);
            }
          }

          std::unique_ptr<Node> pendingNode =
              std::make_unique<Node>(currentType, buffer, currentFlags);

          if( currentType == Node::TypeSection ) {
            pendingNode->startSequence = start;
            pendingNode->stopSequence = stop;
          }

          node = appendNode(nodeStack.back().node, std::move(pendingNode),
              &nodeCount, limits, startLineNo, startCharNo);
          // Push/pop stack
          if( currentType & Node::TypeHasChildren ) {
            nodeStack.push_back(
                ParseFrame(node, startLineNo, startCharNo));
          } else if( currentType == Node::TypeStop ) {
            nodeStack.pop_back();
          }
        }

        if( standaloneEnd != std::string_view::npos ) {
          appendLambdaOnlyOutput(nodeStack.back().node,
              tmpl.substr(pos + tmpStopL,
                  standaloneEnd - (pos + tmpStopL)),
              &nodeCount, limits, lineNo, charNo);
        }
        // Clear buffer
        buffer.clear();
        // Open new buffer
        inTag = false;
        skipUntil = standaloneEnd == std::string_view::npos
            ? pos + tmpStopL - 1
            : standaloneEnd - 1;
        // Triple mustache
        if( !skip && inTripleTag && stop.compare("}}") == 0 ) {
          if( !matchesAt(tmpl, pos + 2, "}") ) {
            std::ostringstream oss;
            oss << "Missing closing triple mustache delimiter at "
                << lineNo << ":" << charNo
                << " in tag starting at "
                << startLineNo << ":" << startCharNo;
            throw TokenizerException(oss.str(), lineNo, charNo);
          }
          skipUntil++;
        }
        inTripleTag = false;
        startLineNo = lineNo;
        startCharNo = charNo; // Could be inaccurate
      }
    }
    
    // Append to buffer
    if( skipUntil == std::string_view::npos ) {
      if( inTag && buffer.size() >= limits.maxTagBytes ) {
        throw TokenizerException(
            "Template tag size limit exceeded", lineNo, charNo);
      }
      buffer.append(1, tmpl[pos]);
    }
  }
  
  if( inTag ) {
    std::ostringstream oss;
    oss << "Unclosed tag at end of template, "
        << "starting at "
        << startLineNo << ":" << startCharNo;
    throw TokenizerException(oss.str(), startLineNo, startCharNo);
  } else if( nodeStack.size() > 1 ) {
    std::ostringstream oss;
    oss << "Unclosed section at end of template, depth was "
        << (nodeStack.size() - 1);
    throw TokenizerException(oss.str(), -1, -1);
  } else if( buffer.length() > 0 ) {
    std::unique_ptr<Node> pendingNode = std::make_unique<Node>();
    pendingNode->type = Node::TypeOutput;
    if( escapeOutput ) {
      pendingNode->data.emplace();
      htmlspecialchars_append(&buffer, &*pendingNode->data);
    } else {
      pendingNode->data = buffer;
    }
    appendNode(nodeStack.back().node, std::move(pendingNode),
        &nodeCount, limits, lineNo, charNo);
    buffer.clear();
  }

  // Partials are caller-managed fallback state rather than parser output.
  // Preserve them while atomically replacing the parsed portion of the root.
  parsedRoot.partials.swap(root->partials);
  *root = std::move(parsedRoot);
}


} // namespace Mustache
