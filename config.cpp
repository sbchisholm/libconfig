
// Credit: Based off of the mini_xml3.cpp example from the boost spirit 
//   qi examples. Joel de Guzman: (http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_VARIANT_NO_FULL_RECURSIVE_VARIANT_SUPPORT
#define BOOST_SPIRIT_DEBUG

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

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
#include <boost/variant/recursive_variant.hpp>
#include <boost/variant/get.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

namespace parse
{
    namespace fusion = boost::fusion;
    namespace phoenix = boost::phoenix;
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    struct config_struct;

    typedef std::string config_key;

    typedef 
        boost::make_recursive_variant<
            std::string
          , double
          , int
          , std::vector<std::string>
          , std::vector<double>
          , std::vector<int>
          , std::multimap<config_key, boost::recursive_variant_>
        >::type 
    config_tree;

    typedef std::multimap<config_key, config_tree> config_type;
    typedef std::pair<config_key, config_tree> config_pair;

}

namespace config {
    parse::config_type parse_config_file(std::string filename);
    std::string expand_includes(std::string base_dir, std::string filename);
}

namespace parse
{
    ///////////////////////////////////////////////////////////////////////////
    //  Print out the configuration tree
    ///////////////////////////////////////////////////////////////////////////
    int const tabsize = 2;

    void tab(int indent)
    {
        for (int i = 0; i < indent; ++i)
            std::cout << ' ';
    }

    template<typename T>
    std::string value_to_string(T const& t)
    {
      std::ostringstream oss;
      oss << t;
      return oss.str();
    }
    
    std::string value_to_string(std::string const& t)
    {
      std::ostringstream oss;
      oss << "\"" << t << "\"";
      return oss.str();
    }

    struct config_printer
    {
        config_printer(int indent = 0)
          : indent(indent)
        {
        }

        void operator()(config_type const& conf) const;

        int indent;
    };

    struct config_item_printer : boost::static_visitor<>
    {
        config_item_printer(int indent = 0)
          : indent(indent)
        {
        }

        template<typename T>
        void operator()(T const& t) const
        {
            std::cout << " = " << value_to_string(t) << ";" << std::endl;
        }

        template<typename T>
        void operator()(std::vector<T> const& t) const
        {
            std::cout << " = (";
            std::vector<std::string> values;
            BOOST_FOREACH(const T& s, t) { 
              values.push_back(value_to_string(s)); 
            }
            std::cout << boost::algorithm::join(values, ", ")
                      << ");" << std::endl;
        }

        void operator()(config_type const& conf) const
        {
            std::cout << ": {" << std::endl;
            config_printer(indent+tabsize)(conf);
            tab(indent);
            std::cout << "};" << std::endl;
        }

        int indent;
    };

    void config_printer::operator()(config_type const& conf) const
    {
        BOOST_FOREACH(config_type::value_type const& it, conf)
        {
            tab(indent);
            std::cout << it.first;  // Print out the key or section header
            boost::apply_visitor(config_item_printer(indent), it.second);
        }
    }
    
    BOOST_PHOENIX_ADAPT_FUNCTION(config_type, parse_config_file, config::parse_config_file, 1);
    BOOST_PHOENIX_ADAPT_FUNCTION(std::string, expand_includes, config::expand_includes, 1);
    
    // Define a skipper for white space and comments
    //template<typename Iterator>
    //struct config_skipper
    //  : qi::grammar<Iterator> 
    //{
    //    config_skipper() 
    //      : config_skipper::base_type(skip, "white space and comments") 
    //    {
    //        using qi::eol;
    //        using ascii::char_;

    //        skip = ( ascii::space )
    //             | ( "//" >> *(char_ - eol) >> eol )
    //        ;
    //    }
    //    qi::rule<Iterator> skip;
    //};
    template<typename Iterator>
    struct config_skipper
      : qi::grammar<Iterator> 
    {
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
                    << _4                               // what failed?
                    << val(" here: \"")
                    << construct<std::string>(_3, _2)   // iterators to error-pos, end
                    << val("\"")
                    << std::endl
            );
        }
        qi::rule<Iterator> skip;
        qi::rule<Iterator> comment;
    };

    BOOST_PHOENIX_ADAPT_FUNCTION(std::string, expand_includes, config::expand_includes, 2);

    template<typename Iterator, typename Skipper = config_skipper<Iterator> >
    struct include_grammar
      : qi::grammar<Iterator, std::string(), Skipper>
    {
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
                >   quoted_string [_val += expand_includes(base_dir, _1)]
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
                    << _4                               // what failed?
                    << val(" here: \"")
                    << construct<std::string>(_3, _2)   // iterators to error-pos, end
                    << val("\"")
                    << std::endl
            );

            //BOOST_SPIRIT_DEBUG_NODE(include);
            //BOOST_SPIRIT_DEBUG_NODE(quoted_string);
            //BOOST_SPIRIT_DEBUG_NODE(config_file);

        }

        qi::rule<Iterator, std::string(), Skipper> include;
        qi::rule<Iterator, std::string(), Skipper> quoted_string;
        qi::rule<Iterator, std::string(), Skipper> config_file;

        std::string base_dir;
    };


    ///////////////////////////////////////////////////////////////////////////
    //  Our config grammar definition
    ///////////////////////////////////////////////////////////////////////////
    template <typename Iterator, typename Skipper = config_skipper<Iterator> >
    struct config_grammar
      : qi::grammar<Iterator, config_type(), Skipper>
    {
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
            using qi::attr;
            using ascii::char_;
            using ascii::alnum;
            using ascii::space;
            using ascii::string;
            using namespace qi::labels;

            using phoenix::construct;
            using phoenix::val;

            config %=
                    /**include_pair
                >   */*item
            ;

            item %= 
                    section | key_value_pair
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
                >   ( unesc_str | double_ | int_ | quoted_string_list | double_list | int_list )
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
                    lexeme[+(alnum | '_')];

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

        qi::symbols<char const, char const> unesc_char;
        qi::rule<Iterator, std::string()> unesc_str;

        qi::rule<Iterator, config_type(), Skipper> config;
        qi::rule<Iterator, config_pair(), Skipper> item;
        qi::rule<Iterator, config_pair(), Skipper> key_value_pair;
        qi::rule<Iterator, std::pair<config_key, config_type>(), Skipper> section;

        qi::rule<Iterator, std::string(), Skipper> quoted_string;
        qi::rule<Iterator, std::vector<std::string>(), Skipper> quoted_string_list;
        qi::rule<Iterator, std::vector<double>(), Skipper> double_list;
        qi::rule<Iterator, std::vector<int>(), Skipper> int_list;

        qi::rule<Iterator, std::string(), Skipper> start_tag;
        qi::rule<Iterator, std::string(), Skipper> key;
        qi::rule<Iterator, void(), Skipper> end_tag;
    };
}
namespace config {
    std::string file_to_string(std::string filename)
    {
      std::ifstream in(filename.c_str(), std::ios_base::in);
    
      if (!in) {
          std::cerr << "Error: Could not open input file: "
                    << filename << std::endl;
          return "";
      }
    
      std::string storage; // We will read the contents here.
      in.unsetf(std::ios::skipws); // No white space skipping!
      std::copy(
          std::istream_iterator<char>(in),
          std::istream_iterator<char>(),
          std::back_inserter(storage));
    
      return storage;
    }
    
    std::string expand_includes(std::string base_dir, std::string filename)
    {
      boost::filesystem::path file_path;
      file_path /= base_dir;
      file_path /= filename; 

      std::string storage = file_to_string(file_path.string());
      std::string result;

      typedef parse::include_grammar<std::string::const_iterator> include_grammar;
      typedef parse::config_skipper<std::string::const_iterator> config_skipper;
      std::string::const_iterator iter = storage.begin();
      std::string::const_iterator end = storage.end();
      include_grammar g(file_path.parent_path().string()); 
      config_skipper s; // Our skipper
        
      std::string parent_path_string = file_path.parent_path().string();
      bool r = phrase_parse(iter, end, g, s, result);
      
      if (r && iter == end)
      {
          std::cout << "-------------------------\n";
          std::cout << "Parsing '" << filename << "' succeeded\n";
          std::cout << "-------------------------\n";
      }
      else
      {
          std::cout << "-------------------------\n";
          std::cout << "Parsing '" << filename << "' failed\n";
          std::cout << "-------------------------\n";
      }

      return result;
    }

    //std::string expand_includes(std::string filename)
    //{
    //  std::cout << "expand_includes(\"" << filename << "\")\n";
    //  std::string storage = file_to_string(filename);
    //  std::string result;

    //  typedef parse::include_grammar<std::string::const_iterator> include_grammar;
    //  std::string::const_iterator iter = storage.begin();
    //  std::string::const_iterator end = storage.end();
    //  include_grammar ig;
    //  bool r = phrase_parse(iter, end, ig, result);
    //  
    //  if (r && iter == end)
    //  {
    //      std::cout << "-------------------------\n";
    //      std::cout << "Parsing '" << filename << "' succeeded\n";
    //      std::cout << "-------------------------\n";
    //  }
    //  else
    //  {
    //      std::cout << "-------------------------\n";
    //      std::cout << "Parsing '" << filename << "' failed\n";
    //      std::cout << "-------------------------\n";
    //  }

    //  return result;
    //}
    
    parse::config_type parse_config_file(std::string filename)
    {
      parse::config_type configuration;
    
      //std::string storage = file_to_string(filename);
      std::string storage = config::expand_includes(boost::filesystem::current_path().string(),  filename);
    
      typedef parse::config_grammar<std::string::const_iterator> config_grammar;
      typedef parse::config_skipper<std::string::const_iterator> config_skipper;
    
      std::string::const_iterator iter = storage.begin();
      std::string::const_iterator end = storage.end();
      config_grammar cg; // Our grammar
      config_skipper skipper; // Our skipper
      bool r = phrase_parse(iter, end, cg, skipper, configuration);
      
      if (r && iter == end)
      {
          std::cout << "-------------------------\n";
          std::cout << "Parsing '" << filename << "' succeeded\n";
          std::cout << "-------------------------\n";
      }
      else
      {
          std::cout << "-------------------------\n";
          std::cout << "Parsing '" << filename << "' failed\n";
          std::cout << "-------------------------\n";
      }
    
      return configuration;
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Main program
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    char const* filename;
    if (argc > 1)
    {
        filename = argv[1];
    }
    else
    {
        std::cerr << "Error: No input file provided." << std::endl;
        return (1);
    }

    parse::config_type configuration = config::parse_config_file(filename);
    parse::config_printer()(configuration);
    return (0);
}


