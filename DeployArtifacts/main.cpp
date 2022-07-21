#include <iostream>
#include <string>
#include <exception>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

int main(int argc, char * argv[]) {

  try {
  po::options_description desc("Deploy Artifacts command line arguments");
  desc.add_options()
    ("help", "print help message")
    ("config,c", po::value<std::string>(), "path to artifacts description file")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  if (vm.count("config")) {
    std::cout << "config path: " << vm["config"].as<std::string>() << std::endl;
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
