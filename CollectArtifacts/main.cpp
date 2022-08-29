#include <iostream>
#include <string>
#include <exception>
#include <regex>

#include <boost/program_options.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

//#define _TESTS_

namespace pt = boost::property_tree;
namespace po = boost::program_options;

namespace Deployka {
enum DependencyType : unsigned int {
  DDT_none,
  DDT_file,
  DDT_directory,
  DDT_pattern,
  DDT_MAX_VALUE
};

struct ArtifactDependency {
  DependencyType type; ///< type of dependency
  std::string path; ///< path to file or path to folder or root directory to apply search by pattern
  std::string pattern; ///< pattern to search
};

struct Artifact {
  std::string name; ///< unique name
  std::string pathToDir; ///< where to store collected artifact
  std::vector<ArtifactDependency> dependencies; ///< files that must be deployed along with target
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

  std::ostream& operator << (std::ostream& os, Deployka::ArtifactDependency const& aDependency) {
    switch (aDependency.type) {
    case Deployka::DDT_directory:
      os << "Directory: " << aDependency.path;
      break;
    case Deployka::DDT_file:
      os << "File: " << aDependency.path;
      break;
    case Deployka::DDT_pattern:
      os << "Pattern: " << aDependency.path << '/' << aDependency.pattern;
      break;
    default:
      os << "UNKNOWN (Type: " << aDependency.type << "path: " << aDependency.path << "; pattern: " << aDependency.pattern << ')';
      break;
    }
    return os;
  }

  std::ostream& operator << (std::ostream& os, Deployka::Artifact const& aTarget) {
    os << aTarget.name << " (" << aTarget.pathToDir << ")\n";
    os << "\nvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n";
    os << aTarget.dependencies;
    os << "\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    return os;
  }

}


// Опции:
// 1) путь к папке с конфигом
// (режим 1)
// 2) путь к корневой папке куда складывать (в соответствии с конфигом) артефакты
// (режим 2)
// 2) путь к корневой папке куда складывать (в соответствии с конфигом) артефакты и endpoint машины где targetHost крутится, чтобы
//     пересылать ему эти файлы вместе с путём куда их класть
//
// Читает конфиг, в нём все цели, для которых SrcRoot в текущей директории
//
// преобразовать все подстановки вида $identifier и str$($identifier)str
//
// Преобразование из какого-то формата задания в конкретный список файлов должно происходить перед передачей по сети/копированием в папку,
//     т.к. на этапе чтения конфига не известно какие именно файлы нужны из тех, что подходят под шаблон (например, не известно свежий ли файл)
//
// Если нужно прост положить в папку назначения
//     Для каждого файла:
//         если целевой файл есть и его дата изменения позже или равна дате изменеия исходного файла
//             continue
//
//         копировать файл с перезаписью
//
//
// Если нужно передать файлы по сети
//     Для каждого файла:
//         прочитать содержимое файла в буфер
//         создать структуру для отправки файла по сети
//         записать содержимое файла туда
//         добавить имя целевой папки
//         отправить по сети
//
// сформировать список файлов в соответствии с шаблонами в конфиге
// прочитать имена и даны обновления
// отправить этот список на целевую ВМ
// там подождать ответа
//

std::string substVars(std::map<std::string, std::string>& varsHash, std::string & inStr);

void TEST_regex() {
  std::string simpleId = "blah blah $someIdentifier bpowjentjb";

  std::regex re1{ R"-(\$\w[0-9a-zA-Z]*)-" };

  std::smatch matchResults;
  bool matchRes = std::regex_search(simpleId, matchResults, re1);

  std::cout << "match res: " << matchRes << '\n';


  std::regex re2{ R"-(\$\([^)]*\))-" };

  std::string p11dId = "blah blahe$($wasya)eworn poen";

  matchRes = std::regex_search(p11dId, matchResults, re2);

  std::cout << "match res: " << matchRes << '\n';
}

void TEST_substVars() {
  std::string example = 
R"str(<Dependency>$SrcRoot\VI\WebClient\Backend\WebServer\$Platform\$Config\WebServer.exe</Dependency>
    <Dependency>$SrcRoot\LicServer\DlLicModuleShell\$Platform\$Config\LicModShell.exe</Dependency>
    <SourceDirectory>$($RemoteDiskLetter):\Configs</SourceDirectory>
    <Dependency>$($RemoteDiskLetter):\DL80\SecServerConsole\BinVI\$Config\$Platform\ConsoleVi.exe</Dependency>
    <Dependency>$SrcRoot\DL80\SecServerConsole\BinVI\$($Config)Vi\$Platform\ServerConsoleVi.exe</Dependency>)str";

  std::map<std::string, std::string> varsHash = {
    {"platform", "x86"},
    {"remotediskletter", "E"},
    {"srcroot", R"-(E:\Files\System32\Drivers\etc\hosts)-"},
    {"config", "Debug"}
  };

  std::string res = substVars(varsHash, example);

  std::cout << "result: " << res;
}

/*!
 * @brief apply lowercase to each char in string
 * @param [in] in string to lowercase
 * @return same string but lowercased
*/
std::string & str_to_lower(std::string & in) {
  std::transform(in.begin(), in.end(), in.begin(), [](char c) { return (char)std::tolower(c); });
  return in;
}

/*!
* @brief substitudes the value of the variable where the name of the variable occurs with leading '$'
*        and reparses text inside $(...) (to separate variable names with plain text $($arch)bit => 64bit)
* @param[in] varsHash vars and their values
* @param[in] inStr string to substitude to
* @return string with all found variables substituted
*/
std::string substVars(std::map<std::string, std::string> & varsHash, std::string & inStr) {

  std::string result;

  std::regex re{ R"-(\$\([^)]*\))-" };

  std::smatch matchResults;

  auto reItEnd = std::sregex_iterator();

  auto reIt = std::sregex_iterator(inStr.begin(), inStr.end(), re);

  for (; reIt != reItEnd; ++reIt) {
    matchResults = *reIt;
    std::ssub_match item = matchResults[0];

    result.append(matchResults.prefix().first, matchResults.prefix().second);
    std::string tmp(item.first + 2, item.second - 1);
    str_to_lower(tmp);

    std::string tmpRes = substVars(varsHash, tmp);

    result.append(tmpRes);
  }

  if (matchResults.ready()) {
    std::ssub_match const & suffix = matchResults.suffix();

    if (suffix.length() > 0) {
      result.append(suffix.first, suffix.second);
      inStr = std::move(result);
      result.clear();
      result.reserve(inStr.length());
    }
    std::smatch tmp;
    matchResults.swap(tmp);
  }

  //foreach (std::ssub_match match in matchResults) {
  //	Console.WriteLine(match.str());
  //}

  std::regex re2{ R"-(\$\w[0-9a-zA-Z]*)-" };

  reIt = std::sregex_iterator(inStr.begin(), inStr.end(), re2);

  for (; reIt != reItEnd; ++reIt) {
    matchResults = *reIt;
    std::ssub_match const & item = matchResults[0];
    result.append(matchResults.prefix().first, matchResults.prefix().second);
    std::string tmp = item.str().substr(1);
    str_to_lower(tmp);

    if (!varsHash.count(tmp)) {
      std::cout << "[W] Cannot find item " << tmp << " in hash\n";
      result.append(item.first, item.second);
      continue;
    }

    result.append(varsHash.at(tmp));
  }

  result.append(matchResults.suffix().first, matchResults.suffix().second);

  return result;
}

/*!
 * @brief fill targets info from XML
 * @param[in] configFile path to XML file
 * @return vector of targets filled from XML file
*/
std::vector<Deployka::Artifact> loadTargetsFromXML(std::string const & configFile) {
//
  std::vector<Deployka::Artifact> result;
  pt::ptree aTree; //
 //
  pt::read_xml(configFile, aTree); //
//
  for (pt::ptree::value_type & targetNode : aTree.get_child("Root.Targets")) {
//
    Deployka::Artifact aTarget;
//
    aTarget.name = targetNode.second.get<std::string>("Name");
    aTarget.pathToDir = targetNode.second.get<std::string>("PathToDirectory");
    for (pt::ptree::value_type& dependencyNode : targetNode.second.get_child("Dependencies")) {
//
      Deployka::ArtifactDependency aDependency;
      aDependency.type = Deployka::DDT_none;
//
      if (dependencyNode.second.count("<xmlattr>")) {
//
        pt::ptree::value_type & xmlAttrVal = dependencyNode.second.get_child("<xmlattr>").front();
//
        if (xmlAttrVal.first == "type" && xmlAttrVal.second.data() == "pattern") {
          std::string rootDirStr = dependencyNode.second.get<std::string>("RootDir");
          std::string patternStr = dependencyNode.second.get<std::string>("Pattern");
          //std::cout << "dependency is a pattern: " << patternStr << " with root dir: " << rootDirStr << "\n";
          aDependency.type = Deployka::DDT_pattern;
          aDependency.path = rootDirStr;
          aDependency.pattern = patternStr;
        }
        else if (xmlAttrVal.first == "type" && xmlAttrVal.second.data() == "directory") {
          //std::cout << "dependency is a directory: " << dependencyNode.second.data() << "\n";
          aDependency.type = Deployka::DDT_directory;
          aDependency.path = dependencyNode.second.data();
        }
        else if (xmlAttrVal.first == "type" && xmlAttrVal.second.data() == "file") {
          //std::cout << "dependency is a file: " << dependencyNode.second.data() << "\n";
          aDependency.type = Deployka::DDT_file;
          aDependency.path = dependencyNode.second.data();
        }
        else {
          std::cout << "[W] Dependency is unknown type with data: " << dependencyNode.second.data() << "\n";
        }
      }
      else {
          //std::cout << "dependency is a file: " << dependencyNode.second.data() << "\n";
          aDependency.type = Deployka::DDT_file;
          aDependency.path = dependencyNode.second.data();
      }
//
      if (dependencyNode.first == "Dependency" && aDependency.type != Deployka::DDT_none) {
        aTarget.dependencies.emplace_back(aDependency);
      }
    }
//
    result.emplace_back(std::move(aTarget));
  }
//
  return result;
}

//================================================================================
int main(int argc, char * argv[]) {

  std::ios::sync_with_stdio(false);

#ifdef _TESTS_
  argc;
  argv;
  //TEST_regex();
  TEST_substVars();
#else
  std::string configFile;
  std::vector<std::string> requestedTargets;

  try {
  po::options_description desc("Collect Artifacts command line arguments");
  desc.add_options()
    ("help", "print help message")
    (
      "variant,V",   
      po::value<std::string>()->default_value("Debug"),
      "debug or release"
    )
    (
      "mem-model,M", 
      po::value<int>()->default_value(64),
      "32 or 64"
    )
    (
      "src-root,r",
      po::value<std::string>()->default_value("./"),
      "Path to directory that will be substituted as SrcRoot in artifacts description"
    )
    (
      "config,c",
      po::value<std::string>(&configFile)->default_value("./BuildArtifacts.xml"),
      "path to artifacts description file"
    )
    (
      "target,T",
      po::value<std::vector<std::string>>(&requestedTargets)->default_value({"Frontend", "WebServer"}),
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

  if (vm.count("variant")) {
    std::cout << "variant:     " << vm["variant"].as<std::string>() << std::endl;
  }

  if (vm.count("mem-model")) {
    std::cout << "mem model:   " << vm["mem-model"].as<int>() << std::endl;
  }

  if (vm.count("src-root")) {
    std::cout << "source root: " << vm["src-root"].as<std::string>() << std::endl;
  }

  if (vm.count("config")) {
    std::cout << "config path: " << vm["config"].as<std::string>() << std::endl;
  }

  if (vm.count("target")) {
    std::cout << "targets:     " << vm["target"].as<std::vector<std::string>>() << std::endl;
  }

  std::vector<Deployka::Artifact> targets = loadTargetsFromXML(configFile);
  std::vector<Deployka::Artifact> toProcess;
//
  for (std::string const& target : requestedTargets) {
    auto foundIt = std::find_if(targets.begin(), targets.end(), [&target](Deployka::Artifact artifact) { return artifact.name == target; });
    if (foundIt == targets.end()) {
      std::cout << "[W] Requested target " << target << " not found in XML\n";
      continue;
    } //
    toProcess.push_back(*foundIt);
  } //
//
  std::cout << "targets to process:\n" << toProcess << '\n';
//
  // TODO load settings from same XML config file
  std::map<std::string, std::string> varsHash = {
    {"platform", std::to_string(vm["mem-model"].as<int>())},
    {"srcroot", vm["src-root"].as<std::string>()},
    {"outroot", R"-(E:\files\OutRoot)-"},
    {"config", vm["variant"].as<std::string>()}
  };
//
  for (Deployka::Artifact& target : toProcess) {
//
    target.pathToDir = substVars(varsHash, target.pathToDir);
//
    for (Deployka::ArtifactDependency& dependency : target.dependencies) {
      dependency.path = substVars(varsHash, dependency.path);
      if (!dependency.pattern.empty()) {
        dependency.pattern = substVars(varsHash, dependency.pattern);
      }
    }
  }
//
  std::cout << "after subst vars: \n" << toProcess << '\n';

  } // /TRY
  catch(std::exception& e) {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
  }
  catch(...) {
      std::cerr << "Exception of unknown type!\n";
      return 2;
  }
#endif

  return 0;
}
