
#include "Libconfig.h"

#include <iostream>
#include <string>

int main(int argc, char **argv)
{
  char const* filename;
  char const* configItemAddress;
  std::string value;
  if (argc > 2)
  {
    filename = argv[1];
    configItemAddress = argv[2];
  }
  else
  {
    std::cerr << "Error: No input file provided." << std::endl;
    return (1);
  }

  // Parse the configuration
  libconfig::Configuration config(filename);

  // Print the configuration
  config.print();

  // Look up a string value in the configuration
  bool found =  config.lookupValue(configItemAddress, value);
  if (found)
    std::cout << configItemAddress << ": \"" << value << "\"" << std::endl;
  else
    std::cout << configItemAddress << " not found" << std::endl;
  
  return (0);
}
