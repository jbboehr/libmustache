
#include "tokenizer.hpp"

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

} // namespace


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
  tokenize(std::string_view(*tmpl), root, escapeOutput);
}

void Tokenizer::tokenize(
    std::string_view tmpl, Node * root, bool escapeOutput)
{
  if( root == NULL ) {
    throw Exception("Missing token root");
  }
  std::string stop(_stopSequence);
  std::string start(_startSequence);
  std::string buffer;
  buffer.reserve(100); // Reserver 100 chars
  
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
  
  int inTag = 0;
  int inTripleTag = 0;
  int skip = 0;
  int startCharNo = 0;
  int startLineNo = 0;
  Node::Type currentType = Node::TypeNone;
  int currentFlags = Node::FlagNone;
  
  //Node::Stack nodeStack;
  NodeStack nodeStack;
  Node * node;
  
  // Initialize root node and stack[0]
  root->type = Node::TypeRoot;
  root->flags = Node::FlagNone;
  root->data = NULL;
  
  nodeStack.push_back(root);
  
  // Scan loop
  for( pos = 0; pos < tmplL; pos++ ) {
    
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
        // Close previous buffer
        if( buffer.length() > 0 ) {
          node = new Node(Node::TypeOutput, buffer);
          nodeStack.back()->children.push_back(node);
          buffer.clear();
        }
        // Open new buffer
        inTag = true;
        skipUntil = pos + startL - 1;
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
          node = new Node(currentType, buffer, currentFlags);

          if( currentType == Node::TypeSection ) {
            node->startSequence = new std::string(start);
            node->stopSequence = new std::string(stop);
          }

          nodeStack.back()->children.push_back(node);
          // Push/pop stack
          if( currentType & Node::TypeHasChildren ) {
            nodeStack.push_back(node);
          } else if( currentType == Node::TypeStop ) {
            if( nodeStack.size() <= 0 ) {
              std::ostringstream oss;
              oss << "Extra closing section or missing opening section"
                  << " detected after tag starting at "
                  << startLineNo << ":" << startCharNo;
              throw TokenizerException(oss.str(), startLineNo, startCharNo);
            }
            nodeStack.pop_back();
          }
        }
        // Clear buffer
        buffer.clear();
        // Open new buffer
        inTag = false;
        skipUntil = pos + tmpStopL - 1;
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
    node = new Node();
    node->type = Node::TypeOutput;
    if( escapeOutput ) {
      node->data = new std::string();
      htmlspecialchars_append(&buffer, node->data);
    } else {
      node->data = new std::string(buffer);
    }
    nodeStack.back()->children.push_back(node);
    buffer.clear();
  }
}


} // namespace Mustache
