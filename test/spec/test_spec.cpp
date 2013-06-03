
#include "test_spec.hpp"

int main( int argc, char * argv[] )
{
  if( argc < 2 ) {
    fprintf(stderr, "Requires at least one argument\n");
    return 1;
  }
  
  DIR * dir;
  struct dirent * ent;
  char * directory = argv[1];
  if( (dir = opendir(directory)) == NULL ) {
    fprintf(stderr, "Unable to open directory\n");
    return 1;
  }
  
  int nFiles = 0;
  while( (ent = readdir(dir)) != NULL ) {
    if( ent->d_name[0] == '.' ) continue;
    if( strlen(ent->d_name) < 5 ) continue;
    if( strcmp(ent->d_name + strlen(ent->d_name) - 4, ".yml") != 0 ) continue;
    if( nFiles >= MAX_TEST_FILES ) continue;
    
    // Make filename
    char * file = ent->d_name;
    std::string fileName;
    fileName += directory;
    fileName += file;
    
    std::ifstream pFile(fileName.c_str());
    if( !pFile.is_open() ) {
      continue;
    }
    
    // get length of file:
    pFile.seekg (0, pFile.end);
    int length = pFile.tellg();
    pFile.seekg (0, pFile.beg);
    
    // read file data
    char * fileData = new char[length];
    pFile.read(fileData, length);
    pFile.close();
    
    // parse the file
    parse_file(fileData, length);
  }
  closedir(dir);
  
  
  return 0;
}

void parse_file(char * fileData, int length)
{
  // start yaml parser
  yaml_parser_t parser;
  yaml_event_t event;
  yaml_parser_initialize(&parser);

  const unsigned char * input = reinterpret_cast<const unsigned char *>(fileData);

  yaml_parser_set_input_string(&parser, input, length);

  int done = 0;
  while (!done) {
    if( !yaml_parser_parse(&parser, &event) ) {
      return;
    }

    switch( event.type ) {

    }

    /* Are we finished? */
    done = (event.type == YAML_STREAM_END_EVENT);

    /* The application is responsible for destroying the event object. */
    yaml_event_delete(&event);
  }
  yaml_parser_delete(&parser);
}