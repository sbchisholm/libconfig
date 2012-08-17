#ifndef _libconfig_types_included_
#define _libconfig_types_included_

#define BOOST_VARIANT_NO_FULL_RECURSIVE_VARIANT_SUPPORT

#include <map>
#include <string>
#include <vector>

#include <boost/none.hpp>
#include <boost/variant/recursive_variant.hpp>
#include <boost/variant/get.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/foreach.hpp>

namespace libconfig {

  template<typename, typename> class map;
  
  // ==========================================================================
  // Configuration types
 
  // The main configuration type is a map:
  //   ConfigKey ->  std::string
  //             or   double
  //             or   int
  //             or   bool
  //             or   std::vector<std::string>
  //             or   std::vector<double>
  //             or   std::vector<int>
  //             or   std::vector<boost::none_t>
  //             or   map<ConfigKey, ConfigTree>

  typedef std::string ConfigKey;
  typedef 
      boost::make_recursive_variant<
          std::string
        , double
        , int
        , bool
        , std::vector<std::string>
        , std::vector<double>
        , std::vector<int>
        , std::vector<boost::none_t>
        , map<ConfigKey, boost::recursive_variant_>
      >::type 
  ConfigTree;

  typedef map<ConfigKey, ConfigTree> ConfigType;
  typedef std::pair<ConfigKey, ConfigTree> ConfigPair;
  
  
  // ==========================================================================
  // libconfig::map 
  //     A map class to overide the functionality of the insert function.
  template<>
  class map<ConfigKey, ConfigTree> : public std::map<ConfigKey, ConfigTree> 
  {
    private:
      // :: -------------------------------------------------------------------
      // :: Private Types

      typedef std::map<ConfigKey, ConfigTree> base;

    public:
      // :: -------------------------------------------------------------------
      // :: Public Types
      
      typedef typename base::iterator iterator;

    private:
      // :: -------------------------------------------------------------------
      // :: boost::variant visitor functor that inserts a new value into the
      // :: map.  This will overwrite a value if it already exists or if the
      // :: thing to insert is a section the section will be either inserted if
      // :: it is a new section or merged with an existing section.
       
      struct map_inserter : boost::static_visitor<>
      {
        public:
          // :: ---------------------------------------------------------------
          // :: Construction

          map_inserter(map<ConfigKey, ConfigTree> *pointer, 
                       ConfigKey const& key) 
            : m_map(pointer) 
            , m_key(key)
          {}

        public:
          // :: ---------------------------------------------------------------
          // :: Public Interface operator()

          // Insert a section, if the section is not found insert the new
          // section, if the section is already there merge the new section with
          // the existing one.
          void operator()(ConfigType const& t) const
          {
            std::map<ConfigKey, ConfigTree>::iterator it = m_map->find(m_key);
            if(it == m_map->end()) {
              m_map->insert(std::make_pair(m_key, t));
            }
            else {
              ConfigType& node = boost::get<ConfigType>(it->second);
              BOOST_FOREACH(ConfigType::value_type value, t) {
                node.insert(node.begin(), value);
              }
            }
          }

          // Inserting a value, either inset the new value or if the value is
          // already there, overwrite the existing one.
          template<typename T> 
          void operator()(T const& t) const
          {
            (*m_map)[m_key] = t;
          }

        private:
          // :: ---------------------------------------------------------------
          // :: Members

          map<ConfigKey, ConfigTree> *m_map;
          ConfigKey m_key;
      };

    public:
      // :: -------------------------------------------------------------------
      // :: Construction - Provide access to the std::map's constructors

      map() 
        : base() 
      { }

      map(const map& other) 
        : base(other) 
      { }

      template<class InputIt> map(InputIt first, InputIt last) 
        : base(first, last) 
      { }

    public:
      // :: -------------------------------------------------------------------
      // :: Overwrite the Insert functions

      // This is the insert function that boost phoenix uses to insert values
      // into a map.
      typename base::iterator 
      insert(typename base::iterator hint, 
             const typename base::value_type& value)
      {
        boost::apply_visitor(map_inserter(this, value.first), value.second);
        return hint;
      }
        
      std::pair<typename base::iterator, bool> insert( const typename base::value_type& value ) 
      { return base::insert(value); }
      template<class InputIt> void insert( InputIt first, InputIt last ) 
      { base::insert(first, last); }
  };


} // namespace libconfig

#endif // _libconfig_included_
