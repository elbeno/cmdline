#include "cmdline_opts.h"
using namespace cmdline_opts;

#include <iostream>
using namespace std;

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  string outputFile;
  string inputFile("wibble");
  bool doAll = false;

  vector<OptDescr> opts;
  opts.emplace_back(
      "o", "output",
      ArgDescr{"<file>", [&outputFile] (const string& s)
        { outputFile = s; }},
      "the file to output to");
  opts.emplace_back(
      "i", "input",
      ArgDescr{"<file>", [&inputFile] (const experimental::optional<string>& s)
        { if (s) inputFile = *s; }},
      "the file to input from");
  opts.emplace_back(
      "a", "all",
      ArgDescr{"", [&doAll] () { doAll = true; }},
      "do all the things");
  opts.emplace_back(
      "?", "help",
      ArgDescr{"", [&opts, &argv] () { usage(argv[0], opts); std::exit(0); }},
      "display help");

  if (!processOptions(argc, argv, opts))
  {
    usage(argv[0], opts);
    return 1;
  }

  std::cout << "Output file: " << (outputFile.empty() ? "(not supplied)" : outputFile) << std::endl;
  std::cout << "Input file: " << inputFile << std::endl;
  std::cout << "Do all: " << std::boolalpha << doAll << std::endl;

  return 0;
}
