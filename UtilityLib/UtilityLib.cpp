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
 * @brief Read settings section from XML file. Supports list setting values
 * @param[in] configFile Path to XML file
 * @return Settings names and values
*/
std::multimap<std::string, std::string> loadSettingsFromXML2(std::string const& configFile) {

  std::multimap<std::string, std::string> result;
  pt::ptree aTree;

  try {

    pt::read_xml(configFile, aTree);

    /// settingNode here is <Setting><Name>...</Name><Value>...</Value></Setting> subtree
    /// including <xmlattr> in <Setting> element
    for (pt::ptree::value_type& settingNode : aTree.get_child("Root.Settings")) {

      std::string name = settingNode.second.get<std::string>("Name"); //< get text inside <Name> element in <Setting>
      str_to_lower(name);

      /// valueSubtree here is <Value>...</Value> subtree including 
      /// including <xmlattr> in <Value> element
      pt::ptree valueSubtree = settingNode.second.get_child("Value");

      //std::cout << "xml attributes:\n";
      if (valueSubtree.count("<xmlattr>")) {
        //pt::ptree xmlattrSubtree = valueSubtree.get_child("<xmlattr>");

        /// first element in <xmlattr> subtree
        pt::ptree::value_type xmlattrNode = valueSubtree.get_child("<xmlattr>").front();

        /// xmlattrNode.second.data() and  xmlattrNode.second.get<std::string>("") is the same
        if (xmlattrNode.first == "type" && xmlattrNode.second.data() == "list") {
          /// treat <Value> subtree as list of string items
          for (pt::ptree::value_type& itemNode : valueSubtree) {
            if (itemNode.first != "Item") {
              continue;
            }

            //std::cout << itemNode.first << " => " << itemNode.second.data() << '\n';
            result.insert({ name, itemNode.second.data() });
          }
        }
        else {
          /// treat <Value> as single string value
          result.insert({ name, settingNode.second.get<std::string>("Value") });
        }
      }
      else {
        /// treat <Value> as single string value
        result.insert({ name, settingNode.second.get<std::string>("Value") });
      }
    }
  }
  catch (std::runtime_error& rte) {
    std::cerr << "Error in loadSettingsFromXML2: " << rte.what();
    throw rte;
  }

  return result;
}

/*!
 * @brief Read settings section from XML file
 * @param[in] configFile Path to XML file
 * @return Settings names and values
*/
std::map<std::string, std::string> loadSettingsFromXML(std::string const& configFile) {

  std::map<std::string, std::string> result;
  pt::ptree aTree;

  try {

    pt::read_xml(configFile, aTree); // throws xml_parser_error

    for (pt::ptree::value_type & settingNode : aTree.get_child("Root.Settings")) {

      std::string name = settingNode.second.get<std::string>("Name");
      str_to_lower(name);
      std::string value = settingNode.second.get<std::string>("Value");

      result.insert({ name, value });
    }
  }
  catch (std::runtime_error& e) {
    std::cerr << "ERROR in loadSettingsFromXML: " << e.what();
    throw e;
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

/*!
* @brief substitudes the value of the variable where the name of the variable occurs with leading '$'
*        and reparses text inside $(...) (to separate variable names with plain text $($arch)bit => 64bit)
* @param[in] varsDict vars and their values
* @param[in] inStr string to substitude to
* @return string with all found variables substituted
*/
std::string substVars2(std::multimap<std::string, std::string> const & varsDict, std::string & inStr) {

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

    std::string tmpRes = substVars2(varsDict, tmp);

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

    std::pair<std::multimap<std::string, std::string>::const_iterator, std::multimap<std::string, std::string>::const_iterator> k;

    // append first founded item with "tmp" key
    k = varsDict.equal_range(tmp);
    result.append(k.first->second);
    //result.append(varsDict.at(tmp));
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

