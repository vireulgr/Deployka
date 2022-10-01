#ifndef __UTILITY_LIB_H__
#define __UTILITY_LIB_H__
#include <string>
#include <map>

namespace Deployka {
  std::string substVars(std::map<std::string, std::string> const & varsDict, std::string & inStr);
  std::string substVars2(std::multimap<std::string, std::string> const & varsDict, std::string & inStr);

  std::map<std::string, std::string> loadSettingsFromXML(std::string const& configFile);
  std::multimap<std::string, std::string> loadSettingsFromXML2(std::string const& configFile);

  inline std::string & str_to_lower(std::string & in);
}
#endif
