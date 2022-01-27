//+*****************************************************************************
//                         Honeywell Proprietary
// This document and all information and expression contained herein are the
// property of Honeywell International Inc., are loaned in confidence, and
// may not, in whole or in part, be used, duplicated or disclosed for any
// purpose without prior written permission of Honeywell International Inc.
//               This document is an unpublished work.
//
// Copyright (C) 2021 Honeywell International Inc. All rights reserved.
//+*****************************************************************************
/* 
 * File:   XMLSupport.h
 * Author: Tomas Kratochvila <tomas.kratochvila at honeywell.com>
 * Author: Petr Bauch <petr.bauch at honeywell.com>
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <iostream>

#include "bbb.h"

namespace XMLSupport {

using namespace Basics;
using Index = int;
using Indices = std::vector<Index>;

class MalformedXMLException : public std::runtime_error {
public:
    MalformedXMLException(const std::string& msg) : runtime_error(msg) {}
};

inline void assert_xml(bool cond, const std::string& msg="Malformed XML") {
  if (!(cond))
    throw MalformedXMLException(msg);
}

/**
 * Print the string (con) consecutively n-times to the stream (s).
 */
template<typename Stream>
static void print_n_times(Stream& s, int n, const String& con) {
  for (int i = 0; i < n; i++)
    s << con;
}

/**
 * Increase the given character position (pos) pointing to the given string (in)
 * to the position of the first non-whitespace character in the string.
 */
static inline void skip_whites(const String& in, SSize& pos) {
  while (std::isspace(in[pos])) pos++;
}

/*
 * The following structures represent components of the XML
 * Each implements constructor(s) .. more than one if the structure has more forms
 * and a print function, taking a reference to the XML and a stream to print into
 */

/// Represents a component of XML
struct IName {
  Index domain;
  Index name;

  IName(Index d, Index n) : domain(d), name(n) {}

  template<typename Con, typename Stream>
  /// Print function. Takes a reference to the XML and a stream to print into.
  void print(const Con& xml, Stream& s) const {
    if (domain != -1) {
      s << xml.tokens[domain] << ":";
    }
    s << xml.tokens[name];
  }
};
using INames = std::vector<IName>;

/// Represents a component of XML
struct Parameter {
  Index domain;
  Index name;
  Index value;

  Parameter(Index d, Index n, Index v) : domain(d), name(n), value(v) {}

  template<typename Con, typename Stream>
  /// Print function. Takes a reference to the XML and a stream to print into.
  void print(const Con& xml, Stream& s) const {
    if (domain == -1)
      s << xml.tokens[name] << "=\"" << xml.tokens[value] << "\"";
    else
      s << xml.tokens[domain] << ":" << xml.tokens[name] << "=\""
        << xml.tokens[value] << "\"";
  }
};
using Parameters = std::vector<Parameter>;

/**
 * Sub is either a list of items or a value
 * The items or the value are intended as indexes of XML structures.
 */
struct Sub {
  Indices sItems;
  Index value;

  Sub(const Indices& i) : sItems(i), value(-1) {}
  Sub(Index v) : value(v) {}

/**
 * Print the sub-structures of the given XML (xml),
 * selected by the Sub to the given stream (s),
 * each structure is indented by given number (indent) of tabulators. 
 */
  template<typename Con, typename Stream>
  /// Print function. Takes a reference to the XML, a stream to print into, and indentation.
  void print(const Con& xml, Stream& s, int indent) const {
    if (sItems.empty()) {
      print_n_times(s, indent, "\t");
      s << xml.tokens[value];
    }
    else {
      for (auto& i : sItems) {
        s << "\n";
        xml.items[i].print(xml, s, indent);
      }
    }
  }
};
using Subs = std::vector<Sub>;

/**
 * Item always has a name and list of parameters (can be empty)
 * it may have a sub-item
 */
struct Item {
  Index name;
  Indices params;
  Index sub;

  Item(Index n, const Indices& p, Index s) : name(n), params(p), sub(s) {}
  Item(Index n, const Indices& p) : name(n), params(p), sub(-1) {}

/**
 * Print XML element (also its attributes and content).
 */
  template<typename Con, typename Stream>
  /// Print function. Takes a reference to the XML, a stream to print into, and indentation.
  void print(const Con& xml, Stream& s, int indent) const {
    print_n_times(s, indent, "\t");
    s << "<";
    xml.iNames[name].print(xml, s);
    if (sub == -1) {//there are no sub-items
      for (auto &p : params) {
        s << " ";
        xml.parameters[p].print(xml, s);
      }
      s << "/>";
    }
    else {
      for (auto &p : params) {
        s << "\n";
        print_n_times(s, indent + 1, "\t");
        xml.parameters[p].print(xml, s);
      }
      s << ">";
      xml.subs[sub].print(xml, s, indent + 1);
      s << "\n";
      print_n_times(s, indent, "\t");
      s << "</";
      xml.iNames[name].print(xml, s);
      s << ">";
    }
  }
};
using Items = std::vector<Item>;

/**
 * Xml is an item
 * all strings are stored in *tokens*
 * all xml elements are referenced as indexes into particular vectors
 * *top* is the index of the xml item
 */
struct Xml {
  //--------------------Data--------------------
  std::map<String, Index> knownTokens;
  Strings tokens;
  INames iNames;
  Parameters parameters;
  Items items;
  Subs subs;

  Index top = 0;
  
  /**
   * Reverts the Xml object into a clean state (as if just constructed)
   */
  void clear();

  //--------------------Parser functions--------------------
  /**
   * Each element has its parser function with the following parameters:
   * *in* input string to read from
   * *pos* reference to position in the string where element should start from
   * reference to index(es) of the newly created element(s)
   *
   * returns true if parsing succeeded
   */
  bool read_single_char(const String& in, SSize& pos, char c);
  bool read_exact_string(const String& in, SSize& pos, const String& s);
  bool read_value_as_item(const String& in, SSize& pos, Index& value);
  bool read_value(const String& in, SSize& pos, Index& value);
  bool read_iname(const String& in, SSize& pos, Index& iName);
  bool read_param(const String& in, SSize& pos, Index& param);
  bool read_params(const String& in, SSize& pos, Indices& params);
  bool read_sub(const String& in, SSize& pos, Index& sub);
  bool read_items(const String& in, SSize& pos, Indices& sItems);
  bool read_item(const String& in, SSize& pos, Index& item);
  bool read_xml_line(const String& in, SSize& pos);

  Xml() {}

  /**
   * Function to build a performance OSLC rdf:description element using the helper ii, ie, .. methods
   */
  Index perf_item(const String& local_address,
                  const String& title,
                  const String& metric,
                  const String& unit,
                  const String& type,
                        Index&  value) {
    return
      ii("rdf", "description",
         { ip("rdf", "about", local_address + " " + title) },
         isi({ ii("rdf", "type",
                  { ip("rdf", "resource",  "http://open-services.net/ns/ems#Measure") }),
               ii("dcterms", "title",
                  {},
                  isv(title)),
               ii("ems", "metric",
                  { ip("rdf", "resource",  metric) }),
               ii("ems", "unitOfMeasure",
                  { ip("rdf", "resource",  unit) }),
               ii("ems", "numericValue",
                  { ip( "rdf", "datatype", type) },
                  (value = isv("")))
            }));
  }

/** Insert XML item with the given name (domain d and name n), parameters (ps), and subitem (s). */
  Index ii(const String& d, const String& n, const Indices& ps, Index s) {
    return insert_item(insert_iname(insert_token(d), insert_token(n)), ps, s);
  }

/** Insert XML item with the given name (domain d and name n) and parameters (ps). */
  Index ii(const String& d, const String& n, const Indices& ps) {
    return insert_item(insert_iname(insert_token(d), insert_token(n)), ps);
  }

/** Insert parameter with the given domain d, name n and value v. */
  Index ip(const String& d, const String& n, const String& v) {
    return insert_parameter(insert_token(d), insert_token(n), insert_token(v));
  }

/** Insert subitem consisting of given list of items. */
  Index isi(const Indices& s) {
    return insert_sub(s);
  }

/** Insert subitem consisting of a given value. */
  Index isv(const String& v) {
    return insert_sub(insert_token(v));
  }

/**
 * Construct a new parameter (from the arguments of its constructor)
 * and add it to the other parameters.
 */
  Index insert_parameter(Index d, Index n, Index v) {
    parameters.emplace_back(d, n, v);
    return parameters.size() - 1;
  }

/**
 * Construct a new IName (from the arguments of its constructor)
 * and add it to the other INames.
 */
  Index insert_iname(Index d, Index n) {
    iNames.emplace_back(d, n);
    return iNames.size() - 1;
  }

/**
 * Construct a new Item (name + parameters + subitem?) (from the arguments of its constuctor)
 * and add it to the other Items.
 */
  template<class... Args>
  Index insert_item(Args&&... args) {
    items.emplace_back(args...);
    return items.size() - 1;
  }

/**
 * Construct a new Sub (from the arguments of its constructor)
 * and add it to the other Subs.
 */
  template<class... Args>
  Index insert_sub(Args&&... args) {
    subs.emplace_back(args...);
    return subs.size() - 1;
  }

/**
 * If the string is known already, return its index in the tokens vector.
 * If the string is not known yet, add it to the tokens and extend the map of knownTokens.
 * Return the index of the token.
 */
  Index insert_token(const String& s) {
    if (knownTokens.count(s) == 0) {
      knownTokens[s] = tokens.size();
      tokens.push_back(s);
    }
    return knownTokens[s];
  }

  Xml(const String& s) {
    SSize begin = 0;
    assert_xml(read_xml_line(s, begin));
    assert_xml(read_item(s, begin, top));
    skip_whites(s, begin);
    assert_xml(begin == s.size());
  }

  void fill(const String& s) {
    SSize begin = 0;
    assert_xml(read_xml_line(s, begin));
    std::cout << "Foo: " << begin << std::endl;
    assert_xml(read_item(s, begin, top));
    skip_whites(s, begin);
    //assert_xml(begin == s.size());
  }

  template<typename Stream>
  /// Print the XML. Takes a stream to print into.
  void print(Stream& s) const {
    items[top].print(*this, s, 0);
    s << "\n";
  }

  String item_name(Index item_id) const {
    return tokens[iNames[items[item_id].name].name];
  }
  String parameter_name(Index param_id) const {
    return tokens[parameters[param_id].name];
  }
  Index find_item(const String& iName) const;
  void find_items(const String& iName, Indices& finds) const;
  Index find_param(const Indices & params, const String & pName) const;
  String find_param_value_string(const Indices & params, const String & pName) const;
  bool find_param_value_bool(const Indices & params, const String & pName) const;
  void find_params(const Indices& params, const String& pName, Indices& res) const;
  void get_params(const Index item, Indices & res) const;
  String get_param_value(Index i) const { return tokens[parameters[i].value]; }
  void get_param_item(const String& iName, const String& pName, Strings& res) const;
  void get_subitems(const Index itemId, Indices& res) const;
  void get_value_after(Index presub_id, Strings& res) const;
  void get_values_after(const String& value_name, Strings& res) const;
  //--------------------Helper functions--------------------
  String token(Index i) { return tokens[i]; }
  
/** In the string (in), read the first non-white characters from the position (pos)
    as long as the (predicate) holds for the character.
    If the read substring is (or can be turned into) a token, the index (domain) contains
    the unique index of the token and the boolean value true is returned.
    Otherwise false is returened.
*/
  template<typename Pred>
  bool read_string_if(const String& in, SSize& pos, Pred predicate, Index& domain) {
    skip_whites(in, pos);
    SSize i;
    for (i = pos; predicate(in[i]); i++) ;
    if (i == pos)
      return false;
    domain = tokenise(String(in, pos, i - pos));
    pos = i;
    return true;
  }

/** In the string (in) from the position (start) find the index of the first character,
    for which the predicate is true.
    (Find first character for which / IF the predicate is true.)
    Return this index.
*/
  template<typename Pre>
  SSize find_first_if(const String& in, SSize start, Pre predicate) {
    SSize i;
    for (i = start; i < in.size() && !predicate(in, i); i++) ;
    return i;
  }

  Index tokenise(String s);
  static bool is_domain_char(char c) { return std::isalnum(c) || c == '_'; }
};

}
 
