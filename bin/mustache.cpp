
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <fstream>
#include <vector>

#include "mustache.hpp"
#include "compiler.hpp"
#include "exception.hpp"
#include "vm.hpp"

#define MUSTACHE_BIN_MODE_COMPILE 1
#define MUSTACHE_BIN_MODE_EXECUTE 2
#define MUSTACHE_BIN_MODE_PRINT 3

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
static int inputTemplateFileType = 0;
static std::string inputTemplate;

static char * outputFileName = NULL;
static std::ofstream outputFileStream;
static std::ostream * outputStream = NULL;

static mustache::Mustache must;
static mustache::Compiler compiler;
static mustache::VM vm;
static mustache::Node node;
static mustache::Data * data;

static int detectFileType(char * filename);
static std::string getFileContents(const char *filename);
static void showUsage();

int main( int argc, char * argv[] )
{
  // Process arguments
  int curopt;
  int numopt = 0;
  opterr = 0;
  
  while( (curopt = getopt(argc, argv, "hceprd:o:t:")) != -1 ) {
    numopt++;
    switch( curopt ) {
      case 'c':
        mode = MUSTACHE_BIN_MODE_COMPILE;
        break;
      case 'e':
        mode = MUSTACHE_BIN_MODE_EXECUTE;
        break;
      case 'p':
        mode = MUSTACHE_BIN_MODE_PRINT;
        break;
      case 'r':
        printReadable = 1;
        break;
        
      case 'd':
        inputDataFileName = optarg;
        break;
      case 'o':
        outputFileName = optarg;
        break;
      case 't':
        inputTemplateFileName = optarg;
        break;
        
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
        goto error;
        break;
    }
  }
  
  // Must specify a mode
  if( mode != MUSTACHE_BIN_MODE_COMPILE && 
      mode != MUSTACHE_BIN_MODE_EXECUTE &&
      mode != MUSTACHE_BIN_MODE_PRINT ) {
    error = 1;
    fprintf(stderr, "Must specify either -c (compile) or -e (execute) or -p (print)\n");
    showUsage();
    goto error;
  }
  
  // Must have input file
  if( inputTemplateFileName == NULL ) {
    error = 1;
    fprintf(stderr, "Must specify an input template file with option -t\n");
    showUsage();
    goto error;
  }
  
  // Detect input types
  inputTemplateFileType = detectFileType(inputTemplateFileName);
  inputDataFileType = detectFileType(inputDataFileName);
  
  // Read input file
  inputTemplate = getFileContents(inputTemplateFileName);
  
  // Check output file
  if( outputFileName == NULL ) {
    outputStream = &std::cout;
  } else {
    outputFileStream.open(outputFileName, std::ofstream::out/* | std::ofstream::app*/);
    outputStream = &outputFileStream;
  }
  
  // Execute mode
  if( mode == MUSTACHE_BIN_MODE_EXECUTE ) {
    // Tokenize
    if( inputTemplateFileType == MUSTACHE_BIN_INPUT_MUSTACHE ) {
      must.tokenize(&inputTemplate, &node);
    }
    
    // Get input data
    if( inputDataFileName != NULL ) {
      inputData = getFileContents(inputDataFileName);
    }
    
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
    std::string * output = new std::string;
    if( inputTemplateFileType == MUSTACHE_BIN_INPUT_MUSTACHE ) {
      must.render(&node, data, NULL, output);
    } else if( inputTemplateFileType == MUSTACHE_BIN_INPUT_MUSTACHE_BIN ) {
      uint8_t * codes = NULL;
      int length = 0;
      mustache::Compiler::stringToBuffer(&inputTemplate, &codes, &length);
      output = vm.execute(codes, length, data);
    }
    
    // Output
    *outputStream << *output;
    
  } else if( mode == MUSTACHE_BIN_MODE_COMPILE ) {
    // Tokenize
    must.tokenize(&inputTemplate, &node);
    
    // Compile
    std::vector<uint8_t> * codes = compiler.compile(&node);
    
    // Output
    if( codes != NULL ) {
      if( printReadable ) {
        std::string * out = compiler.print(codes);
        *outputStream << *out;
        delete out;
      } else {
        char buf[101];
        std::vector<uint8_t>::iterator it;
        for( it = codes->begin(); it != codes->end(); it++ ) {
          *outputStream << *it;
        }
      }
    }
    
  } else if( mode == MUSTACHE_BIN_MODE_PRINT ) {
    
    std::vector<uint8_t> * codes;
    mustache::Compiler::stringToVector(&inputTemplate, &codes);
    std::string * output = compiler.print(codes);
    
    // Output
    *outputStream << *output;
    
  } else {
    error = 1;
    fprintf(stderr, "Invalid mode\n");
    goto error;
  }
  
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
  } else if( strcmp(ext, ".bin") == 0 ) {
    return MUSTACHE_BIN_INPUT_MUSTACHE_BIN;
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
  fprintf(stdout, "\n");
  fprintf(stdout, "Supported data formats:\n");
  fprintf(stdout, "    ");
#if defined(HAVE_LIBJSON) || defined(HAVE_LIBJANSSON)
  fprintf(stdout, "json ");
#endif
#ifdef HAVE_LIBYAML
  fprintf(stdout, "yaml ");
#endif
  fprintf(stdout, "\n");
}
