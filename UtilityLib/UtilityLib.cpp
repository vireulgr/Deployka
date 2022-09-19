#include "UtilityLib.h"

#include <algorithm>
#include <regex>
#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace pt = boost::property_tree;

namespace Deployka {
/*!
 * @brief apply tolower to each char in string
 * @param [in] in string to lowercase
 * @return same string but lowercased
 */
inline std::string & str_to_lower(std::string & in) {
  std::transform(in.begin(), in.end(), in.begin(), [](char c) { return (char)std::tolower(c); });
  return in;
}

/*!
 * @brief Read settings section from XML file
 * @param[in] configFile Path to XML file
 * @return Settings names and values
*/
std::map<std::string, std::string> loadSettingsFromXML(std::string const& configFile) {

  std::map<std::string, std::string> result;
  pt::ptree aTree;

  pt::read_xml(configFile, aTree);

  for (pt::ptree::value_type & targetNode : aTree.get_child("Root.Settings")) {

    std::string name = targetNode.second.get<std::string>("Name");
    str_to_lower(name);
    std::string value = targetNode.second.get<std::string>("Value");

    result.insert({ name, value });
  }

  return result;
}

/*!
* @brief substitudes the value of the variable where the name of the variable occurs with leading '$'
*        and reparses text inside $(...) (to separate variable names with plain text $($arch)bit => 64bit)
* @param[in] varsDict vars and their values
* @param[in] inStr string to substitude to
* @return string with all found variables substituted
*/
std::string substVars(std::map<std::string, std::string> const & varsDict, std::string & inStr) {

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

    std::string tmpRes = substVars(varsDict, tmp);

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

    if (!varsDict.count(tmp)) {
      std::cout << "[W] Cannot find item " << tmp << " in dictionary!\n";
      result.append(item.first, item.second);
      continue;
    }

    result.append(varsDict.at(tmp));
  }

  if (matchResults.ready()) {
    result.append(matchResults.suffix().first, matchResults.suffix().second);
  }
  else if (result.empty()) {
    return inStr;
  }

  return result;
}
}

