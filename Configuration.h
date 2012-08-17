#ifndef _libconfig_configuration_included_
#define _libconfig_configuration_included_

#include "Types.h"
#include "Parse.h"
#include "Printing.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/regex.hpp>
#include <boost/format.hpp>

namespace libconfig {

  // ==========================================================================
  // Configuraiton class is the main interface to loading and parsing libconfig
  // files.
  class Configuration
  {
    private:
      struct FormatValue : boost::static_visitor<std::string>
      {
        public:
          // :: ---------------------------------------------------------------
          // :: Construction

          FormatValue(std::string formatString = "%1%")
            : m_formatString(formatString)
          {}

          // :: ---------------------------------------------------------------
          // :: Public Interface

          template<typename T>
          std::string operator()(T const& t) const
          {
            return boost::str( boost::format(m_formatString) % t );
          }
          
          template<typename T>
          std::string operator()(std::vector<T> const& t) const
          {
            throw std::runtime_error("String reference pointing to a list.");
          }

          std::string operator()(ConfigType const& t) const
          {
            throw std::runtime_error("String reference pointing to a section.");
          }

        private:
          // :: ---------------------------------------------------------------
          // :: Members

          std::string m_formatString;
      };

    public:
      // :: -------------------------------------------------------------------
      // :: Construction

      Configuration(const ConfigType& configurationMap)
        : m_configurationMap(configurationMap)
      {}

      Configuration(const std::string& configFilename)
        : m_configurationMap(parse::parseConfigFile(configFilename))
      {}

    public:
      // :: -------------------------------------------------------------------
      // :: Public Interface

      // Lookup a configuration item given the address and the value where the
      // item will be stored.  Returns 'true' or 'false' depending on if the 
      // item is found in the configuration.
      template<typename T>
      bool lookupValue(const std::string& address, T& value)
      {
        std::vector<std::string> keys;
        boost::split(keys, address, boost::is_any_of("."));
        return prv_lookupValue(m_configurationMap, value, keys);
      }

      void load(std::string configFilename)
      {
        m_configurationMap = parse::parseConfigFile(configFilename);
      }

      // Print the configuration to std::cout
      void print()
      {
        printing::ConfigPrinter()(m_configurationMap);
      }

    private:
      // :: ------------------------------------------------------------------
      // :: Private Member Functions

      // Retrieve a value from a map, this is used for the last key in the
      // configuration address.
      template<typename T>
      bool prv_getValue(const ConfigType& subConfig,
                        const std::string& key, T& value)
      {
        if(subConfig.find(key) == subConfig.end())
          return false;
        try {
          value = boost::get<T>(subConfig.find(key)->second);
        }
        catch(boost::bad_get e) {
          throw std::runtime_error("Type requested does not match"
                                   "the configuration item's type.");
        }
        return true;
      }
      
      // Specialization for lookupValue when the value type is a list, this is
      // to handle empty lists.
      template<typename T>
      bool prv_getValue(const ConfigType& subConfig,
                        const std::string& key, std::vector<T>& value)
      {
        if(subConfig.find(key) == subConfig.end())
          return false;
        try {
          value = boost::get<std::vector<T> >(
                          subConfig.find(key)->second);
        }
        catch(boost::bad_get) {
          try {
            boost::get<std::vector<boost::none_t> >(
                    subConfig.find(key)->second);
            value = std::vector<T>();
          }
          catch(boost::bad_get) {
            throw std::runtime_error("Type requested does not match"
                                     "the configuration item's type.");
          }
        }
        return true;
      }

      // Specialization for std::string values, this will look up any references
      // in the string values.
      bool prv_getValue(const ConfigType& subConfig,
                        const std::string& key, std::string& value)
      {
        if(subConfig.find(key) == subConfig.end())
          return false;
        try {
          value = prv_resolveReferences(
                          boost::get<std::string>(subConfig.find(key)->second));
        }
        catch(boost::bad_get e) {
          throw std::runtime_error("Type requested does not match"
                                   "the configuration item's type.");
        }
        return true;
      }

      bool prv_getValueAsString(const ConfigType& subConfig,
                                const std::string& key, std::string& value)
      {
        if(subConfig.find(key) == subConfig.end()) 
          return false;
        value = boost::apply_visitor(FormatValue(), subConfig.find(key)->second);
        return true;
      }

      // Recursive lookupValue function to traverse the configuration tree
      // searching for the configuration item specified in the address.
      template<typename T>
      bool prv_lookupValue(const ConfigType& subConfig,
                           T& value, const std::vector<std::string>& keys, 
                           size_t keysIdx = 0, bool convertToString = false)
      {
        // If the index is the last key in the configuration address, then get
        // the value of the config item.
        if(keys.size() == keysIdx + 1)
          return convertToString 
            ? prv_getValueAsString(subConfig, keys[keysIdx], value)
            : prv_getValue(subConfig, keys[keysIdx], value);
        
        // RECURSION: Lookup the key and attempt to access the next section.
        if(subConfig.find(keys[keysIdx]) != subConfig.end()) 
        {
          try {
            if(prv_lookupValue(
                 boost::get<ConfigType>(subConfig.find(keys[keysIdx])->second),
                 value, keys, keysIdx + 1)) 
            {
              return true;
            }
          }
          catch(boost::bad_get e) {
            throw std::runtime_error("The specified key is not a section");
          }
        }
         
        // Check for the address in an #include_section.
        boost::optional<std::vector<std::string> > includeSectionKeys = 
          prv_findIncludeSection(subConfig, keys[keysIdx]);
        if(includeSectionKeys) 
        { 
          return prv_lookupValue(m_configurationMap, value, 
                   prv_combineKeys(keys, keysIdx + 1, *includeSectionKeys));
        }
        else
        {
          return false;
        }
      }

      // Look for the give key in the #include_section.
      boost::optional<std::vector<std::string> > 
      prv_findIncludeSection(const ConfigType& subConfig, 
                             const std::string& key)
      {
        if(subConfig.find("$references") != subConfig.end()) 
        {
          std::string includeSection;
          std::vector<std::string> includeSectionKeys;
          if(prv_lookupValue(
               boost::get<ConfigType>(subConfig.find("$references")->second),
               includeSection, std::vector<std::string>(1, key)))
          {
            return boost::optional<std::vector<std::string> >(
                       boost::split(includeSectionKeys, 
                                    includeSection, 
                                    boost::is_any_of(".")));
          }
          else
          {
            return boost::none;
          }
        }
        return boost::none;
      }

      // Combine the keys from the two addresses.
      std::vector<std::string> 
      prv_combineKeys(std::vector<std::string> keys1, 
                      size_t keys1Idx, std::vector<std::string> keys2)
      {
        keys2.insert(keys2.end(), keys1.begin() + keys1Idx, keys1.end());
        return keys2;
      }

      // resolve any of the references found the string value.
      std::string prv_resolveReferences(std::string value)
      {
        boost::regex referencePattern("\\$\\{([\\w\\.]*)\\}");
        boost::sregex_iterator it(value.begin(), value.end(), referencePattern);
        boost::sregex_iterator end;
        for(/**/; it != end; ++it) 
        {
          std::string address((*it)[1].first, (*it)[1].second);
          std::string resolvedValue;
          std::vector<std::string> keys;
          boost::split(keys, address, boost::is_any_of("."));

          if(not prv_lookupValue(
                     m_configurationMap, resolvedValue, keys, 0, true))
          {
            char* env;
            env = std::getenv(address.c_str());
            if(env == NULL) {
              throw std::runtime_error(boost::str(boost::format(
                      "Unable to resolve reference '%1%' in string value.")
                      % address));
            }
            resolvedValue = env;
          }
          value.replace(it->position(), it->length(), resolvedValue);
        }
        return value;
      }

    private:
      // :: ------------------------------------------------------------------
      // :: Members

      ConfigType m_configurationMap;
  };

} // namespace libconfig

#endif // _libconfig_configuration_included_
