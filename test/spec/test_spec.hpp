
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define MAX_TEST_FILES 50

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <yaml.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>

#include "mustache.hpp";

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
    bool passed() {
      return (expected == output);
    };
    void print() {
      
      std::cout << name << " ... " 
                << (passed() ? "PASSED" : "FAILED")
                << "\n";
              
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
