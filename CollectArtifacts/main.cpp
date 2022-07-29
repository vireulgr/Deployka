#include <iostream>
#include <string>
#include <exception>

#include <boost/program_options.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>


struct Target {
  std::string m_name;
  std::string m_pathToDir;
  std::vector<std::string> m_dependencies;
};

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

std::ostream& operator << (std::ostream& os, Target const& aTarget) {
  os << aTarget.m_name.c_str() << " (" << aTarget.m_pathToDir.c_str() << ")\n";
  os << aTarget.m_dependencies << '\n';
  return os;
}

std::vector<Target> g_targets;

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
// Если там какие нибудь glob параметры или параметры типа "всю папку мне запили" то это необходимо преобразовать к просто списку
//     фйалов для обработки
//
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
//

namespace pt = boost::property_tree;
namespace po = boost::program_options;

int main(int argc, char * argv[]) {

  std::string configFile;

  try {

  po::options_description desc("Collect Artifacts command line arguments");
  desc.add_options()
    ("help", "print help message")
    ("variant,V",   po::value<std::string>(),              "debug or release")
    ("mem-model,M", po::value<int>(),                      "32 or 64")
    ("config,c",    po::value<std::string>(&configFile)->default_value("./BuildArtifacts.xml"), "path to artifacts description file")
    ("src-root,r",  po::value<std::string>(),              "Path to directory that will be substituted as SrcRoot in artifacts description")
    ("target,T",    po::value<std::vector<std::string>>(), "Which target from config to collected")
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

  if (vm.count("config")) {
    std::cout << "config path: " << vm["config"].as<std::string>() << std::endl;
  }

  if (vm.count("src-root")) {
    std::cout << "source root: " << vm["src-root"].as<std::string>() << std::endl;
  }

  if (vm.count("target")) {
    std::vector<std::string> vec = vm["target"].as<std::vector<std::string>>();

    std::cout << "targets:     " << vec << std::endl;
  }

  /// LOAD XML and parse targets

  pt::ptree aTree;

  pt::read_xml(configFile, aTree);

  for (pt::ptree::value_type & v : aTree.get_child("Root.Targets")) {

    Target aTarget;

    aTarget.m_name = v.second.get<std::string>("Name");
    aTarget.m_pathToDir = v.second.get<std::string>("PathToDirectory");
    for (pt::ptree::value_type& v2 : v.second.get_child("Dependencies")) {
      //size_t srcStrSize = v2.first.size();
      //std::unique_ptr<wchar_t[]> tmpWCharBuf = std::make_unique<wchar_t[]>(srcStrSize);
      //std::mbstowcs(tmpWCharBuf.get(), v2.first.c_str(), srcStrSize);

      //std::cout
      //  << "f: " << v2.first << "; "
      //  << "s: " << v2.second.data() << "; "
      //  << "xmlattrs: " << v2.second.count("<xmlattr>") << "\n";

      if (v2.second.count("<xmlattr>")) {
        std::cout
          << "rd: " << v2.second.count("RootDir") << "; "
          << "pat: " << v2.second.count("Pattern") << ";\n";

        pt::ptree::value_type & xmlAttrVal = v2.second.get_child("<xmlattr>").front();
        //pt::ptree::value_type & xmlRootDir = v2.second.get_child("RootDir").front();
        //pt::ptree::value_type & xmlPattern = v2.second.get_child("Pattern").front();

        std::cout << xmlAttrVal.first << " " << xmlAttrVal.second.data() << '\n';
        //std::cout << xmlRootDir.first << "\n";
        //std::cout << xmlPattern.first << "\n";
      }

      if (v2.first == "Dependency") {
        aTarget.m_dependencies.emplace_back(v2.second.data());
      }
    }

    //std::cout << aTarget << '\n';
    g_targets.emplace_back(std::move(aTarget));
  }


  } // /TRY
  catch(std::exception& e) {
      std::cerr << "error: " << e.what() << "\n";
      return 1;
  }
  catch(...) {
      std::cerr << "Exception of unknown type!\n";
  }

  return 0;
}
