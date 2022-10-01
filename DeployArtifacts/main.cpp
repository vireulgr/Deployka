#include "UtilityLib.h"

#include <iostream>
#include <string>
#include <exception>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

//#define _TESTS_

namespace pt = boost::property_tree;
namespace po = boost::program_options;

namespace Deployka {
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

struct Target {
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
}

namespace std {
  template <class T>
  std::ostream& operator << (std::ostream& os, std::vector<T> const& vec) {

    if (!vec.empty()) {
      os << vec.front();
    }
    for (auto it = vec.begin() + 1; it != vec.end(); ++it) {
      os << ", ";
      os << *it;
    }

    return os;
  }
//
  std::ostream& operator << (std::ostream& os, Deployka::Target const& aTarget) {
    os << aTarget.id << "; src: " << aTarget.sourceDirectory << "; dest: " << aTarget.targetDirectory;
    os << "\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
    os << aTarget.dependencies;
    os << "\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    return os;
  }
}
//
/*!
 * @brief Loads targets from XML config
 * @param configFile Path to config
 * @return vector of DeployTargets
*/
std::vector<Deployka::Target> loadTargetsFromXML(std::string configFile) {
  pt::ptree aTree;//
//
  std::vector<Deployka::Target> targetsVector;//
//
  pt::read_xml(configFile, aTree);//
//
  for (pt::ptree::value_type & targetNode : aTree.get_child("Root.Targets")) {//
//
    Deployka::Target aTarget;//
//
    aTarget.id = targetNode.second.get<std::string>("Name");//
    aTarget.fileName = targetNode.second.get<std::string>("FileName");//
    aTarget.objectType = Deployka::objectTypeStringToEnum(targetNode.second.get<std::string>("Type"));//
    aTarget.sourceDirectory = targetNode.second.get<std::string>("SourceDirectory");//
    aTarget.targetDirectory = targetNode.second.get<std::string>("TargetDirectory");//
//
    for (pt::ptree::value_type& dependencyNode : targetNode.second.get_child("Dependencies")) {
//
      if (dependencyNode.second.count("<xmlattr>")) {
        std::cout
          << "rd: " << dependencyNode.second.count("RootDir") << "; "
          << "pat: " << dependencyNode.second.count("Pattern") << ";\n";
//
        pt::ptree::value_type & xmlAttrVal = dependencyNode.second.get_child("<xmlattr>").front();
        //std::cout << xmlAttrVal.first << " => " << xmlAttrVal.second.data() << '\n';
      }
//
      if (dependencyNode.first == "Dependency") {
        aTarget.dependencies.emplace_back(dependencyNode.second.data());
      }
    }
//
    //std::cout << aTarget << '\n';
    targetsVector.emplace_back(std::move(aTarget));
  }
//
  return targetsVector;
}
//
#ifdef _TESTS_
void TEST_readXmlSettings2() {
  std::string fileName = "./BinariesUpdate.xml";
  std::multimap<std::string, std::string> varsDict = Deployka::loadSettingsFromXML2(fileName);
  for (auto& item : varsDict) {
    std::cout << item.first << " => " << item.second << '\n';
  }
}
#endif

int main(int argc, char * argv[]) {

#ifdef _TESTS_
  argc;
  argv;
  try {
    TEST_readXmlSettings2();
  }
  catch (std::runtime_error& e) {
    std::cerr << e.what();
  }
#else

  std::string configFile;
  std::vector<std::string> requestedTargets;

  try {
  po::options_description desc("Deploy Artifacts command line arguments");
  desc.add_options()
    ("help", "print help message")
    ("config,c", po::value<std::string>(&configFile)->default_value("./BinariesUpdate.xml"), "path to artifacts description file")
    (
      "target,T",
      po::value<std::vector<std::string>>(&requestedTargets)->default_value({"NotebookWasya", "SubfolderTarget", "PatternTarget"}),
      "Which target from config to collected"
    )
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
//
  if (vm.count("target")) {
    std::vector<std::string> vec = vm["target"].as<std::vector<std::string>>();
  }
//
  // load targets from XML
  std::vector<Deployka::Target> targets = loadTargetsFromXML(configFile);
  std::vector<Deployka::Target> toProcess;
//
  for (std::string const& requested : requestedTargets) {
    auto foundIt = std::find_if(targets.begin(), targets.end(), [&requested](Deployka::Target artifact) { return artifact.id == requested; });
    if (foundIt == targets.end()) {
      std::cout << "[W] Requested target " << requested << " not found in XML\n";
      continue;
    }//
    toProcess.push_back(*foundIt);
  }//
//
  // load settings from XML
  std::multimap<std::string, std::string> varsDict = Deployka::loadSettingsFromXML2(configFile);
  varsDict.insert({ "memmodel" , std::to_string(vm["mem-model"].as<int>()) });
  varsDict.insert({ "srcroot" , vm["src-root"].as<std::string>() });
  varsDict.insert({ "variant" , vm["variant"].as<std::string>() });
//
  // substitute variables in targets from XML
  auto varsMapIt = varsDict.begin();
  for (; varsMapIt != varsDict.end(); ++varsMapIt) {
    varsMapIt->second = Deployka::substVars2(varsDict, varsMapIt->second);
  }//
//
  for (Deployka::Target& target : toProcess) {
    target.targetDirectory = Deployka::substVars2(varsDict, target.targetDirectory);
    target.sourceDirectory = Deployka::substVars2(varsDict, target.sourceDirectory);
  }//

  }
  catch(std::exception& e) {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
  }
  catch(...) {
      std::cerr << "Exception of unknown type!\n";
  }
#endif

  return 0;
}
