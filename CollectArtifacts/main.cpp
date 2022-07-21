#include <iostream>
#include <string>
#include <exception>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char * argv[]) {

  try {

  po::options_description desc("Collect Artifacts command line arguments");
  desc.add_options()
    ("help", "print help message")
    ("variant,V", po::value<std::string>(), "debug or release")
    ("mem-model,M", po::value<int>(), "32 or 64")
    ("config,c", po::value<std::string>(), "path to artifacts description file")
    ("src-root,r", po::value<std::string>(), "path to directory that will be substituted as SrcRoot in artifacts description")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  if (vm.count("variant")) {
    std::cout << "variant:     " << vm["variant"].as<std::string>() << std::endl;
  }

  if (vm.count("mem-model")) {
    std::cout << "mem model:   " << vm["mem-model"].as<int>() << std::endl;
  }

  if (vm.count("config")) {
    std::cout << "config path: " << vm["config"].as<std::string>() << std::endl;
  }

  if (vm.count("src-root")) {
    std::cout << "source root: " << vm["src-root"].as<std::string>() << std::endl;
  }

  }
  catch(std::exception& e) {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
  }
  catch(...) {
      std::cerr << "Exception of unknown type!\n";
  }

  return 0;
}
