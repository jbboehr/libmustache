
//#ifdef HAVE_CONFIG_H
#include <mustache_config.h>
//#endif

#define MAX_TEST_FILES 50

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <dirent.h>
#include <yaml.h>

#include <cstdlib>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include <exception>

#include "mustache.hpp"
#include "compiler.hpp"
#include "vm.hpp"

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
    int _passed;
    
    uint8_t * compiled;
    size_t compiled_length;
    std::string compiled_output;
    int _compiled_passed;
    
    MustacheSpecTest() : _passed(-1), _compiled_passed(-1) {};
    bool passed() {
      if( -1 == _passed ) {
        std::string strippedExpected = expected;
        std::string strippedOutput = output;
        mustache::stripWhitespace(strippedExpected);
        mustache::stripWhitespace(strippedOutput);
        if( strippedOutput == strippedExpected ) {
          _passed = 1;
        } else {
          _passed = 0;
        }
      }
      return (_passed == 1);
    };
    bool compiled_passed() {
      if( -1 == _compiled_passed ) {
        std::string strippedExpected = expected;
        std::string strippedOutput = compiled_output;
        mustache::stripWhitespace(strippedExpected);
        mustache::stripWhitespace(strippedOutput);
        if( strippedOutput == strippedExpected ) {
          _compiled_passed = 1;
        } else {
          _compiled_passed = 0;
        }
      }
      return (_compiled_passed == 1);
    }
    void print() {
      bool _passed = passed();
      std::cout << name << " ... " 
                << (_passed ? "PASSED" : "FAILED")
                << "\n";
      if( !_passed ) {
        std::cout << "Expected: " << expected << "\n";
        std::cout << "Output: " << output << "\n";
      }
      bool _compiled_passed = compiled_passed();
      std::cout << name << " (compiled) ... " 
                << (_compiled_passed ? "PASSED" : "FAILED")
                << "\n";
      if( !_compiled_passed ) {
        std::cout << "Expected: " << expected << "\n";
        std::cout << "Output: " << compiled_output << "\n";
        std::cout << "Code: " << "\n" << *mustache::Compiler::print(compiled, compiled_length) << "\n";
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
