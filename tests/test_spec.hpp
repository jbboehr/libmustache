
#include "mustache_config.h"

#include <stdio.h>
#include <stdlib.h>

#include <yaml.h>

#include <cstdlib>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include <exception>

#include "mustache.hpp"
#include "spec_expectations.hpp"

class MustacheSpecTest {
  private:
  public:
    std::string name;
    std::string desc;
    mustache::Data data;
    std::string tmpl;
    mustache::Node::Partials partials;
    std::string expected;
    std::string output;
    std::string suite;
    std::string expectationReason;
    bool skipped;
    bool knownFailure;
    int _passed;
    
    MustacheSpecTest() : skipped(false), knownFailure(false), _passed(-1) {};
    bool passed() {
      if( -1 == _passed ) {
        if( output == expected ) {
          _passed = 1;
        } else {
          _passed = 0;
        }
      }
      return (_passed == 1);
    };
    static std::string escapeForDiagnostic(const std::string& value) {
      std::string escaped;
      char buffer[5];
      for( std::size_t i = 0; i < value.size(); ++i ) {
        const unsigned char chr = static_cast<unsigned char>(value[i]);
        switch( chr ) {
          case '\\':
            escaped.append("\\\\");
            break;
          case '"':
            escaped.append("\\\"");
            break;
          case '\n':
            escaped.append("\\n");
            break;
          case '\r':
            escaped.append("\\r");
            break;
          case '\t':
            escaped.append("\\t");
            break;
          default:
            if( chr < 0x20 || chr >= 0x7f ) {
              snprintf(buffer, sizeof(buffer), "\\x%02x", chr);
              escaped.append(buffer);
            } else {
              escaped.push_back(static_cast<char>(chr));
            }
            break;
        }
      }
      return escaped;
    }
    void print() {
      std::cout << suite << " / " << name << " ... ";
      if( skipped ) {
        std::cout << "SKIPPED (" << expectationReason << ")\n";
        return;
      }

      const bool outputMatches = passed();
      if( knownFailure ) {
        std::cout << (outputMatches ? "UNEXPECTED PASS" : "KNOWN FAILURE")
                  << " (" << expectationReason << ")\n";
      } else {
        std::cout << (outputMatches ? "PASSED" : "FAILED") << "\n";
      }

      if( !outputMatches && !knownFailure ) {
        std::cout << "Expected: \"" << escapeForDiagnostic(expected)
                  << "\"\n";
        std::cout << "Output:   \"" << escapeForDiagnostic(output)
                  << "\"\n";
      }
    }
    std::string toString() {
      std::string ret;
      ret.append("name=");
      ret.append(name);
      ret.append(", ");
      ret.append("desc=");
      ret.append(desc);
      ret.append(", ");
      ret.append("tmpl=");
      ret.append(tmpl);
      ret.append(", ");
      ret.append("expected=");
      ret.append(expected);
      return ret;
    }
};

void parse_file(char * fileData, int length);

void mustache_spec_parse_document(yaml_document_t * document);
void mustache_spec_parse_tests(yaml_document_t * document, yaml_node_t * node);
void mustache_spec_parse_test(yaml_document_t * document, yaml_node_t * node);
void mustache_spec_parse_data(yaml_document_t * document, yaml_node_t * node, mustache::Data * data);
void mustache_spec_parse_partials(yaml_document_t * document, yaml_node_t * node, mustache::Node::Partials * partials);
