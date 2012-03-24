
#include "tokenizer.hpp"

namespace mustache {


void Tokenizer::setStartSequence(const std::string& start) {
  _startSequence.assign(start);
}

void Tokenizer::setStartSequence(const char * start, long len) {
  // @todo specifying the length seems to cause huge allocations
  //if( len <= 0 ) {
    _startSequence.assign(start);
  //} else {
  //  _startSequence.assign(start, (size_t) len);
  //}
}

void Tokenizer::setStopSequence(const std::string& stop) {
  _stopSequence.assign(stop);
}

void Tokenizer::setStopSequence(const char * stop, long len) {
  // @todo specifying the length seems to cause huge allocations
  //if( len <= 0 ) {
    _stopSequence.assign(stop);
  //} else {
  //  _stopSequence.assign(stop, (size_t) len);
  //}
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

void Tokenizer::tokenize(std::string * tmpl, Node * root)
{
  unsigned int tmplL = tmpl->length();
  const char * chr = tmpl->c_str();
  
  std::string start(_startSequence);
  char startC = start.at(0);
  long startL = start.length();
  
  std::string stop(_stopSequence);
  char stopC = stop.at(0);
  long stopL = stop.length();
  long tmpStopL = stopL;
  
  long pos = 0;
  long skipUntil = -1;
  long lineNo = 1;
  long charNo = 0;
  
  int inTag = 0;
  int inTripleTag = 0;
  int skip = 0;
  int startCharNo = 0;
  int startLineNo = 0;
  int currentFlags = Node::FlagNone;
  std::string buffer;
  buffer.reserve(100); // Reserver 100 chars
  
  int depth = 0;
  Node::Stack nodeStack;
  Node * node;
  
  // Initialize root node and stack[0]
  root->type = Node::TypeRoot;
  root->flags = Node::FlagNone;
  root->data = NULL;
  
  nodeStack.push(root);
  
  // Scan loop
  chr = tmpl->c_str();
  for( pos = 0; pos < tmplL; pos++ ) {
    
    // Track line numbers
    if( *chr == '\n' ) {
      lineNo++;
      charNo = 0;
    } else {
      charNo++;
    }
      
    // Skip until position
    if( skipUntil > -1 ) {
      if( pos <= skipUntil ) {
        chr++;
        continue;
      } else {
        skipUntil = -1;
      }
    }
    
    // Main
    if( !inTag ) {
      if( *chr == startC && tmpl->compare(pos, startL, start) == 0 ) {
        // Close previous buffer
        if( buffer.length() > 0 ) {
          node = new Node(Node::TypeOutput, buffer);
          nodeStack.top()->children.push_back(node);
          buffer.clear();
        }
        // Open new buffer
        inTag = true;
        skipUntil = pos + startL - 1;
        startLineNo = lineNo;
        startCharNo = charNo; // Could be inaccurate
        // Triple mustache
        if( start.compare("{{") == 0 && tmpl->compare(pos+2, 1, "{") == 0 ) {
          inTripleTag = true;
          skipUntil++;
        }
      }
    } else {
      if( *chr == stopC && tmpl->compare(pos, stopL, stop) == 0 ) {
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
        currentFlags = Node::FlagNone;
        switch( buffer.at(0) ) {
          case '&':
            currentFlags = Node::FlagEscape;
            break;
          case '^':
            currentFlags = Node::FlagNegate;
            break;
          case '#':
            currentFlags = Node::FlagSection;
            break;
          case '/':
            currentFlags = Node::FlagStop;
            break;
          case '!':
            currentFlags = Node::FlagComment;
            break;
          case '>':
            currentFlags = Node::FlagPartial;
            break;
          case '<':
            currentFlags = Node::FlagInlinePartial;
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
          if( currentFlags > 0 ) {
            buffer.erase(0, 1);
            trim(buffer);
          }
          if( currentFlags == 0 || currentFlags == Node::FlagEscape ) {
            // @todo kind of hack-ish
            if( inTripleTag ) {
              currentFlags = currentFlags | Node::FlagEscape;
            }
            if( !_escapeByDefault ) {
              currentFlags = currentFlags ^ Node::FlagEscape;
            }
          }
          // Create node
          if( currentFlags & Node::FlagInlinePartial ) { 
            root->partials.insert(std::make_pair(buffer, mustache::Node(Node::TypeRoot, buffer, currentFlags)));
            node = &(root->partials[buffer]);
//            node->type = Node::TypeRoot; // Kind of hackish
//            node->data = new std::string(buffer);
//            node->flags = currentFlags;
          } else {
            node = new Node(Node::TypeTag, buffer, currentFlags);
            nodeStack.top()->children.push_back(node);
          }
          // Push/pop stack
          if( currentFlags & Node::FlagHasChildren ) {
            depth++;
            nodeStack.push(node);
          } else if( currentFlags & Node::FlagStop ) {
            nodeStack.pop();
            depth--;
            if( depth < 0 ) {
              std::ostringstream oss;
              oss << "Extra closing section or missing opening section"
                  << " detected after tag starting at "
                  << startLineNo << ":" << startCharNo;
              throw TokenizerException(oss.str(), startLineNo, startCharNo);
            }
          }
        }
        // Clear buffer
        buffer.clear();
        // Open new buffer
        inTag = false;
        skipUntil = pos + tmpStopL - 1;
        // Triple mustache
        if( !skip && inTripleTag && stop.compare("}}") == 0 ) {
          if( tmpl->compare(pos+2, 1, "}") != 0 ) {
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
    if( skipUntil == -1 ) {
      buffer.append(1, *chr);
    }
    
    // Increment character pointer
    chr++;
  }
  
  if( inTag ) {
    std::ostringstream oss;
    oss << "Unclosed tag at end of template, "
        << "starting at "
        << startLineNo << ":" << startCharNo;
    throw TokenizerException(oss.str(), startLineNo, startCharNo);
  } else if( depth > 0 ) {
    std::ostringstream oss;
    oss << "Unclosed section at end of template";
    throw TokenizerException(oss.str(), -1, -1);
  } else if( buffer.length() > 0 ) {
    node = new Node();
    node->type = Node::TypeOutput;
    node->data = new std::string(buffer);
    nodeStack.top()->children.push_back(node);
    buffer.clear();
  }
}


} // namespace Mustache
