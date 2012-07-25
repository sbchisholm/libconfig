
// Credit: Based off of the mini_xml3.cpp example from the boost spirit 
//   qi examples. Joel de Guzman: (http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/variant/get.hpp>
#include <boost/foreach.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

namespace parse
{
    namespace fusion = boost::fusion;
    namespace phoenix = boost::phoenix;
    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    ///////////////////////////////////////////////////////////////////////////
    //  Our config tree representation
    ///////////////////////////////////////////////////////////////////////////
    struct config_struct;

    typedef
        boost::variant<
            std::string
          , double
          , int
          , std::vector<std::string>
          , std::vector<double>
          , std::vector<int>
        >
    config_value;

    typedef 
        std::pair<std::string, config_value>
    config_key_value_pair;

    typedef
        boost::variant<
            boost::recursive_wrapper<config_struct>
          , config_key_value_pair
        >
    config_item;

    struct config_struct
    {
        std::string name;                           // config name
        std::vector<config_item> children;          // children
    };
}

// We need to tell fusion about our config struct
// to make it a first-class fusion citizen
BOOST_FUSION_ADAPT_STRUCT(
    parse::config_struct,
    (std::string, name)
    (std::vector<parse::config_item>, children)
)

namespace parse
{
    ///////////////////////////////////////////////////////////////////////////
    //  Print out the configuration tree
    ///////////////////////////////////////////////////////////////////////////
    int const tabsize = 4;

    void tab(int indent)
    {
        for (int i = 0; i < indent; ++i)
            std::cout << ' ';
    }

    struct config_printer
    {
        config_printer(int indent = 0)
          : indent(indent)
        {
        }

        void operator()(config_struct const& conf) const;

        int indent;
    };

    struct config_value_to_string : boost::static_visitor<std::string>
    {
        template<typename T>
        std::string operator()(T const& t) const
        {
            std::ostringstream oss;
            oss << t;
            return oss.str();
        }

        template<typename T>
        std::string operator()(std::vector<T> const& t) const
        {
            std::ostringstream oss;
            BOOST_FOREACH(const T& s, t) { oss << s << " "; }
            return oss.str();
        }

        int indent;
    };

    struct config_item_printer : boost::static_visitor<>
    {
        config_item_printer(int indent = 0)
          : indent(indent)
        {
        }

        void operator()(config_struct const& conf) const
        {
            config_printer(indent+tabsize)(conf);
        }

        void operator()(config_key_value_pair const& pair) const
        {
            tab(indent+tabsize);
            std::cout << "( "<< pair.first 
                      << ": " << boost::apply_visitor(config_value_to_string(), pair.second)
                      << " )" << std::endl;
        }

        int indent;
    };

    void config_printer::operator()(config_struct const& conf) const
    {
        tab(indent);
        std::cout << "Section: " << conf.name << std::endl;
        tab(indent);
        std::cout << '{' << std::endl;

        BOOST_FOREACH(config_item const& it, conf.children)
        {
            boost::apply_visitor(config_item_printer(indent), it);
        }

        tab(indent);
        std::cout << '}' << std::endl;
    }

    // Define a skipper for white space and comments
    template<typename Iterator>
    struct config_skipper
      : qi::grammar<Iterator> 
    {
        config_skipper() 
          : config_skipper::base_type(skip, "white space and comments") 
        {
            using qi::eol;
            using ascii::char_;

            skip = ( ascii::space )
                 | ( "//" >> *(char_ - eol) >> eol )
            ;
        }
        qi::rule<Iterator> skip;
    };
    

    ///////////////////////////////////////////////////////////////////////////
    //  Our config grammar definition
    ///////////////////////////////////////////////////////////////////////////
    template <typename Iterator, typename Skipper = config_skipper<Iterator> >
    struct config_grammar
      : qi::grammar<Iterator, config_struct(), qi::locals<std::string>, Skipper>
    {
        config_grammar()
          : config_grammar::base_type(config, "config")
        {
            using qi::lit;
            using qi::lexeme;
            using qi::on_error;
            using qi::fail;
            using qi::accept;
            using qi::double_;
            using qi::int_;
            using ascii::char_;
            using ascii::alnum;
            using ascii::string;
            using namespace qi::labels;

            using phoenix::construct;
            using phoenix::val;

            item %= key_value_pair | config;

            start_tag %=
                    lexeme[+(alnum | '_')]
                >> !lit('=')
                >   lit(':') 
                >   lit('{')
            ;

            key_value_pair %=
                    lexeme[+(alnum | '_')]
                >> !lit(':')
                >   lit('=')
                >   ( quoted_string | double_ | int_ | quoted_string_list | double_list | int_list )
                >   lit(';')
            ;

            end_tag =
                    lit('}') 
                >   lit(';')
            ;

            config %=
                    start_tag[_a = _1]
                >  *item
                >   end_tag(_a)
            ;

            quoted_string %=
                    '"' > qi::no_skip[*alnum] > '"'
            ;

            quoted_string_list %=
                    lit('(')
                >>   quoted_string % ','
                >   lit(')')
            ;

            double_list %=
                    lit('(')
                >>   double_ % ','
                >   lit(')')
            ;

            int_list %=
                    lit('(')
                >>   int_ % ','
                >   lit(')')
            ;

            config.name("config");
            item.name("item");
            key_value_pair.name("key_value_pair");
            quoted_string.name("quoted_string");
            quoted_string_list.name("quoted_string_list");
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
        }

        qi::rule<Iterator, config_struct(), qi::locals<std::string>, Skipper> config;
        qi::rule<Iterator, config_item(), Skipper> item;
        qi::rule<Iterator, config_key_value_pair(), Skipper> key_value_pair;
        qi::rule<Iterator, std::string(), Skipper> quoted_string;
        qi::rule<Iterator, std::vector<std::string>(), Skipper> quoted_string_list;
        qi::rule<Iterator, std::vector<double>(), Skipper> double_list;
        qi::rule<Iterator, std::vector<int>(), Skipper> int_list;
        qi::rule<Iterator, std::string(), Skipper> start_tag;
        qi::rule<Iterator, void(std::string), Skipper> end_tag;
    };
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
        return 1;
    }

    std::ifstream in(filename, std::ios_base::in);

    if (!in)
    {
        std::cerr << "Error: Could not open input file: "
            << filename << std::endl;
        return 1;
    }

    std::string storage; // We will read the contents here.
    in.unsetf(std::ios::skipws); // No white space skipping!
    std::copy(
        std::istream_iterator<char>(in),
        std::istream_iterator<char>(),
        std::back_inserter(storage));

    typedef parse::config_grammar<std::string::const_iterator> config_grammar;
    typedef parse::config_skipper<std::string::const_iterator> config_skipper;
    config_grammar cg; // Our grammar
    config_skipper skipper; // Our skipper
    parse::config_struct configuration; // Our tree

    std::string::const_iterator iter = storage.begin();
    std::string::const_iterator end = storage.end();
    bool r = phrase_parse(iter, end, cg, skipper, configuration);

    if (r && iter == end)
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing succeeded\n";
        std::cout << "-------------------------\n";
        parse::config_printer()(configuration);
    
        return 0;
    }
    else
    {
        std::cout << "-------------------------\n";
        std::cout << "Parsing failed\n";
        std::cout << "-------------------------\n";
        parse::config_printer()(configuration);
        return 1;
    }

}


