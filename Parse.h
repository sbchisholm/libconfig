#ifndef _libconfig_parse_included_
#define _libconfig_parse_included_

#include "Types.h"

#include <fstream>

#define BOOST_SPIRIT_DEBUG

#define BOOST_SPIRIT_USE_PHOENIX_V3
#include <boost/phoenix/function/adapt_callable.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_attr.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/filesystem.hpp>


namespace libconfig {
  namespace parse {
    
    namespace fusion = boost::fusion;
    namespace phoenix = boost::phoenix;
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    // Forward declaration of the expand includes function for boost phoenix
    // adapt
    std::string _expandIncludes(std::string, std::string);
    
    // Adapt the expandIncludes funtion to with with boost phoenix, this allows
    // for calling the expandIncludes function in an boost spirit qi action.
    BOOST_PHOENIX_ADAPT_FUNCTION(std::string, expandIncludes, 
                                 _expandIncludes, 2);
    
    // =======================================================================
    // Grammar definition of the white space and comment skipper
    template<typename Iterator>
    struct config_skipper
      : qi::grammar<Iterator> 
    {
      // :: ------------------------------------------------------------------
      // :: Construction

      config_skipper() 
        : config_skipper::base_type(skip, "white space and comments") 
      {
        using qi::eol;
        using ascii::char_;
        using ascii::space;
        using qi::on_error;
        using qi::fail;
        using namespace qi::labels;
        using phoenix::construct;
        using phoenix::val;

        skip = space | comment;

        comment = "//" > *((space|char_) - eol) > eol;
        
        on_error<fail>
        (
            skip
          , std::cout
                << val("Error! Expecting ")
                << _4                             // what failed?
                << val(" here: \"")
                << construct<std::string>(_3, _2) // iterators to error-pos, end
                << val("\"")
                << std::endl
        );
      }

      // :: -------------------------------------------------------------------
      // :: Members

      qi::rule<Iterator> skip;
      qi::rule<Iterator> comment;
    };


    // ========================================================================
    // Grammar definition for parsing the include files
    template<typename Iterator, typename Skipper = config_skipper<Iterator> >
    struct include_grammar
      : qi::grammar<Iterator, std::string(), Skipper>
    {
      // :: ------------------------------------------------------------------
      // :: Construction
      
      include_grammar(std::string _base_dir)
        : include_grammar::base_type(config_file, "expand the includes")
        , base_dir(_base_dir)
      {
        using qi::lit;
        using qi::skip;
        using qi::eol;
        using qi::lexeme;
        using qi::no_skip;
        using qi::on_error;
        using qi::fail;
        using ascii::space;
        using ascii::char_;
        using namespace qi::labels;
        using phoenix::construct;
        using phoenix::val;

        quoted_string %= 
                lexeme['"' >> +(char_ - '"') >> '"'] 
        ;

        include = 
                lit("#include")       
            >   quoted_string [_val += expandIncludes(base_dir, _1)]
        ;

        config_file %= 
                *include
            >>  no_skip[*char_]
        ;

        include.name("include");
        quoted_string.name("quoted_string");
        config_file.name("config_file");
        
        on_error<fail>
        (
            config_file
          , std::cout
                << val("Error! Expecting ")
                << _4                             // what failed?
                << val(" here: \"")
                << construct<std::string>(_3, _2) // iterators to error-pos, end
                << val("\"")
                << std::endl
        );

        // Uncomment to disable debugging
        //BOOST_SPIRIT_DEBUG_NODE(include);
        //BOOST_SPIRIT_DEBUG_NODE(quoted_string);
        //BOOST_SPIRIT_DEBUG_NODE(config_file);

      }

      // :: -------------------------------------------------------------------
      // :: Members

      qi::rule<Iterator, std::string(), Skipper> include;
      qi::rule<Iterator, std::string(), Skipper> quoted_string;
      qi::rule<Iterator, std::string(), Skipper> config_file;

      std::string base_dir;
    };


    // ========================================================================
    // Grammar definition for parsing the configuration format
    template <typename Iterator, typename Skipper = config_skipper<Iterator> >
    struct config_grammar
      : qi::grammar<Iterator, ConfigType(), Skipper>
    {
      // :: ------------------------------------------------------------------
      // :: Construction

      config_grammar()
        : config_grammar::base_type(config, "config")
      {
        using qi::skip;
        using qi::eol;
        using qi::no_skip;
        using qi::lit;
        using qi::lexeme;
        using qi::on_error;
        using qi::fail;
        using qi::double_;
        using qi::int_;
        using qi::true_;
        using qi::false_;
        using qi::attr;
        using ascii::char_;
        using ascii::alnum;
        using ascii::space;
        using ascii::string;
        using namespace qi::labels;

        using phoenix::construct;
        using phoenix::val;
        using phoenix::at_c;

        config %=
                *item
        ;

        item %= 
                ( section | key_value_pair | include_section )
        ;

        start_tag %=
                key
            >> !lit('=')
            >   lit(':') 
            >   lit('{')
        ;

        key_value_pair %=
                key
            >> !lit(':')
            >   lit('=')
            >   ( unesc_str | double_ | int_ | quoted_string_list | 
                  double_list | int_list | bool_type | empty_list )
            >   lit(';')
        ;

        end_tag =
                lit('}')
            >   lit(';')
        ;

        section %=
                start_tag
            >  *item
            >   end_tag
        ;

        unesc_char.add("\\a", '\a')("\\b", '\b')("\\f", '\f')("\\n", '\n')
                      ("\\r", '\r')("\\t", '\t')("\\v", '\v')
                      ("\\\\", '\\')("\\\'", '\'')("\\\"", '\"')
        ;
         
        unesc_str = 
                '"'
            >>  skip(
                    no_skip[
                            '"' 
                        >>  *( space | ( "//" >> *(char_ - eol) >> eol ) ) 
                        >>  '"'
                    ] 
                )
                [
                    *( unesc_char | ("\\x" >> qi::hex) | ( char_ - ( char_('"') ) ) )
                ]
            >   '"'
        ;
        
        key %=
                lexeme[+char_("0-9a-zA-Z_")]
        ;

        quoted_string %= 
                unesc_str
        ;

        quoted_string_list %=
                lit('(')
            >>  unesc_str % ','
            >   lit(')')
        ;

        double_list %=
                lit('(')
            >>  double_ % ','
            >   lit(')')
        ;

        int_list %=
                lit('(')
            >>  int_ % ','
            >   lit(')')
        ;

        empty_list %= 
                lit('(') 
            >>  lit(')')
        ;

        include_section %= 
                attr("$references") 
            >>  include_section_pair
        ;

        include_section_pair =
                lit("#include_section")
                // Store the attr in the second position of the tuple
            >   quoted_string [at_c<1>(_val) = _1]  
            >   lit("as")
                // store the attr in the first postion of the tuple
            >   quoted_string [at_c<0>(_val) = _1] 
        ;

        bool_type %=
                true_ | false_
        ;

        config.name("config");
        section.name("section");
        item.name("item");
        key.name("key");
        key_value_pair.name("key_value_pair");
        quoted_string.name("quoted_string");
        quoted_string_list.name("quoted_string_list");
        double_list.name("double_list");
        int_list.name("int_list");
        start_tag.name("start_tag");
        end_tag.name("end_tag");
        include_section.name("include_section");
        include_section_pair.name("include_section_pair");
        bool_type.name("bool_type");
        empty_list.name("empty_list");

        on_error<fail>
        (
            config
          , std::cout
                << val("Error! Expecting ")
                << _4                               // what failed?
                << val(" here: \"")
                << construct<std::string>(_3, _2)   // iterators to error-pos, end
                << val("\"")
                << std::endl
        );


          //BOOST_SPIRIT_DEBUG_NODE(key_value_pair);
          //BOOST_SPIRIT_DEBUG_NODE(section);
      }
      
      // :: -------------------------------------------------------------------
      // :: Members

      qi::symbols<char const, char const> unesc_char;
      qi::rule<Iterator, std::string()> unesc_str;

      qi::rule<Iterator, ConfigType(), Skipper> config;
      qi::rule<Iterator, ConfigPair(), Skipper> item;
      qi::rule<Iterator, ConfigPair(), Skipper> key_value_pair;
      qi::rule<Iterator, std::pair<ConfigKey, ConfigType>(), Skipper> section;

      qi::rule<Iterator, std::pair<ConfigKey, ConfigType>(), Skipper> include_section;
      qi::rule<Iterator, ConfigPair(), Skipper> include_section_pair;

      qi::rule<Iterator, std::string(), Skipper> quoted_string;
      qi::rule<Iterator, std::vector<std::string>(), Skipper> quoted_string_list;
      qi::rule<Iterator, std::vector<double>(), Skipper> double_list;
      qi::rule<Iterator, std::vector<int>(), Skipper> int_list;
      qi::rule<Iterator, std::vector<boost::none_t>(), Skipper> empty_list;
      qi::rule<Iterator, bool(), Skipper> bool_type;

      qi::rule<Iterator, std::string(), Skipper> start_tag;
      qi::rule<Iterator, std::string(), Skipper> key;
      qi::rule<Iterator, void(), Skipper> end_tag;
    };
    
    // ========================================================================
    // Open the file named <filename> and return to the contents of the file as
    // an std::string
    std::string fileToString(std::string filename)
    {
      std::ifstream in(filename.c_str(), std::ios_base::in);
    
      if (!in) {
          throw std::runtime_error("Could not open input file.");
      }
    
      std::string storage; // We will read the contents here.
      in.unsetf(std::ios::skipws); // No white space skipping!
      std::copy(
          std::istream_iterator<char>(in),
          std::istream_iterator<char>(),
          std::back_inserter(storage));
    
      return storage;
    }
    
    // ========================================================================
    // Given the config file return a string representation of the file with 
    // all of the includes expanded.
    std::string _expandIncludes(std::string baseDir, std::string filename)
    {
      boost::filesystem::path filePath;
      filePath /= baseDir;
      filePath /= filename; 

      std::string storage = fileToString(filePath.string());
      std::string result;

      typedef include_grammar<std::string::const_iterator> include_grammar;
      typedef config_skipper<std::string::const_iterator> config_skipper;

      std::string::const_iterator iter = storage.begin();
      std::string::const_iterator end = storage.end();
      include_grammar grammar(filePath.parent_path().string());
      config_skipper skipper;
      bool r = phrase_parse(iter, end, grammar, skipper, result);
      
      if (not r or iter != end)
        throw std::runtime_error("Parsing Includes Failed");

      return result;
    }

    // ========================================================================
    // Parse the config file in to a ConfigType object
    ConfigType parseConfigFile(std::string filename)
    {
      ConfigType configuration;
    
      std::string storage = 
          boost::filesystem::path(filename).root_directory().empty()
        ? _expandIncludes(boost::filesystem::current_path().string(), filename)
        : _expandIncludes("", filename);
    
      typedef config_grammar<std::string::const_iterator> config_grammar;
      typedef config_skipper<std::string::const_iterator> config_skipper;
    
      std::string::const_iterator iter = storage.begin();
      std::string::const_iterator end = storage.end();
      config_grammar grammar; 
      config_skipper skipper; 
      bool r = phrase_parse(iter, end, grammar, skipper, configuration);
      
      if (not r or iter != end)
        throw std::runtime_error("Parsing Configuration Failed");

      return configuration;
    }

  } // namespace parse
} // namespace libconfig


#endif // _libconfig_parse_included_
