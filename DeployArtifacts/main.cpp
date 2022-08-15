#include <iostream>
#include <string>
#include <exception>

#include <boost/program_options.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace pt = boost::property_tree;
namespace po = boost::program_options;

enum ObjectType : unsigned long long int {
  TOT_unknown,
  TOT_service,
  TOT_process,
  TOT_file,
  TOT_max
};

ObjectType objectTypeStringToEnum(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](char c) {return static_cast<char>(tolower(c));});
  if (s == "service") {
    return TOT_service;
  }
  if (s == "process") {
    return TOT_process;
  }
  if (s == "file") {
    return TOT_file;
  }

  return TOT_unknown;
}

struct DeployTarget {
  //<Name>SecServer</Name>
  std::string id;
  //<FileName>SecServer.exe</FileName>
  std::string fileName;
  //<Dependencies></Dependencies>
  std::vector<std::string> dependencies; // id of dependent Targets
  //<TargetDirectory>C:\DLVI\VI</TargetDirectory>
  std::string targetDirectory;
  //<SourceDirectory>C:\temp\Core</SourceDirectory>
  std::string sourceDirectory;
  //<Type>Service</Type>
  //<ReplaceMethod>
  ObjectType objectType;
  //  <Type>Service</Type>
  //  <ProcessName>SecServer</ProcessName>
  //  <ServiceName>*Dallas Lock*</ServiceName>
  //</ReplaceMethod>
};


int main(int argc, char * argv[]) {

  std::string configFile;

  try {
  po::options_description desc("Deploy Artifacts command line arguments");
  desc.add_options()
    ("help", "print help message")
    ("config,c", po::value<std::string>(&configFile)->default_value("./BinariesUpdate.xml"), "path to artifacts description file")
    ("target,T", po::value<std::vector<std::string>>(), "Which target from config to collected")
  ;

  po::positional_options_description posOpts;
  posOpts.add("target", -1);

  po::variables_map vm;
  po::store(
    po::command_line_parser(argc, argv)
      .options(desc)
      .positional(posOpts)
      .run(),
    vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 0;
  }

  if (vm.count("config")) {
    std::cout << "config path: " << vm["config"].as<std::string>() << std::endl;
  }

  if (vm.count("target")) {
    std::vector<std::string> vec = vm["target"].as<std::vector<std::string>>();
  }

  pt::ptree aTree;

  std::vector<DeployTarget> targetsVector;

  pt::read_xml(configFile, aTree);

  for (pt::ptree::value_type & targetNode : aTree.get_child("Root.Targets")) {

    DeployTarget aTarget;

    aTarget.id = targetNode.second.get<std::string>("Name");
    aTarget.fileName = targetNode.second.get<std::string>("FileName");
    aTarget.objectType = objectTypeStringToEnum(targetNode.second.get<std::string>("Type"));
    aTarget.sourceDirectory = targetNode.second.get<std::string>("SourceDirectory");
    aTarget.targetDirectory = targetNode.second.get<std::string>("TargetDirectory");
    for (pt::ptree::value_type& dependencyNode : targetNode.second.get_child("Dependencies")) {
      //size_t srcStrSize = dependencyNode.first.size();
      //std::unique_ptr<wchar_t[]> tmpWCharBuf = std::make_unique<wchar_t[]>(srcStrSize);
      //std::mbstowcs(tmpWCharBuf.get(), dependencyNode.first.c_str(), srcStrSize);

      //std::cout
      //  << "f: " << dependencyNode.first << "; "
      //  << "s: " << dependencyNode.second.data() << "; "
      //  << "xmlattrs: " << dependencyNode.second.count("<xmlattr>") << "\n";

      if (dependencyNode.second.count("<xmlattr>")) {
        std::cout
          << "rd: " << dependencyNode.second.count("RootDir") << "; "
          << "pat: " << dependencyNode.second.count("Pattern") << ";\n";

        pt::ptree::value_type & xmlAttrVal = dependencyNode.second.get_child("<xmlattr>").front();
        //pt::ptree::value_type & xmlRootDir = dependencyNode.second.get_child("RootDir").front();
        //pt::ptree::value_type & xmlPattern = dependencyNode.second.get_child("Pattern").front();

        std::cout << xmlAttrVal.first << " => " << xmlAttrVal.second.data() << '\n';
        //std::cout << xmlRootDir.first << "\n";
        //std::cout << xmlPattern.first << "\n";
      }

      if (dependencyNode.first == "Dependency") {
        aTarget.dependencies.emplace_back(dependencyNode.second.data());
      }
    }

    //std::cout << aTarget << '\n';
    targetsVector.emplace_back(std::move(aTarget));
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
