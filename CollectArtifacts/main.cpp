#include "UtilityLib.h"

#include <iostream>
#include <string>
#include <exception>
#include <regex>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/filesystem.hpp>

//#define _TESTS_

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;
namespace po = boost::program_options;

namespace Deployka {
enum CollectMethod : unsigned int {
  DCM_none,
  DCM_noCopyIfExists,
  DCM_replaceAnyway,
  DCM_replaceIfNewer,
  DCM_cleanup,
  DCM_MAX_VALUE
};

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
  std::string storeDir; ///< where to store collected artifact
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
    os << aTarget.name << " (" << aTarget.storeDir << ")\n";
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
// Преобразовать все подстановки вида $identifier и str$($identifier)str
//
// Преобразование из какого-то формата задания в конкретный список файлов должно происходить перед передачей по сети/копированием в папку,
//     т.к. на этапе чтения конфига не известно какие именно файлы нужны из тех, что подходят под шаблон (например, не известно свежий ли файл)
//
// Режим 1
// Если нужно прост положить в папку назначения
//     Для каждого файла:
//         если целевой файл есть и его дата изменения позже или равна дате изменеия исходного файла
//             continue
//
//         копировать файл с перезаписью
//
//
// Режим 2
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

/*
 * @brief If source file is newer than (existing) destination file,
 *        destination file will be deleted and source file will be copied
 *        to destination location
 * @param[in] from Full path to source file. If not exists, function will throw filesystem_error.
 * @param[in] to Full path to destination file. If not exists, function will throw filesystem_error.
*/
void replaceIfNewer(fs::path from, fs::path to) {
  //std::cout << "replace if newer: " << from << "; " << to << '\n';
  time_t lwtFrom = fs::last_write_time(from); /// may throw filesystem_error if to is not exists
  time_t lwtTo = fs::last_write_time(to); /// may throw filesystem_error if to is not exists
  if (lwtFrom > lwtTo) {
    fs::remove(to); /// if 'to' is not exists, remove(to) simply returns false
    fs::copy_file(from, to);
  }
}

struct MyCopyator {
  Deployka::CollectMethod method;
  fs::path destDirectory;
  fs::path srcRootDirectory;

  /*!
  * @brief Simply creates subdirectory in target directory 
  * @param[in] path Path to source directory
  */
  bool processDirectory(fs::path const & path) {
    //std::cout << "Copyator processDir  " << path << '\n';
    fs::path relativePath = fs::relative(path, this->srcRootDirectory);
    fs::path destDir = this->destDirectory / relativePath;
    fs::create_directory(destDir);
    return true;
  }

  /*!
   * @brief Performs action (corresponding to method member) on every source file
   * @param[in] path Path to source file
   * @return 
  */
  bool processFile(fs::path const & path) {
    //std::cout << "Copyator processFile " << path << '\n';
    fs::path relativePath = fs::relative(path, this->srcRootDirectory);
    fs::path destPath = this->destDirectory / relativePath;
    bool result = false;
    switch (this->method) {
    case Deployka::DCM_replaceIfNewer:
      if (fs::exists(destPath)) {
        ::replaceIfNewer(path, destPath);
      }
      else {
        fs::copy_file(path, destPath);
      }
      result = true;
      break;
    case Deployka::DCM_cleanup:
      //std::cout << "Copyator Removing " << destPath;
      fs::remove(destPath);
      result = true;
      break;
    case Deployka::DCM_replaceAnyway:
      if (fs::exists(destPath)) {
        fs::remove(destPath);
      }
      result = fs::copy_file(path, destPath);
    default:
      if (!fs::exists(destPath)) {
        result = fs::copy_file(path, destPath);
      }
      break;
    }
    return result;
  }
};

bool copyDirectoryRecursively(fs::path root, MyCopyator my) {

  //std::cout << "copyDirRecursively\n";
  bool doContinue = true;
  std::vector<fs::path> directories;

  fs::directory_iterator dirIt = fs::directory_iterator(root);
  fs::directory_iterator endDirIt2 = fs::directory_iterator();
  for (; dirIt != endDirIt2 && doContinue; ++dirIt) {

    if (fs::is_directory(dirIt->status())) {
      doContinue = my.processDirectory(dirIt->path());
      directories.push_back(dirIt->path());
    }
    else if (fs::is_regular_file(dirIt->status())) {
      doContinue = my.processFile(dirIt->path());
    }
  }

  for (auto vecIt = directories.begin(); vecIt != directories.end() && doContinue; ++vecIt) {
    doContinue = copyDirectoryRecursively(*vecIt, my);
  }

  return doContinue;
}

void copyDirectoryRecursivelyWrapper(fs::path fromDir, fs::path toDir, Deployka::CollectMethod cm) {

  MyCopyator mc;
  mc.destDirectory = toDir;
  mc.srcRootDirectory = fromDir;
  mc.method = cm;

  copyDirectoryRecursively(fromDir, mc);
}

/*
 * @brief Process artifacts vector
 * @param[in] anArtifacts Vector of artifacts to process
 * @param[in] cm Collect method
*/
void processArtifacts(std::vector<Deployka::Artifact> & anArtifacts, Deployka::CollectMethod cm) {
  for (auto& item : anArtifacts) {

    if (!fs::exists(item.storeDir) || !fs::is_directory(item.storeDir)) {
      std::cout << "[W] Store directory " << item.storeDir << " is not exists!\n";
      fs::create_directories(item.storeDir);
    }

    for (auto & dep : item.dependencies) {
      if (!fs::exists(dep.path)) {
        std::cout << "[W] Source path " << dep.path << " is not exists!\n";
        continue;
      }
      switch (dep.type) {
      case Deployka::DDT_pattern: {

        fs::directory_iterator dirIt = fs::directory_iterator(dep.path);
        fs::directory_iterator endDirIt = fs::directory_iterator();
        for (; dirIt != endDirIt; ++dirIt) {
          std::string fileName = dirIt->path().filename().string();

          try {
            std::regex aRegex(dep.pattern);
            std::smatch results;
            bool isMatch = std::regex_match(fileName, results, aRegex);
            if (isMatch) {
              fs::path result = fs::path(item.storeDir) / dirIt->path().filename();
              if (fs::exists(result)) {
                if (cm == Deployka::DCM_replaceIfNewer) {
                  ::replaceIfNewer(dirIt->path(), result);
                }
                else {
                  std::cout << "[W] Already exists: " << result << "\n";
                  continue;
                }
              }
              else {
                fs::copy_file(dirIt->path(), result);
              }
            }
          }
          catch (std::regex_error& re) {
            std::cerr << "[W] in processArtifacts pattern: " << dep.pattern << "; artifact: " << item.name << '\n';
            std::cerr << "[W] " << re.what() << '\n';
            break;
          }
        }
      }
      break;
      case Deployka::DDT_directory: {
        fs::path result = fs::path(item.storeDir) / fs::path(dep.path).filename();
        if (!fs::exists(result)) {
          fs::create_directories(result);
        }
        copyDirectoryRecursivelyWrapper(dep.path, result, cm);
      }
      break;
      case Deployka::DDT_file:
      default: {
        fs::path result = fs::path(item.storeDir) / fs::path(dep.path).filename();
        if (fs::exists(result)) {
          if (cm == Deployka::DCM_replaceIfNewer) {
            ::replaceIfNewer(dep.path, result);
          }
          else {
            std::cout << "[W] Already exists: " << result << "\n";
            break;
          }
        }
        else {
          fs::copy_file(dep.path, result);
        }
      }
      break;
      }
    }
  }
}

#ifdef _TESTS_
void TEST_regex() {
  std::string simpleId = "blah blah $some_Identifier bpowjentjb";

  std::regex re1{ R"-(\$\w[_9a-zA-Z]*)-" };

  std::smatch matchResults;
  bool matchRes = std::regex_search(simpleId, matchResults, re1);

  std::cout << "match res (" << matchRes << "): " << matchResults[0].str() << '\n';

  std::regex re2{ R"-(\$\([^)]*\))-" };

  std::string p11dId = "blah blahe$($wasya)eworn poen";

  matchRes = std::regex_search(p11dId, matchResults, re2);

  std::cout << "match res (" << matchRes << "): " << matchResults[0].str() << '\n';
}

void TEST_filesystemRecursiveDirIterator() {
  fs::path rootDirPath = R"-(E:\test\file_set)-";

  if (!fs::exists(rootDirPath)) {
    throw std::logic_error("Root directory not exists!");
  }

  fs::recursive_directory_iterator rdIt = fs::recursive_directory_iterator(rootDirPath);
  fs::recursive_directory_iterator rdItEnd = fs::recursive_directory_iterator();

  for (; rdIt != rdItEnd; ++rdIt) {
    std::cout << (fs::is_directory(rdIt->status()) ? "dir:  " : "file: ") << rdIt->path();
    if (fs::is_regular_file(rdIt->status())) {

      char charBuf[120] = {};
      time_t lwt = fs::last_write_time(rdIt->path());
      struct tm tms = {};
      auto err = localtime_s(&tms, &lwt);
      if (err) {
        throw std::logic_error("Cannot convert localtime");
      }
      strftime(charBuf, 120, "%d.%m.%Y %H:%M", &tms);
      //std::transform(charBuf, charBuf + 120, charBuf, [](char c) { if (c == '\n' || c == '\r') { return ' '; }; return c; });

      std::cout << "; last write time: " << charBuf;

      //       drivers\etc\hosts      c:\windows\system32\drivers\etc\hosts c:\windows\system32
      fs::path relPath = fs::relative(rdIt->path(),                         rootDirPath);

      std::cout << "; rel path: " <<  relPath;
      relPath.parent_path();
    }
    std::cout << '\n';
  }
}

void TEST_copyDirRecursively() {
  fs::path srcRoot = R"-(E:\prog\web\web-chat\trunk)-";
  fs::path dest = R"-(E:\test\VMShared\web_chat)-";

  copyDirectoryRecursivelyWrapper(srcRoot, dest, Deployka::DCM_cleanup);

  copyDirectoryRecursivelyWrapper(srcRoot, dest, Deployka::DCM_replaceIfNewer);

}

void TEST_substVars() {
  std::string example = 
R"str(<Dependency>$SrcRoot\VI\WebClient\Backend\WebServer\$MemModel\$Variant\WebServer.exe</Dependency>
    <Dependency>$SrcRoot\LicServer\DlLicModuleShell\$MemModel\$Variant\LicModShell.exe</Dependency>
    <SourceDirectory>$($RemoteDiskLetter):\Configs</SourceDirectory>
    <Dependency>$($RemoteDiskLetter):\DL80\SecServerConsole\BinVI\$Variant\$MemModel\ConsoleVi.exe</Dependency>
    <Dependency>$SrcRoot\DL80\SecServerConsole\BinVI\$($Variant)Vi\$MemModel\ServerConsoleVi.exe</Dependency>)str";

  std::map<std::string, std::string> varsDict = {
    {"memmodel", "x86"},
    {"remotediskletter", "E"},
    {"srcroot", R"-(E:\Files\System32\Drivers\etc\hosts)-"},
    {"variant", "Debug"}
  };

  std::string res = substVars(varsDict, example);

  std::cout << "result: " << res;
}
#endif

/*!
 * @brief fill targets info from XML
 * @param[in] configFile path to XML file
 * @return vector of targets filled from XML file
*/
std::vector<Deployka::Artifact> loadTargetsFromXML(std::string const& configFile) {

  std::vector<Deployka::Artifact> result;
  pt::ptree aTree;

  try {

    pt::read_xml(configFile, aTree);

    for (pt::ptree::value_type& targetNode : aTree.get_child("Root.Targets")) {

      Deployka::Artifact aTarget;

      aTarget.name = targetNode.second.get<std::string>("Name");
      aTarget.storeDir = targetNode.second.get<std::string>("PathToDirectory");
      for (pt::ptree::value_type& dependencyNode : targetNode.second.get_child("Dependencies")) {

        Deployka::ArtifactDependency aDependency;
        aDependency.type = Deployka::DDT_none;

        if (dependencyNode.second.count("<xmlattr>")) { // <Dependency type="pattern">...</Dependency>

          pt::ptree::value_type& xmlAttrVal = dependencyNode.second.get_child("<xmlattr>").front();

          if (xmlAttrVal.first == "type" && xmlAttrVal.second.data() == "pattern") {
            std::string rootDirStr = dependencyNode.second.get<std::string>("RootDir");
            std::string patternStr = dependencyNode.second.get<std::string>("Pattern");
            //std::out << "dependency is a pattern: " << patternStr << " with root dir: " << rootDirStr << "\n";
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

        if (dependencyNode.first == "Dependency" && aDependency.type != Deployka::DDT_none) {
          aTarget.dependencies.emplace_back(aDependency);
        }
      }

      result.emplace_back(std::move(aTarget));
    }
  }
  catch (std::runtime_error& re) {
    // details of the exception will be shown in outer context
    std::cerr << "[E] in loadTargetsFromXML configFile: " << configFile << '\n';
    throw re;
  }

  return result;
}

//====================================================================
int main(int argc, char * argv[]) {

  std::ios::sync_with_stdio(false);

#ifdef _TESTS_
  argc;
  argv;
  try {
    //TEST_regex();
    //TEST_substVars();
    //TEST_filesystemRecursiveDirIterator();
    TEST_copyDirRecursively();
  }
  catch (fs::filesystem_error& fserr) {
    std::cerr << fserr.what();
  }
  catch (std::runtime_error& e) {
    std::cerr << e.what();
  }
#else
  std::string configFile;
  std::vector<std::string> requestedTargets;

  try {
  // setup command line options
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
      //po::value<std::vector<std::string>>(&requestedTargets)->default_value({"Frontend", "WebServer"}),
      po::value<std::vector<std::string>>(&requestedTargets)->default_value({"Frontend", "WebServer", "ViCoreService"}),
      "Which target from config to collected"
    )
    // TODO add collect method parameter
  ;

  // process command line options
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

  // load targets from XML
  std::vector<Deployka::Artifact> targets = loadTargetsFromXML(configFile);
  std::vector<Deployka::Artifact> toProcess;

  for (std::string const& target : requestedTargets) {
    auto foundIt = std::find_if(targets.begin(), targets.end(), [&target](Deployka::Artifact artifact) { return artifact.name == target; });
    if (foundIt == targets.end()) {
      std::cout << "[W] Requested target " << target << " not found in XML\n";
      continue;
    }
    toProcess.push_back(*foundIt);
  }

  // load settings from XML
  std::map<std::string, std::string> varsDict = Deployka::loadSettingsFromXML(configFile);
  varsDict["memmodel"] = std::to_string(vm["mem-model"].as<int>());
  varsDict["srcroot"] = vm["src-root"].as<std::string>();
  varsDict["variant"] = vm["variant"].as<std::string>();

  // substitute variables in targets from XML
  auto varsMapIt = varsDict.begin();
  for (; varsMapIt != varsDict.end(); ++varsMapIt) {
    varsMapIt->second = Deployka::substVars(varsDict, varsMapIt->second);
  }

  for (Deployka::Artifact& target : toProcess) {

    target.storeDir = Deployka::substVars(varsDict, target.storeDir);

    for (Deployka::ArtifactDependency& dependency : target.dependencies) {
      dependency.path = Deployka::substVars(varsDict, dependency.path);
      if (!dependency.pattern.empty()) {
        dependency.pattern = Deployka::substVars(varsDict, dependency.pattern);
      }
    }
  }

  // process artifacts
  processArtifacts(toProcess, Deployka::DCM_replaceIfNewer);

  std::cout << "Done!" << std::endl;

  } // /TRY
  catch(std::exception& e) {
      std::cerr << "[E] in main: " << e.what() << "\n";
      return 1;
  }
  catch(...) {
      std::cerr << "[E] Exception of unknown type!\n";
      return 2;
  }
#endif

  return 0;
}
