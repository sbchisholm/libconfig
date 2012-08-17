#ifndef _libconfig_printing_included_
#define _libconfig_printing_included_

#include "Types.h"

#include <iostream>
#include <sstream>
#include <boost/algorithm/string/join.hpp>

namespace libconfig {
  namespace printing {
   
    // ========================================================================
    // Globals
    int const tabsize = 2;

    // Free function to print spaces to match the indent
    void tab(int indent)
    {
        for (int i = 0; i < indent; ++i)
            std::cout << ' ';
    }

    // Convert a value to a string using the output stream << operator
    template<typename T>
    std::string valueToString(T const& t)
    {
      std::ostringstream oss;
      oss << t;
      return oss.str();
    }
    
    // Specialization of valueToString to print boolean values
    std::string valueToString(bool const& b)
    {
      std::ostringstream oss;
      b ? oss << "true" : oss << "false";
      return oss.str();
    }

    // Specialization of valueToString to print string values
    std::string valueToString(std::string const& t)
    {
      std::ostringstream oss;
      oss << "\"" << t << "\"";
      return oss.str();
    }

    // ========================================================================
    // Function object for printing the contents of a ConfigType object
    struct ConfigPrinter
    {
      ConfigPrinter(int indent = 0)
        : indent(indent)
      { }

      void operator()(ConfigType const& conf) const;

      int indent;
    };

    // ========================================================================
    // boost visitor for printing the variant type in the ConfigType's mapped
    // type
    struct ConfigItemPrinter : boost::static_visitor<>
    {
      public:
        // :: -----------------------------------------------------------------
        // :: Construction

        ConfigItemPrinter(int indent = 0)
          : indent(indent)
        { }

      public:
        // :: -----------------------------------------------------------------
        // :: Public Interface

        // Print a single value
        template<typename T>
        void operator()(T const& t) const
        {
          std::cout << " = " << valueToString(t) << ";" << std::endl;
        }

        // Print a list of values
        template<typename T>
        void operator()(std::vector<T> const& t) const
        {
          std::cout << " = (";
          std::vector<std::string> values;
          BOOST_FOREACH(const T& s, t) { 
            values.push_back(valueToString(s)); 
          }
          std::cout << boost::algorithm::join(values, ", ")
                    << ");" << std::endl;
        }

        // Print a section
        void operator()(ConfigType const& conf) const
        {
          std::cout << ": {" << std::endl;
          ConfigPrinter(indent+tabsize)(conf);
          tab(indent);
          std::cout << "};" << std::endl;
        }

      public:
        // :: ----------------------------------------------------------------
        // :: Members

        int indent;
    };

    // Definition of the ConfigPrinter::operator() used to print the contents of
    // a ConfigType object
    void ConfigPrinter::operator()(ConfigType const& conf) const
    {
        BOOST_FOREACH(ConfigType::value_type const& it, conf)
        {
            tab(indent);
            // print out the key or section header
            std::cout << it.first;  
            // 
            boost::apply_visitor(ConfigItemPrinter(indent), it.second);
        }
    }
  } // namespace printing
} // namespace libconfig

#endif // _libconfig_printing_included_
