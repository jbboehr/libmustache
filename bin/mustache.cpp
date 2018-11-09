
//#ifdef HAVE_CONFIG_H
#include "mustache_config.h"
//#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
//#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>

#include "mustache.hpp"
#include "exception.hpp"
#include "utils.hpp"

#define MUSTACHE_BIN_INPUT_UNKNOWN 0
#define MUSTACHE_BIN_INPUT_JSON 1
#define MUSTACHE_BIN_INPUT_YAML 2
#define MUSTACHE_BIN_INPUT_MUSTACHE 3
#define MUSTACHE_BIN_INPUT_MUSTACHE_BIN 4
#define MUSTACHE_BIN_INPUT_NONE 99

static int error = 0;
static int mode = 0;
static int number = 1;
static int printReadable = 0;

static char * inputDataFileName = NULL;
static int inputDataFileType = 0;
static std::string inputData;

static char * inputTemplateFileName = NULL;
static std::string inputTemplate;

static char * outputFileName = NULL;
static std::ofstream outputFileStream;
static std::ostream * outputStream = NULL;

static std::map<std::string, std::string> partialFiles;
std::map<std::string, std::string>::iterator pf_it;

static mustache::Mustache must;
static mustache::Node node;
static mustache::Node::Partials partials;
static mustache::Data * data;

static int detectFileType(char * filename);
static std::string getFileContents(const char *filename);
static void showUsage();
static void showVersion();

int main( int argc, char * argv[] )
{
  // Process arguments
  int curopt;
  int numopt = 0;
  opterr = 0;
  
  while( (curopt = getopt(argc, argv, "hrvd:o:t:n:l:")) != -1 ) {
    numopt++;
    switch( curopt ) {
      case 'r':
        printReadable = 1;
        break;
      case 'v':
        showVersion();
        return 0;
        
      case 'd':
        inputDataFileName = optarg;
        break;
      case 'o':
        outputFileName = optarg;
        break;
      case 't':
        inputTemplateFileName = optarg;
        break;
      case 'n':
        number = atoi(optarg);
        break;
      case 'l': {
        std::string optargstr(optarg);
        std::vector<std::string> optargparts;
        mustache::explode("=", optargstr, &optargparts);
        if( optargparts.size() < 2 ) {
          fprintf(stderr, "Must specify a partial name and file in the format <name>=<file>\n");
          return 1;
        }
        partialFiles.insert(std::make_pair(optargparts[0], optargparts[1]));
        break;
      }
        
      case '?':
        if( optopt == 'd' || optopt == 'o' || optopt == 't' ) {
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        } else if( isprint(optopt) ) {
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        }
        return 1;
        break;
      
      default:
        error = 1;
      case 'h':
        showUsage();
        return 0;
    }
  }
  
  // Must have input file
  if( inputTemplateFileName == NULL ) {
    error = 1;
    fprintf(stderr, "Must specify an input template file with option -t\n");
    showUsage();
    return 1;
  }
  
  // Detect input types
  inputDataFileType = detectFileType(inputDataFileName);
  
  // Read input file
  inputTemplate = getFileContents(inputTemplateFileName);
  
  // Read partials
  for( pf_it = partialFiles.begin(); pf_it != partialFiles.end(); pf_it++ ) {
    try {
      partialFiles[pf_it->first] = getFileContents(pf_it->second.c_str());
    } catch( int e ) {
      fprintf(stderr, "Error reading partial: %s. Code: %d\n", pf_it->first.c_str(), e);
      return 1;
    }
  }
  
  // Check output file
  if( outputFileName == NULL ) {
    outputStream = &std::cout;
  } else {
    outputFileStream.open(outputFileName, std::ofstream::out/* | std::ofstream::app*/);
    outputStream = &outputFileStream;
  }
  
  // Tokenize
  must.tokenize(&inputTemplate, &node);
  
  // Tokenize partials
  for( pf_it = partialFiles.begin(); pf_it != partialFiles.end(); pf_it++ ) {
    mustache::Node node;
    partials.insert(std::make_pair(pf_it->first, node));
    must.tokenize(&pf_it->second, &(partials[pf_it->first]));
  }
  
  // Get input data
  if( inputDataFileName != NULL ) {
    inputData = getFileContents(inputDataFileName);
  }

  std::string * output = new std::string;
  
  // Process data
  try {
    if( inputData.length() ) {
      if( inputDataFileType == MUSTACHE_BIN_INPUT_JSON ) {
        data = mustache::Data::createFromJSON(inputData.c_str());
      } else if( inputDataFileType == MUSTACHE_BIN_INPUT_YAML ) {
        data = mustache::Data::createFromYAML(inputData.c_str());
      }
    }
  } catch( mustache::Exception& e ) {
    error = 1;
    fprintf(stderr, "%s\n", e.what());
    goto error;
  }
  
  // Render
  int i;
  for( i = 0; i < number; i++ ) {
    output->clear();
    must.render(&node, data, &partials, output);
  }
  
  // Output
  *outputStream << *output;
  
error:
  if( outputFileStream.is_open() ) {
    outputFileStream.close();
  }
  if( data != NULL ) {
    delete data;
  }
  return error;
}

static int detectFileType(char * filename)
{
  if( filename == NULL ) {
    return MUSTACHE_BIN_INPUT_NONE;
  }
  
  char * ext = strrchr(filename, '.');
  if( ext == NULL ) {
    return MUSTACHE_BIN_INPUT_UNKNOWN;
  }
  
  if( strcmp(ext, ".json") == 0 ) {
    return MUSTACHE_BIN_INPUT_JSON;
  } else if( strcmp(ext, ".yml") == 0 ) {
    return MUSTACHE_BIN_INPUT_YAML;
  } else if( strcmp(ext, ".mustache") == 0 ) {
    return MUSTACHE_BIN_INPUT_MUSTACHE;
  } else {
    return MUSTACHE_BIN_INPUT_UNKNOWN;
  }
}

static std::string getFileContents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if( in ) {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}

static void showUsage()
{
  fprintf(stdout, "Usage: mustache -e -t <template file> -d <data file> (-o <output file>)\n");
  fprintf(stdout, "       mustache -c -t <template file> (-o <output file>)\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Modes:\n");
  fprintf(stdout, "    -c, Compile\n");
  fprintf(stdout, "    -e, Execute\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "    -d, Input data file\n");
  fprintf(stdout, "    -o, Output file\n");
  fprintf(stdout, "    -p, Print human-readable bytecode\n");
  fprintf(stdout, "    -t, Input template file\n");
  fprintf(stdout, "    -l, Partials in format name=file\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Supported data formats:\n");
  fprintf(stdout, "    ");
#if defined(MUSTACHE_HAVE_LIBJSON) || defined(MUSTACHE_HAVE_LIBJANSSON)
  fprintf(stdout, "json ");
#endif
#ifdef MUSTACHE_HAVE_LIBYAML
  fprintf(stdout, "yaml ");
#endif
  fprintf(stdout, "\n");
}

static void showVersion()
{
  fprintf(stdout, "mustache %s\n", mustache_version());
#ifdef MUSTACHE_HAVE_LIBJSON
  fprintf(stdout, "JSON support: libjson\n");
#elif MUSTACHE_HAVE_LIBJANSSON
  fprintf(stdout, "JSON support: jansson\n");
#else
  fprintf(stdout, "JSON support: none\n");
#endif
#ifdef MUSTACHE_HAVE_LIBYAML
  fprintf(stdout, "YAML support: libyaml\n");
#else
  fprintf(stdout, "JSON support: none\n");
#endif
#ifdef MUSTACHE_HAVE_CXX11
  fprintf(stdout, "C++11 support: enabled\n");
#else
  fprintf(stdout, "C++11 support: disabled\n");
#endif
}
