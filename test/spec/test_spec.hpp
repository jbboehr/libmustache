
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define MAX_TEST_FILES 50

#include <dirent.h>
#include <yaml.h>

#include <cstdlib>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include <exception>

#include "mustache.hpp"

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
    MustacheSpecTest() : _passed(-1) {};
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
    void print() {
      bool _passed = passed();
      std::cout << name << " ... " 
                << (_passed ? "PASSED" : "FAILED")
                << "\n";
      if( !_passed ) {
        std::cout << "Expected: " << expected << "\n";
        std::cout << "Output: " << output << "\n";
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
